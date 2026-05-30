#pragma once
#include "config.h"

#define CONFIG_BROKER_URL "mqtt://broker.emqx.io"

void mqtt_app_start(void);
void mqtt_publish(const imu_data_t *data);
void mqtt_send_stop_signal(void);