/*
 * SPDX-FileCopyrightText: 2026 Bojaqu
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <inttypes.h>
#include <stdlib.h>
#include "sdkconfig.h"
#include "esp_log.h"
#include "__COMPONENT_NAME__.h"

static const char *TAG = "__COMPONENT_NAME__";

struct __COMPONENT_NAME___ctx {
    uint32_t timeout_ms;
};

esp_err_t __COMPONENT_NAME___init(const __COMPONENT_NAME___config_t *config, __COMPONENT_NAME___handle_t *out_handle)
{
    if (config == NULL || out_handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    struct __COMPONENT_NAME___ctx *ctx = calloc(1, sizeof(*ctx));
    if (ctx == NULL) {
        return ESP_ERR_NO_MEM;
    }

    ctx->timeout_ms = (config->timeout_ms > 0U) ? config->timeout_ms : CONFIG___COMPONENT_UPPER___DEFAULT_TIMEOUT_MS;
    *out_handle = ctx;

    ESP_LOGI(TAG, "Initialized %s (timeout=%" PRIu32 " ms)", TAG, ctx->timeout_ms);
    return ESP_OK;
}

esp_err_t __COMPONENT_NAME___deinit(__COMPONENT_NAME___handle_t handle)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    free(handle);
    return ESP_OK;
}
