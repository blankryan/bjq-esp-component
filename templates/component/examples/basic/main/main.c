/*
 * SPDX-FileCopyrightText: 2026 Bojaqu
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_err.h"
#include "esp_log.h"
#include "__COMPONENT_NAME__.h"

static const char *TAG = "example";

void app_main(void)
{
    __COMPONENT_NAME___config_t cfg = {
        .timeout_ms = 0,
    };
    __COMPONENT_NAME___handle_t handle = NULL;

    esp_err_t err = __COMPONENT_NAME___init(&cfg, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize __COMPONENT_NAME__: %s", esp_err_to_name(err));
        return;
    }

    ESP_LOGI(TAG, "__COMPONENT_NAME__ basic example initialized");
    ESP_ERROR_CHECK(__COMPONENT_NAME___deinit(handle));
}
