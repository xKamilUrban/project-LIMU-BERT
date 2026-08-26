#pragma once
#include "config.h"

#define CONFIG_BROKER_URL "mqtt://broker.emqx.io"
//#define CONFIG_BROKER_URL "mqtt://192.168.1.10:1883"

void mqtt_app_start(void);
void mqtt_publish(const imu_data_t *data);
void mqtt_send_stop_signal(void);