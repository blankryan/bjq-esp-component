/*
 * SPDX-FileCopyrightText: 2026 Bojaqu
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "unity.h"
#include "bjq_mfrc522.h"

TEST_CASE("init rejects null and invalid arguments", "[bjq_mfrc522]")
{
    bjq_mfrc522_handle_t handle = NULL;
    bjq_mfrc522_config_t cfg = {
        .spi_host = SPI2_HOST,
        .cs_io_num = GPIO_NUM_NC,
        .rst_io_num = GPIO_NUM_NC,
        .spi_clock_hz = 0,
        .timeout_ms = 0,
    };

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bjq_mfrc522_init(NULL, &handle));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bjq_mfrc522_init(&cfg, NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bjq_mfrc522_init(&cfg, &handle));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bjq_mfrc522_deinit(NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bjq_mfrc522_get_config(NULL, &cfg));
}

TEST_CASE("init rejects spi clock above MFRC522 datasheet limit", "[bjq_mfrc522]")
{
    bjq_mfrc522_handle_t handle = NULL;
    bjq_mfrc522_config_t cfg = {
        .spi_host = SPI2_HOST,
        .cs_io_num = GPIO_NUM_4,
        .rst_io_num = GPIO_NUM_NC,
        .spi_clock_hz = 20000000,
        .timeout_ms = 100,
    };

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bjq_mfrc522_init(&cfg, &handle));
}

TEST_CASE("protocol and register APIs reject null handles and invalid arguments", "[bjq_mfrc522]")
{
    uint8_t byte = 0;
    uint8_t buf[4] = {0};
    uint8_t block[BJQ_MFRC522_MIFARE_BLOCK_SIZE] = {0};
    bool present = false;
    bjq_mfrc522_uid_t uid = {.bytes = {0x01, 0x02, 0x03, 0x04}, .size = 4, .sak = 0x08};
    bjq_mfrc522_mifare_key_t key = {.bytes = BJQ_MFRC522_MIFARE_DEFAULT_KEY};

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bjq_mfrc522_reset(NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bjq_mfrc522_read_version(NULL, &byte));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bjq_mfrc522_set_antenna(NULL, true));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bjq_mfrc522_set_rx_gain(NULL, BJQ_MFRC522_RX_GAIN_33DB));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bjq_mfrc522_write_reg(NULL, 0x01U, 0x00U));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bjq_mfrc522_write_reg_buf(NULL, 0x09U, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bjq_mfrc522_read_reg(NULL, 0x37U, &byte));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bjq_mfrc522_read_reg_buf(NULL, 0x09U, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bjq_mfrc522_set_reg_bits(NULL, 0x14U, 0x03U));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bjq_mfrc522_clear_reg_bits(NULL, 0x14U, 0x03U));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bjq_mfrc522_is_new_card_present(NULL, &present));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bjq_mfrc522_wakeup_card(NULL, &present));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bjq_mfrc522_read_card_uid(NULL, &uid));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bjq_mfrc522_halt_card(NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bjq_mfrc522_stop_crypto(NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      bjq_mfrc522_mifare_authenticate(NULL, BJQ_MFRC522_MIFARE_KEY_A, 4U, &key, &uid));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bjq_mfrc522_mifare_read_block(NULL, 4U, block));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bjq_mfrc522_mifare_write_block(NULL, 4U, block));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bjq_mfrc522_mifare_write_block_raw(NULL, 4U, block));
}

TEST_CASE("get_picc_type decodes standard ISO14443A SAK values", "[bjq_mfrc522]")
{
    TEST_ASSERT_EQUAL(BJQ_MFRC522_PICC_MIFARE_1K, bjq_mfrc522_get_picc_type(0x08U));
    TEST_ASSERT_EQUAL(BJQ_MFRC522_PICC_MIFARE_4K, bjq_mfrc522_get_picc_type(0x18U));
    TEST_ASSERT_EQUAL(BJQ_MFRC522_PICC_MIFARE_ULTRALIGHT, bjq_mfrc522_get_picc_type(0x00U));
    TEST_ASSERT_EQUAL(BJQ_MFRC522_PICC_MIFARE_MINI, bjq_mfrc522_get_picc_type(0x09U));
    TEST_ASSERT_EQUAL(BJQ_MFRC522_PICC_ISO_14443_4, bjq_mfrc522_get_picc_type(0x20U));
    TEST_ASSERT_EQUAL(BJQ_MFRC522_PICC_NOT_COMPLETE, bjq_mfrc522_get_picc_type(0x04U));
    TEST_ASSERT_EQUAL(BJQ_MFRC522_PICC_UNKNOWN, bjq_mfrc522_get_picc_type(0x7FU));
    TEST_ASSERT_NOT_NULL(bjq_mfrc522_picc_type_name(BJQ_MFRC522_PICC_MIFARE_1K));
    TEST_ASSERT_NOT_NULL(bjq_mfrc522_picc_type_name(BJQ_MFRC522_PICC_UNKNOWN));
}

TEST_CASE("uid_to_hex formats 4, 7, and 10 byte UIDs and enforces bounds", "[bjq_mfrc522]")
{
    char out[BJQ_MFRC522_UID_HEX_STR_MIN_LEN] = {0};

    bjq_mfrc522_uid_t uid4 = {
        .bytes = {0xDE, 0xAD, 0xBE, 0xEF},
        .size = 4,
        .sak = 0x08,
    };
    TEST_ASSERT_EQUAL(ESP_OK, bjq_mfrc522_uid_to_hex(&uid4, out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("DE:AD:BE:EF", out);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_SIZE, bjq_mfrc522_uid_to_hex(&uid4, out, 11));

    bjq_mfrc522_uid_t uid7 = {
        .bytes = {0x04, 0xA1, 0xB2, 0xC3, 0xD4, 0xE5, 0xF6},
        .size = 7,
        .sak = 0x00,
    };
    TEST_ASSERT_EQUAL(ESP_OK, bjq_mfrc522_uid_to_hex(&uid7, out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("04:A1:B2:C3:D4:E5:F6", out);

    bjq_mfrc522_uid_t uid10 = {
        .bytes = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A},
        .size = 10,
        .sak = 0x20,
    };
    TEST_ASSERT_EQUAL(ESP_OK, bjq_mfrc522_uid_to_hex(&uid10, out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("01:02:03:04:05:06:07:08:09:0A", out);

    bjq_mfrc522_uid_t bad = {.size = 5};
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bjq_mfrc522_uid_to_hex(&bad, out, sizeof(out)));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bjq_mfrc522_uid_to_hex(NULL, out, sizeof(out)));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, bjq_mfrc522_uid_to_hex(&uid4, NULL, sizeof(out)));
}

TEST_CASE("mifare block classification guards manufacturer and 1K/4K sector trailers", "[bjq_mfrc522]")
{
    TEST_ASSERT_TRUE(bjq_mfrc522_mifare_is_manufacturer_block(0U));
    TEST_ASSERT_FALSE(bjq_mfrc522_mifare_is_manufacturer_block(1U));
    TEST_ASSERT_FALSE(bjq_mfrc522_mifare_is_manufacturer_block(4U));
    TEST_ASSERT_FALSE(bjq_mfrc522_mifare_is_manufacturer_block(255U));

    /* MIFARE 1K / lower 32 sectors of 4K (4 blocks per sector) */
    TEST_ASSERT_FALSE(bjq_mfrc522_mifare_is_sector_trailer(0U));
    TEST_ASSERT_FALSE(bjq_mfrc522_mifare_is_sector_trailer(2U));
    TEST_ASSERT_TRUE(bjq_mfrc522_mifare_is_sector_trailer(3U));
    TEST_ASSERT_FALSE(bjq_mfrc522_mifare_is_sector_trailer(4U));
    TEST_ASSERT_TRUE(bjq_mfrc522_mifare_is_sector_trailer(7U));
    TEST_ASSERT_TRUE(bjq_mfrc522_mifare_is_sector_trailer(63U));
    TEST_ASSERT_FALSE(bjq_mfrc522_mifare_is_sector_trailer(126U));
    TEST_ASSERT_TRUE(bjq_mfrc522_mifare_is_sector_trailer(127U));

    /* MIFARE 4K upper 8 sectors (16 blocks per sector) */
    TEST_ASSERT_FALSE(bjq_mfrc522_mifare_is_sector_trailer(128U));
    TEST_ASSERT_FALSE(bjq_mfrc522_mifare_is_sector_trailer(131U));
    TEST_ASSERT_FALSE(bjq_mfrc522_mifare_is_sector_trailer(142U));
    TEST_ASSERT_TRUE(bjq_mfrc522_mifare_is_sector_trailer(143U));
    TEST_ASSERT_TRUE(bjq_mfrc522_mifare_is_sector_trailer(159U));
    TEST_ASSERT_FALSE(bjq_mfrc522_mifare_is_sector_trailer(254U));
    TEST_ASSERT_TRUE(bjq_mfrc522_mifare_is_sector_trailer(255U));
}

void app_main(void)
{
    UNITY_BEGIN();
    unity_run_all_tests();
    UNITY_END();
}
