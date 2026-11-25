/*
 * SPDX-FileCopyrightText: 2026 Bojaqu
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "unity.h"
#include "__COMPONENT_NAME__.h"

TEST_CASE("init rejects null arguments", "[__COMPONENT_NAME__]")
{
    __COMPONENT_NAME___handle_t handle = NULL;
    __COMPONENT_NAME___config_t cfg = {
        .timeout_ms = 50,
    };

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, __COMPONENT_NAME___init(NULL, &handle));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, __COMPONENT_NAME___init(&cfg, NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, __COMPONENT_NAME___deinit(NULL));
}

TEST_CASE("init and deinit lifecycle succeeds", "[__COMPONENT_NAME__]")
{
    __COMPONENT_NAME___handle_t handle = NULL;
    __COMPONENT_NAME___config_t cfg = {
        .timeout_ms = 0,
    };

    TEST_ASSERT_EQUAL(ESP_OK, __COMPONENT_NAME___init(&cfg, &handle));
    TEST_ASSERT_NOT_NULL(handle);
    TEST_ASSERT_EQUAL(ESP_OK, __COMPONENT_NAME___deinit(handle));
}

void app_main(void)
{
    UNITY_BEGIN();
    unity_run_all_tests();
    UNITY_END();
}
