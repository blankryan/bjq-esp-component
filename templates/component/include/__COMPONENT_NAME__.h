/*
 * SPDX-FileCopyrightText: 2026 Bojaqu
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file __COMPONENT_NAME__.h
 * @brief Public API for __COMPONENT_NAME__.
 */

/**
 * @brief Opaque handle representing a __COMPONENT_NAME__ instance.
 */
typedef struct __COMPONENT_NAME___ctx *__COMPONENT_NAME___handle_t;

/**
 * @brief Configuration structure for initializing __COMPONENT_NAME__.
 */
typedef struct {
    uint32_t timeout_ms; /**< Operation timeout in milliseconds (0 uses Kconfig default). */
} __COMPONENT_NAME___config_t;

/**
 * @brief Create and initialize a __COMPONENT_NAME__ instance.
 *
 * @param[in]  config     Pointer to configuration structure. Must not be NULL.
 * @param[out] out_handle Returned handle on success. Must not be NULL.
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if @p config or @p out_handle is NULL
 *      - ESP_ERR_NO_MEM if memory allocation fails
 */
esp_err_t __COMPONENT_NAME___init(const __COMPONENT_NAME___config_t *config, __COMPONENT_NAME___handle_t *out_handle);

/**
 * @brief Deinitialize and release resources owned by a __COMPONENT_NAME__ instance.
 *
 * @param[in] handle Handle returned by __COMPONENT_NAME___init().
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if @p handle is NULL
 */
esp_err_t __COMPONENT_NAME___deinit(__COMPONENT_NAME___handle_t handle);

#ifdef __cplusplus
}
#endif
