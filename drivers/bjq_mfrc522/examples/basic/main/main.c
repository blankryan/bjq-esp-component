/*
 * SPDX-FileCopyrightText: 2026 Bojaqu
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <inttypes.h>
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_log.h"
#include "bjq_mfrc522.h"

static const char *TAG = "bjq_mfrc522_example";

/* Safe default GPIOs valid across esp32, esp32s3, esp32c3, and esp32c6 for build/smoke testing. */
#define EXAMPLE_SPI_HOST SPI2_HOST
#define EXAMPLE_PIN_MISO GPIO_NUM_2
#define EXAMPLE_PIN_MOSI GPIO_NUM_4
#define EXAMPLE_PIN_SCLK GPIO_NUM_5
#define EXAMPLE_PIN_CS   GPIO_NUM_6

void app_main(void)
{
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = EXAMPLE_PIN_MOSI,
        .miso_io_num = EXAMPLE_PIN_MISO,
        .sclk_io_num = EXAMPLE_PIN_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };

    ESP_ERROR_CHECK(spi_bus_initialize(EXAMPLE_SPI_HOST, &bus_cfg, SPI_DMA_CH_AUTO));

    bjq_mfrc522_config_t cfg = {
        .spi_host = EXAMPLE_SPI_HOST,
        .cs_io_num = EXAMPLE_PIN_CS,
        .rst_io_num = GPIO_NUM_NC,
        .spi_clock_hz = 0,
        .timeout_ms = 0,
    };

    bjq_mfrc522_handle_t reader = NULL;
    ESP_ERROR_CHECK(bjq_mfrc522_init(&cfg, &reader));

    bjq_mfrc522_config_t active_cfg = {0};
    ESP_ERROR_CHECK(bjq_mfrc522_get_config(reader, &active_cfg));
    ESP_LOGI(TAG, "Active MFRC522 SPI clock=%d Hz, timeout=%" PRIu32 " ms", active_cfg.spi_clock_hz,
             active_cfg.timeout_ms);

    ESP_ERROR_CHECK(bjq_mfrc522_deinit(reader));
    ESP_ERROR_CHECK(spi_bus_free(EXAMPLE_SPI_HOST));
}
