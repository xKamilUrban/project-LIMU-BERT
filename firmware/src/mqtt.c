#include "mqtt.h"
#include "config.h"
#include "esp_event.h"
#include "mqtt_client.h"
#include "esp_log.h"

static const char *TAG = "imu_mqtt";
static esp_mqtt_client_handle_t client;

#define BATCH_SIZE 10


static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data){
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        current_mode = MODE_WIFI;
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        break;

    case MQTT_EVENT_DISCONNECTED:
        current_mode = MODE_NONE;
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        break;

    default:
        break;
    }
}

void mqtt_app_start(void){
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = CONFIG_BROKER_URL,
    };

    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
}

void mqtt_publish(const imu_data_t *data){
    static imu_data_t batch_buffer[BATCH_SIZE];
    static int batch_idx = 0;

    if(current_mode != MODE_WIFI || client == NULL){
        batch_idx = 0;
        return;
    } 
    batch_buffer[batch_idx++] = *data;

    if(batch_idx >= BATCH_SIZE){
        esp_mqtt_client_publish(client, "/imu/data", (const char *)batch_buffer, sizeof(imu_data_t) * BATCH_SIZE, 1, 0);
        batch_idx = 0;
    }
}

void mqtt_send_stop_signal(void) {
    if (client != NULL && current_mode == MODE_WIFI) {
        ESP_LOGI(TAG, "LOGGER STOP");
        esp_mqtt_client_publish(client, "/imu/control", "STOP", 0, 1, 0);
    }
}