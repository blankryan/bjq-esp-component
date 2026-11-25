/*
 * SPDX-FileCopyrightText: 2026 Bojaqu
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <inttypes.h>
#include <stdlib.h>
#include "sdkconfig.h"
#include "esp_log.h"
#include "bjq_mfrc522.h"

#define BJQ_MFRC522_MAX_SPI_CLOCK_HZ 10000000

static const char *TAG = "bjq_mfrc522";

struct bjq_mfrc522_ctx {
    bjq_mfrc522_config_t config;
    spi_device_handle_t spi_dev;
};

esp_err_t bjq_mfrc522_init(const bjq_mfrc522_config_t *config, bjq_mfrc522_handle_t *out_handle)
{
    if (config == NULL || out_handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!GPIO_IS_VALID_OUTPUT_GPIO(config->cs_io_num)) {
        ESP_LOGE(TAG, "Invalid CS GPIO: %d", (int)config->cs_io_num);
        return ESP_ERR_INVALID_ARG;
    }

    if (config->rst_io_num != GPIO_NUM_NC && !GPIO_IS_VALID_OUTPUT_GPIO(config->rst_io_num)) {
        ESP_LOGE(TAG, "Invalid RST GPIO: %d", (int)config->rst_io_num);
        return ESP_ERR_INVALID_ARG;
    }

    int clock_hz = (config->spi_clock_hz > 0) ? config->spi_clock_hz : CONFIG_BJQ_MFRC522_DEFAULT_SPI_CLOCK_HZ;
    if (clock_hz <= 0 || clock_hz > BJQ_MFRC522_MAX_SPI_CLOCK_HZ) {
        ESP_LOGE(TAG, "SPI clock %d Hz exceeds MFRC522 limit (%d Hz)", clock_hz, BJQ_MFRC522_MAX_SPI_CLOCK_HZ);
        return ESP_ERR_INVALID_ARG;
    }

    struct bjq_mfrc522_ctx *ctx = calloc(1, sizeof(*ctx));
    if (ctx == NULL) {
        return ESP_ERR_NO_MEM;
    }

    ctx->config = *config;
    ctx->config.spi_clock_hz = clock_hz;
    ctx->config.timeout_ms = (config->timeout_ms > 0U) ? config->timeout_ms : CONFIG_BJQ_MFRC522_DEFAULT_TIMEOUT_MS;

    if (ctx->config.rst_io_num != GPIO_NUM_NC) {
        gpio_config_t rst_cfg = {
            .pin_bit_mask = (1ULL << ctx->config.rst_io_num),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        esp_err_t err = gpio_config(&rst_cfg);
        if (err != ESP_OK) {
            free(ctx);
            return err;
        }
        err = gpio_set_level(ctx->config.rst_io_num, 1);
        if (err != ESP_OK) {
            free(ctx);
            return err;
        }
    }

    spi_device_interface_config_t dev_cfg = {
        .clock_speed_hz = ctx->config.spi_clock_hz,
        .mode = 0,
        .spics_io_num = ctx->config.cs_io_num,
        .queue_size = 1,
    };

    esp_err_t err = spi_bus_add_device(ctx->config.spi_host, &dev_cfg, &ctx->spi_dev);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to attach MFRC522 SPI device: %s", esp_err_to_name(err));
        free(ctx);
        return err;
    }

    *out_handle = ctx;
    ESP_LOGI(TAG, "Initialized bjq_mfrc522 on host %d (CS=%d, clock=%d Hz, timeout=%" PRIu32 " ms)",
             (int)ctx->config.spi_host, (int)ctx->config.cs_io_num, ctx->config.spi_clock_hz, ctx->config.timeout_ms);
    return ESP_OK;
}

esp_err_t bjq_mfrc522_deinit(bjq_mfrc522_handle_t handle)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = spi_bus_remove_device(handle->spi_dev);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to remove MFRC522 SPI device: %s", esp_err_to_name(err));
        return err;
    }

    free(handle);
    return ESP_OK;
}

esp_err_t bjq_mfrc522_get_config(bjq_mfrc522_handle_t handle, bjq_mfrc522_config_t *out_config)
{
    if (handle == NULL || out_config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    *out_config = handle->config;
    return ESP_OK;
}
