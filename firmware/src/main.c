#include "bluetooth.h"
#include "config.h"
#include "bluetooth.h"
#include "mpu6050.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "wifi.h"
#include "mqtt.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_log.h"
#include "driver/gpio.h"

volatile app_mode_t current_mode = MODE_NONE;
#define BOOT_BUTTON_PIN GPIO_NUM_0

void init_boot_button(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BOOT_BUTTON_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
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

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    mpu6050_init();
    bluetooth_init();
    
    if (wifi_init_sta()) {
        mqtt_app_start();
    } else {
        ESP_LOGI("main", "WiFi failed, switching to Bluetooth");
        esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
    }

    xTaskCreatePinnedToCore(mpu_data_task, "mpu_task", 8192, NULL, 10, NULL, 1);
}