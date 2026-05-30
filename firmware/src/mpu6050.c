#include "mpu6050.h"
#include "config.h"
#include "mqtt.h"
#include "mqtt_client.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "esp_spp_api.h"
#include "bluetooth.h"
#include "driver/gpio.h"

static i2c_master_bus_handle_t bus_handle;
static i2c_master_dev_handle_t dev_handle;

static esp_err_t i2c_master_init(void){
    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_MASTER_NUM,
        .sda_io_num = I2C_MASTER_SDA,
        .scl_io_num = I2C_MASTER_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .intr_priority = 0,
        .flags.enable_internal_pullup = true,
    };

    esp_err_t esp_err = i2c_new_master_bus(&bus_config, &bus_handle);

    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = MPU6050_ADDR,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };

    esp_err = i2c_master_bus_add_device (bus_handle, &dev_config, &dev_handle);
    return esp_err; 
}

static esp_err_t mpu6050_read(uint8_t reg_addr, uint8_t *data, size_t len)
{
    return i2c_master_transmit_receive(dev_handle, &reg_addr, 1, data, len, I2C_MASTER_TIMEOUT_MS);
}

static esp_err_t mpu6050_write_byte(uint8_t reg_addr, uint8_t data)
{
    uint8_t write_buf[2] = {reg_addr, data};
    return i2c_master_transmit(dev_handle, write_buf, sizeof(write_buf), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}

//Kalibracja - obliczanie wartosci przyspieszenia gdy urzadzenie lezy nieruchomo (obliczanie sredniej w celu okreslenia wartosci poprawki)
void calibrate_mpu6050(int16_t offset_acc[], int16_t offset_gyro[]){
    int32_t sum_acc[3] = {0};
    int32_t sum_gyro[3] = {0};
    const int samples = 500;
    uint8_t read_data[14];

    for(int i=0; i < samples; i++){
        ESP_ERROR_CHECK(mpu6050_read(MPU6050_ACCEL_X_ADDR, read_data, 14));
        sum_acc[0] += (int16_t)((read_data[0] << 8) | read_data[1]);
        sum_acc[1] += (int16_t)((read_data[2] << 8) | read_data[3]);
        sum_acc[2] += (int16_t)((read_data[4] << 8) | read_data[5]);

        sum_gyro[0] += (int16_t)((read_data[8] << 8) | read_data[9]);
        sum_gyro[1] += (int16_t)((read_data[10] << 8) | read_data[11]);
        sum_gyro[2] += (int16_t)((read_data[12] << 8) | read_data[13]);
        vTaskDelay(pdMS_TO_TICKS(2));
    }
    offset_acc[0] = (int16_t)(sum_acc[0] / samples);
    offset_acc[1] = (int16_t)(sum_acc[1] / samples);
    offset_acc[2] = (int16_t)((sum_acc[2] / samples) - 4096);

    offset_gyro[0] = (int16_t)(sum_gyro[0] / samples);
    offset_gyro[1] = (int16_t)(sum_gyro[1] / samples);
    offset_gyro[2] = (int16_t)(sum_gyro[2] / samples);
}

void mpu_data_task(void *pvParameters){
    ESP_ERROR_CHECK(mpu6050_write_byte(MPU6050_PWR_MGMT_1, 0x01));
    //+-1000 deg/s
    ESP_ERROR_CHECK(mpu6050_write_byte(MPU6050_GYRO_CONFIG , 0x10));
    //+-8g
    ESP_ERROR_CHECK(mpu6050_write_byte(MPU6050_ACCEL_CONFIG , 0x10));
    //konfiguracja filtra sprzetowego
    //ustawienie DLPF na 42Hz
    ESP_ERROR_CHECK(mpu6050_write_byte(MPU6050_DLPF_CONFIG, 0x03));
    //Ustalamy stala czestotliwosc probkowania 20ms = 50Hz
    ESP_ERROR_CHECK(mpu6050_write_byte(MPU6050_SMPRT_DIV, 19));

    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(20);

    int16_t offset_acc[3] = {0};
    int16_t offset_gyro[3] = {0};
    calibrate_mpu6050(offset_acc, offset_gyro);

    uint8_t read_data[14];
    uint8_t tx_buffer[16];

    static bool stop_sent = false;
    
    while (1)
    {
        if (gpio_get_level(GPIO_NUM_0) == 0) {
            if (!stop_sent) {
                mqtt_send_stop_signal(); 
                stop_sent = true;
            }
        } else {
            stop_sent = false;
        }

        if (mpu6050_read(MPU6050_ACCEL_X_ADDR, read_data, 14) == ESP_OK){
            int16_t raw_ax = (int16_t)((read_data[0] << 8) | read_data[1]);
            int16_t raw_ay = (int16_t)((read_data[2] << 8) | read_data[3]);
            int16_t raw_az = (int16_t)((read_data[4] << 8) | read_data[5]);

            int16_t raw_gx = (int16_t)((read_data[8] << 8) | read_data[9]);
            int16_t raw_gy = (int16_t)((read_data[10] << 8) | read_data[11]);
            int16_t raw_gz = (int16_t)((read_data[12] << 8) | read_data[13]);

            int32_t calc_ax = (int32_t)raw_ax - offset_acc[0];
            int32_t calc_ay = (int32_t)raw_ay - offset_acc[1];
            int32_t calc_az = (int32_t)raw_az - offset_acc[2];

            int32_t calc_gx = (int32_t)raw_gx - offset_gyro[0];
            int32_t calc_gy = (int32_t)raw_gy - offset_gyro[1];
            int32_t calc_gz = (int32_t)raw_gz - offset_gyro[2];

            if (calc_ax > 32767) calc_ax = 32767; else if (calc_ax < -32768) calc_ax = -32768;
            if (calc_ay > 32767) calc_ay = 32767; else if (calc_ay < -32768) calc_ay = -32768;
            if (calc_az > 32767) calc_az = 32767; else if (calc_az < -32768) calc_az = -32768;

            if (calc_gx > 32767) calc_gx = 32767; else if (calc_gx < -32768) calc_gx = -32768;
            if (calc_gy > 32767) calc_gy = 32767; else if (calc_gy < -32768) calc_gy = -32768;
            if (calc_gz > 32767) calc_gz = 32767; else if (calc_gz < -32768) calc_gz = -32768;

            imu_data_t imu_data = {
                .timestamp = pdTICKS_TO_MS(xTaskGetTickCount()),
                .ax = (int16_t)calc_ax,
                .ay = (int16_t)calc_ay,
                .az = (int16_t)calc_az,
                .gx = (int16_t)calc_gx,
                .gy = (int16_t)calc_gy,
                .gz = (int16_t)calc_gz,
            };

            memcpy(tx_buffer, &imu_data, sizeof(imu_data_t));

            if(current_mode == MODE_BLUETOOTH){
                if (bluetooth_is_ready()) {
                    esp_spp_write(bluetooth_get_handle(), 16, tx_buffer);
                }
            }else if(current_mode == MODE_WIFI){
                mqtt_publish(&imu_data);
            }

        }
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }

}

void mpu6050_init(void){
        uint8_t data[2];
    if(i2c_master_init() == ESP_OK){
        printf("ESP_OK \n");
    }else{
        printf("ERROR! \n");
        return;
    }

    esp_err_t err = mpu6050_read(MPU6050_WHO_AM_I_ADDR, data, 1);
    if(err == ESP_OK){
        ESP_LOGI("MPU", "WHO_AM_I = 0x%02X", data[0]);
    }else{
        ESP_LOGE("MPU", "WHO_AM_I error: %s", esp_err_to_name(err));
    }
}
