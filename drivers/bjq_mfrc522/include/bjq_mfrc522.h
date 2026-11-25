/*
 * SPDX-FileCopyrightText: 2026 Bojaqu
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file bjq_mfrc522.h
 * @brief Public API for the Bojaqu MFRC522 RFID/NFC transceiver driver.
 */

/**
 * @brief Opaque handle representing an initialized MFRC522 driver instance.
 */
typedef struct bjq_mfrc522_ctx *bjq_mfrc522_handle_t;

/**
 * @brief Configuration parameters for attaching an MFRC522 device to an SPI bus.
 *
 * The caller is responsible for initializing @p spi_host via spi_bus_initialize()
 * before calling bjq_mfrc522_init().
 */
typedef struct {
    spi_host_device_t spi_host; /**< Initialized ESP-IDF SPI host peripheral (for example SPI2_HOST). */
    gpio_num_t cs_io_num;       /**< Chip-select GPIO pin. Must be a valid output-capable GPIO. */
    gpio_num_t rst_io_num;      /**< Optional hardware reset GPIO pin, or GPIO_NUM_NC if unused. */
    int spi_clock_hz;           /**< SPI clock in Hz (0 uses CONFIG_BJQ_MFRC522_DEFAULT_SPI_CLOCK_HZ). */
    uint32_t timeout_ms;        /**< Operation timeout in ms (0 uses CONFIG_BJQ_MFRC522_DEFAULT_TIMEOUT_MS). */
} bjq_mfrc522_config_t;

/**
 * @brief Initialize an MFRC522 driver instance and attach it to an initialized SPI host.
 *
 * @param[in]  config     Pointer to device configuration. Must not be NULL.
 * @param[out] out_handle Returned opaque driver handle on success. Must not be NULL.
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if @p config or @p out_handle is NULL or parameters are out of range
 *      - ESP_ERR_NO_MEM if memory allocation fails
 *      - Other error codes propagated from esp_driver_spi or esp_driver_gpio
 */
esp_err_t bjq_mfrc522_init(const bjq_mfrc522_config_t *config, bjq_mfrc522_handle_t *out_handle);

/**
 * @brief Detach the MFRC522 device from the SPI bus and release all driver resources.
 *
 * The underlying SPI bus itself is not freed because it is owned by the caller.
 *
 * @param[in] handle Handle returned by bjq_mfrc522_init().
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if @p handle is NULL
 *      - Other error codes propagated from spi_bus_remove_device()
 */
esp_err_t bjq_mfrc522_deinit(bjq_mfrc522_handle_t handle);

/**
 * @brief Copy the resolved runtime configuration of an active MFRC522 handle.
 *
 * @param[in]  handle     Active driver handle. Must not be NULL.
 * @param[out] out_config Destination struct receiving resolved settings. Must not be NULL.
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if @p handle or @p out_config is NULL
 */
esp_err_t bjq_mfrc522_get_config(bjq_mfrc522_handle_t handle, bjq_mfrc522_config_t *out_config);

#ifdef __cplusplus
}
#endif
