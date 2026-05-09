#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "esp_log.h"

#define I2C_MASTER_SCL          GPIO_NUM_22
#define I2C_MASTER_SDA          GPIO_NUM_21
#define I2C_MASTER_NUM          I2C_NUM_0
#define I2C_MASTER_FREQ_HZ      100000
#define I2C_MASTER_TIMEOUT_MS   1000


#define MPU6050_ADDR            0x68
#define MPU6050_WHO_AM_I_ADDR   0x75
#define MPU6050_PWR_MGMT_1      0x6B
#define MPU6050_ACCEL_X_ADDR    0x3B

#define MPU6050_GYRO_CONFIG     0x1B
#define MPU6050_ACCEL_CONFIG    0x1C
#define MPU6050_DLPF_CONFIG     0x1A
#define MPU6050_SMPRT_DIV       0x19

static esp_err_t i2c_master_init(i2c_master_bus_handle_t* bus_handle, i2c_master_dev_handle_t* dev_handle){
    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_MASTER_NUM,
        .sda_io_num = I2C_MASTER_SDA,
        .scl_io_num = I2C_MASTER_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .intr_priority = 0,
        .flags.enable_internal_pullup = true,
    };

    esp_err_t esp_err = i2c_new_master_bus(&bus_config, bus_handle);

    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = MPU6050_ADDR,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };

    esp_err = i2c_master_bus_add_device (*bus_handle, &dev_config, dev_handle);
    return esp_err; 
}

static esp_err_t mpu6050_read(i2c_master_dev_handle_t dev_handle, uint8_t reg_addr, uint8_t *data, size_t len)
{
    return i2c_master_transmit_receive(dev_handle, &reg_addr, 1, data, len, I2C_MASTER_TIMEOUT_MS);
}


static esp_err_t mpu6050_write_byte(i2c_master_dev_handle_t dev_handle, uint8_t reg_addr, uint8_t data)
{
    uint8_t write_buf[2] = {reg_addr, data};
    return i2c_master_transmit(dev_handle, write_buf, sizeof(write_buf), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}


//Kalibracja - obliczanie wartosci przyspieszenia gdy urzadzenie lezy nieruchomo (obliczanie sredniej w celu okreslenia wartosci poprawki)
void calibrate_mpu6050(i2c_master_dev_handle_t dev_handle, float offset_acc[], float offset_gyro[]){
    int32_t sum_acc[3] = {0};
    int32_t sum_gyro[3] = {0};
    const int samples = 500;
    uint8_t read_data[14];

    for(int i=0; i < samples; i++){
        ESP_ERROR_CHECK(mpu6050_read(dev_handle, MPU6050_ACCEL_X_ADDR, read_data, 14));
        int16_t ax = (read_data[0] << 8) | read_data[1];
        int16_t ay = (read_data[2] << 8) | read_data[3];
        int16_t az = (read_data[4] << 8) | read_data[5];

        int16_t gx = (read_data[8] << 8) | read_data[9];
        int16_t gy = (read_data[10] << 8) | read_data[11];
        int16_t gz = (read_data[12] << 8) | read_data[13];

        sum_acc[0] += ax;
        sum_acc[1] += ay;
        sum_acc[2] += az;

        sum_gyro[0] += gx;
        sum_gyro[1] += gy;
        sum_gyro[2] += gz;
        vTaskDelay(pdMS_TO_TICKS(2));
    }
    offset_acc[0] = (float)(sum_acc[0] / samples) / 4096.0;
    offset_acc[1] = (float)(sum_acc[1] / samples) / 4096.0;
    offset_acc[2] = ((float)(sum_acc[2] / samples) / 4096.0) - 1.0;

    offset_gyro[0] = (float)(sum_gyro[0] / samples) / 32.8;
    offset_gyro[1] = (float)(sum_gyro[1] / samples) / 32.8;
    offset_gyro[2] = (float)(sum_gyro[2] / samples) / 32.8;
}

void app_main(void)
{
    vTaskDelay(pdMS_TO_TICKS(2000));

    uint8_t data[2];
    i2c_master_bus_handle_t bus_handle;
    i2c_master_dev_handle_t dev_handle;

    if(i2c_master_init(&bus_handle, &dev_handle) == ESP_OK){
        printf("ESP_OK \n");
    }else{
        printf("ERROR! \n");
        return;
    }


    esp_err_t err = mpu6050_read(dev_handle, MPU6050_WHO_AM_I_ADDR, data, 1);
    if(err == ESP_OK){
        ESP_LOGI("MPU", "WHO_AM_I = 0x%02X", data[0]);
    }else{
        ESP_LOGE("MPU", "WHO_AM_I error: %s", esp_err_to_name(err));
    }

    ESP_ERROR_CHECK(mpu6050_write_byte(dev_handle, MPU6050_PWR_MGMT_1, 0x01));

    //+-1000 deg/s
    ESP_ERROR_CHECK(mpu6050_write_byte(dev_handle, MPU6050_GYRO_CONFIG , 0x10));

    //+-8g
    ESP_ERROR_CHECK(mpu6050_write_byte(dev_handle, MPU6050_ACCEL_CONFIG , 0x10));

    //konfiguracja filtra sprzetowego
    //ustawienie DLPF na 42Hz
    ESP_ERROR_CHECK(mpu6050_write_byte(dev_handle, MPU6050_DLPF_CONFIG, 0x03));

    //Ustalamy stala czestotliwosc probkowania 20ms = 50Hz
    ESP_ERROR_CHECK(mpu6050_write_byte(dev_handle, MPU6050_SMPRT_DIV, 19));
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(20);

    float offset_acc[3] = {0};
    float offset_gyro[3] = {0};
    calibrate_mpu6050(dev_handle, offset_acc, offset_gyro);

    uint8_t read_data[14];
    while (1)
    {
        ESP_ERROR_CHECK(mpu6050_read(dev_handle, MPU6050_ACCEL_X_ADDR, read_data, 14));
        int16_t ax = (read_data[0] << 8) | read_data[1];
        int16_t ay = (read_data[2] << 8) | read_data[3];
        int16_t az = (read_data[4] << 8) | read_data[5];

        int16_t gx = (read_data[8] << 8) | read_data[9];
        int16_t gy = (read_data[10] << 8) | read_data[11];
        int16_t gz = (read_data[12] << 8) | read_data[13];
        
        uint32_t current_time_ms = pdTICKS_TO_MS(xTaskGetTickCount());

        printf("%lu,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f\n",
            current_time_ms, 
            ((float)ax/4096.0) - offset_acc[0], 
            ((float)ay/4096.0) - offset_acc[1], 
            ((float)az/4096.0) - offset_acc[2],
            ((float)gx/32.8) - offset_gyro[0], 
            ((float)gy/32.8) - offset_gyro[1], 
            ((float)gz/32.8) - offset_gyro[2]);
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}