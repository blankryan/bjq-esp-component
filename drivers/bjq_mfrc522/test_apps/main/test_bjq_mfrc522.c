/*
 * SPDX-FileCopyrightText: 2026 Bojaqu
 *
 * SPDX-License-Identifier: Apache-2.0
 */

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

void app_main(void)
{
    UNITY_BEGIN();
    unity_run_all_tests();
    UNITY_END();
}
