#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"
#include "esp_bt_device.h"
#include "esp_spp_api.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define SPP_TAG "SPP_ACCEPTOR"
#define DEVICE_NAME "ESP32_MPU_DATA"
#define SPP_SERVER_NAME "MPU_SERVER"

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

static bool is_congested = false;
static uint32_t current_spp_handle = 0;
i2c_master_bus_handle_t bus_handle;
i2c_master_dev_handle_t dev_handle;

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

static void esp_spp_cb(esp_spp_cb_event_t event, esp_spp_cb_param_t *param){
    switch (event) {
    case ESP_SPP_INIT_EVT:
        if (param->init.status == ESP_SPP_SUCCESS) {
            ESP_LOGI(SPP_TAG, "ESP_SPP_INIT_EVT");
            esp_spp_start_srv(ESP_SPP_SEC_AUTHENTICATE, ESP_SPP_ROLE_SLAVE, 0, SPP_SERVER_NAME);
        } else {
            ESP_LOGE(SPP_TAG, "ESP_SPP_INIT_EVT status:%d", param->init.status);
        }
        break;
    case ESP_SPP_CLOSE_EVT:
        ESP_LOGI(SPP_TAG, "ESP_SPP_CLOSE_EVT status:%d handle:%"PRIu32" close_by_remote:%d", param->close.status,
                 param->close.handle, param->close.async);
        current_spp_handle = 0;
        break;
    case ESP_SPP_START_EVT:
        if (param->start.status == ESP_SPP_SUCCESS) {
            ESP_LOGI(SPP_TAG, "ESP_SPP_START_EVT handle:%"PRIu32" sec_id:%d scn:%d", param->start.handle, param->start.sec_id,
                     param->start.scn);
            esp_bt_gap_set_device_name(DEVICE_NAME);
            esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
        } else {
            ESP_LOGE(SPP_TAG, "ESP_SPP_START_EVT status:%d", param->start.status);
        }
        break;
    case ESP_SPP_CONG_EVT:
        ESP_LOGI(SPP_TAG, "ESP_SPP_CONG_EVT");
        is_congested = param->cong.cong;
        break;
    case ESP_SPP_WRITE_EVT:
        break;
    case ESP_SPP_SRV_OPEN_EVT:
        ESP_LOGI(SPP_TAG, "ESP_SPP_SRV_OPEN_EVT status:%d handle:%"PRIu32"", param->srv_open.status, param->srv_open.handle);
        current_spp_handle = param->srv_open.handle;
        break;
    default:
        break;
    }
}

void esp_bt_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param) {
    if (event == ESP_BT_GAP_PIN_REQ_EVT) {
        esp_bt_pin_code_t pin_code = {'1', '2', '3', '4'};
        esp_bt_gap_pin_reply(param->pin_req.bda, true, 4, pin_code);
    }
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
    ESP_ERROR_CHECK(mpu6050_write_byte(MPU6050_SMPRT_DIV, 9));

    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(10);

    int16_t offset_acc[3] = {0};
    int16_t offset_gyro[3] = {0};
    calibrate_mpu6050(offset_acc, offset_gyro);

    uint8_t read_data[14];
    uint8_t tx_buffer[16];
    
    while (1)
    {
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

            int16_t ax = (int16_t)calc_ax;
            int16_t ay = (int16_t)calc_ay;
            int16_t az = (int16_t)calc_az;
            int16_t gx = (int16_t)calc_gx;
            int16_t gy = (int16_t)calc_gy;
            int16_t gz = (int16_t)calc_gz;
            
            uint32_t current_time_ms = pdTICKS_TO_MS(xTaskGetTickCount());
            
            memcpy(tx_buffer,    &current_time_ms, 4);
            memcpy(tx_buffer+4,  &ax, 2);
            memcpy(tx_buffer+6,  &ay, 2);
            memcpy(tx_buffer+8,  &az, 2);
            memcpy(tx_buffer+10, &gx, 2);
            memcpy(tx_buffer+12, &gy, 2);
            memcpy(tx_buffer+14, &gz, 2);

            if (current_spp_handle != 0 && !is_congested) {
                esp_spp_write(current_spp_handle, 16, tx_buffer);
            }
        }
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }

}

void app_main(void)
{
    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );

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

    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_BLE));
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT));

    esp_bluedroid_config_t bluedroid_cfg = BT_BLUEDROID_INIT_CONFIG_DEFAULT();
    bluedroid_cfg.ssp_en = false; 
    ESP_ERROR_CHECK(esp_bluedroid_init_with_cfg(&bluedroid_cfg));
    ESP_ERROR_CHECK(esp_bluedroid_enable());

    esp_bt_gap_register_callback(esp_bt_gap_cb);
    esp_spp_register_callback(esp_spp_cb);

    esp_spp_cfg_t bt_spp_cfg = { .mode = ESP_SPP_MODE_CB, .enable_l2cap_ertm = true, .tx_buffer_size = 0 };
    esp_spp_enhanced_init(&bt_spp_cfg);

    esp_bt_pin_type_t pin_type = ESP_BT_PIN_TYPE_FIXED;
    esp_bt_pin_code_t pin_code = {'1', '2', '3', '4'};
    esp_bt_gap_set_pin(pin_type, 4, pin_code);

    xTaskCreate(mpu_data_task, "mpu_task", 8192, NULL, 5, NULL);
}