#pragma once

#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

typedef struct {
    uint32_t timestamp;
    int16_t ax, ay, az;
    int16_t gx, gy, gz;
} imu_data_t;

typedef enum {
    MODE_NONE,
    MODE_BLUETOOTH,
    MODE_WIFI
} app_mode_t;

volatile app_mode_t current_mode = MODE_NONE;

extern QueueHandle_t imu_queue;
extern app_mode_t current_mode;