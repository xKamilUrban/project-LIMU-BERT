#pragma once
#include "esp_gap_bt_api.h"

void esp_bt_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param);
bool bluetooth_is_ready(void);
uint32_t bluetooth_get_handle(void);
void bluetooth_init(void);