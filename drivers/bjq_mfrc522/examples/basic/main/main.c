/*
 * SPDX-FileCopyrightText: 2026 Bojaqu
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <inttypes.h>
#include <stdbool.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_log.h"
#include "bjq_mfrc522.h"

static const char *TAG = "bjq_mfrc522_example";

/* Adjust these GPIOs to match your board wiring. */
#define EXAMPLE_SPI_HOST SPI2_HOST
#define EXAMPLE_PIN_MISO GPIO_NUM_2
#define EXAMPLE_PIN_MOSI GPIO_NUM_4
#define EXAMPLE_PIN_SCLK GPIO_NUM_5
#define EXAMPLE_PIN_CS GPIO_NUM_6

/* Sector 1, first safe user data block (never block 0 or a sector trailer). */
#define EXAMPLE_MIFARE_DATA_BLOCK 4U

static void demo_mifare_classic_rw(bjq_mfrc522_handle_t reader, const bjq_mfrc522_uid_t *uid)
{
    const bjq_mfrc522_mifare_key_t default_key = {
        .bytes = BJQ_MFRC522_MIFARE_DEFAULT_KEY,
    };

    esp_err_t err =
        bjq_mfrc522_mifare_authenticate(reader, BJQ_MFRC522_MIFARE_KEY_A, EXAMPLE_MIFARE_DATA_BLOCK, &default_key, uid);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "MIFARE Key A auth failed on block %u: %s", (unsigned)EXAMPLE_MIFARE_DATA_BLOCK,
                 esp_err_to_name(err));
        return;
    }

    uint8_t original[BJQ_MFRC522_MIFARE_BLOCK_SIZE] = {0};
    err = bjq_mfrc522_mifare_read_block(reader, EXAMPLE_MIFARE_DATA_BLOCK, original);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "MIFARE read failed on block %u: %s", (unsigned)EXAMPLE_MIFARE_DATA_BLOCK, esp_err_to_name(err));
        return;
    }
    ESP_LOG_BUFFER_HEX_LEVEL(TAG, original, sizeof(original), ESP_LOG_INFO);

    uint8_t payload[BJQ_MFRC522_MIFARE_BLOCK_SIZE] = "BJQ_MFRC522_V03";
    err = bjq_mfrc522_mifare_write_block(reader, EXAMPLE_MIFARE_DATA_BLOCK, payload);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "MIFARE write failed on block %u: %s", (unsigned)EXAMPLE_MIFARE_DATA_BLOCK, esp_err_to_name(err));
        return;
    }

    uint8_t readback[BJQ_MFRC522_MIFARE_BLOCK_SIZE] = {0};
    err = bjq_mfrc522_mifare_read_block(reader, EXAMPLE_MIFARE_DATA_BLOCK, readback);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "MIFARE readback failed on block %u: %s", (unsigned)EXAMPLE_MIFARE_DATA_BLOCK,
                 esp_err_to_name(err));
        return;
    }

    if (memcmp(payload, readback, sizeof(payload)) == 0) {
        ESP_LOGI(TAG, "MIFARE block %u write/readback verified: \"%.15s\"", (unsigned)EXAMPLE_MIFARE_DATA_BLOCK,
                 (const char *)readback);
    } else {
        ESP_LOGW(TAG, "MIFARE block %u readback mismatch", (unsigned)EXAMPLE_MIFARE_DATA_BLOCK);
    }
}

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
    esp_err_t err = bjq_mfrc522_init(&cfg, &reader);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize bjq_mfrc522: %s", esp_err_to_name(err));
        ESP_ERROR_CHECK(spi_bus_free(EXAMPLE_SPI_HOST));
        return;
    }

    uint8_t version = 0;
    if (bjq_mfrc522_read_version(reader, &version) == ESP_OK) {
        ESP_LOGI(TAG, "MFRC522 VersionReg=0x%02X", version);
    }

    ESP_LOGI(TAG, "Polling for ISO/IEC 14443-3A cards...");
    while (true) {
        bool present = false;
        err = bjq_mfrc522_is_new_card_present(reader, &present);
        if (err == ESP_OK && present) {
            bjq_mfrc522_uid_t uid = {0};
            err = bjq_mfrc522_read_card_uid(reader, &uid);
            if (err == ESP_OK) {
                char uid_hex[BJQ_MFRC522_UID_HEX_STR_MIN_LEN] = {0};
                ESP_ERROR_CHECK(bjq_mfrc522_uid_to_hex(&uid, uid_hex, sizeof(uid_hex)));
                bjq_mfrc522_picc_type_t picc_type = bjq_mfrc522_get_picc_type(uid.sak);
                ESP_LOGI(TAG, "Card detected: UID=%s (%u bytes) SAK=0x%02X Type=%s", uid_hex, (unsigned)uid.size,
                         uid.sak, bjq_mfrc522_picc_type_name(picc_type));

                if (picc_type == BJQ_MFRC522_PICC_MIFARE_1K || picc_type == BJQ_MFRC522_PICC_MIFARE_4K ||
                    picc_type == BJQ_MFRC522_PICC_MIFARE_MINI) {
                    demo_mifare_classic_rw(reader, &uid);
                }

                (void)bjq_mfrc522_halt_card(reader);
                (void)bjq_mfrc522_stop_crypto(reader);
            } else {
                ESP_LOGW(TAG, "Failed to read card UID: %s", esp_err_to_name(err));
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
