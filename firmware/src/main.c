#include "bluetooth.h"
#include "config.h"
#include "bluetooth.h"
#include "mpu6050.h"
#include "nvs.h"
#include "nvs_flash.h"


void app_main(void)
{
    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );


    mpu6050_init();
    bluetooth_init();

    xTaskCreate(mpu_data_task, "mpu_task", 8192, NULL, 5, NULL);
}