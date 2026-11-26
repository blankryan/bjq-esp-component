/*
 * SPDX-FileCopyrightText: 2026 Bojaqu
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "bjq_mfrc522.h"
#include "bjq_mfrc522_regs.h"

static const char *TAG = "bjq_mfrc522";

struct bjq_mfrc522_ctx {
    bjq_mfrc522_config_t config;
    spi_device_handle_t spi_dev;
};

static inline uint8_t spi_write_addr(uint8_t reg)
{
    return (uint8_t)((reg << 1U) & 0x7EU);
}

static inline uint8_t spi_read_addr(uint8_t reg)
{
    return (uint8_t)(((reg << 1U) & 0x7EU) | 0x80U);
}

static esp_err_t mfrc522_calc_crc(bjq_mfrc522_handle_t handle, const uint8_t *data, size_t len, uint8_t out_crc[2])
{
    esp_err_t err = bjq_mfrc522_write_reg(handle, BJQ_MFRC522_REG_COMMAND, BJQ_MFRC522_CMD_IDLE);
    if (err != ESP_OK) {
        return err;
    }

    err = bjq_mfrc522_write_reg(handle, BJQ_MFRC522_REG_DIV_IRQ, BJQ_MFRC522_DIV_IRQ_CRC);
    if (err != ESP_OK) {
        return err;
    }

    err = bjq_mfrc522_write_reg(handle, BJQ_MFRC522_REG_FIFO_LEVEL, BJQ_MFRC522_FIFO_FLUSH_BIT);
    if (err != ESP_OK) {
        return err;
    }

    err = bjq_mfrc522_write_reg_buf(handle, BJQ_MFRC522_REG_FIFO_DATA, data, len);
    if (err != ESP_OK) {
        return err;
    }

    err = bjq_mfrc522_write_reg(handle, BJQ_MFRC522_REG_COMMAND, BJQ_MFRC522_CMD_CALC_CRC);
    if (err != ESP_OK) {
        return err;
    }

    int64_t deadline_us = esp_timer_get_time() + ((int64_t)handle->config.timeout_ms * 1000LL);
    while (true) {
        uint8_t irq = 0;
        err = bjq_mfrc522_read_reg(handle, BJQ_MFRC522_REG_DIV_IRQ, &irq);
        if (err != ESP_OK) {
            return err;
        }
        if ((irq & BJQ_MFRC522_DIV_IRQ_CRC) != 0U) {
            break;
        }
        if (esp_timer_get_time() >= deadline_us) {
            (void)bjq_mfrc522_write_reg(handle, BJQ_MFRC522_REG_COMMAND, BJQ_MFRC522_CMD_IDLE);
            return ESP_ERR_TIMEOUT;
        }
        esp_rom_delay_us(100);
    }

    err = bjq_mfrc522_write_reg(handle, BJQ_MFRC522_REG_COMMAND, BJQ_MFRC522_CMD_IDLE);
    if (err != ESP_OK) {
        return err;
    }

    err = bjq_mfrc522_read_reg(handle, BJQ_MFRC522_REG_CRC_RESULT_L, &out_crc[0]);
    if (err != ESP_OK) {
        return err;
    }

    return bjq_mfrc522_read_reg(handle, BJQ_MFRC522_REG_CRC_RESULT_H, &out_crc[1]);
}

static esp_err_t mfrc522_transceive(bjq_mfrc522_handle_t handle, const uint8_t *tx_data, size_t tx_len,
                                    uint8_t *rx_data, size_t *rx_len, uint8_t *valid_bits, uint8_t rx_align,
                                    bool check_crc)
{
    if (tx_data == NULL || tx_len == 0U || tx_len > BJQ_MFRC522_FIFO_SIZE) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t tx_last_bits = (valid_bits != NULL) ? (uint8_t)(*valid_bits & 0x07U) : 0U;
    uint8_t bit_framing = (uint8_t)(((rx_align & 0x07U) << 4U) | tx_last_bits);

    esp_err_t err = bjq_mfrc522_write_reg(handle, BJQ_MFRC522_REG_COMMAND, BJQ_MFRC522_CMD_IDLE);
    if (err != ESP_OK) {
        return err;
    }

    err = bjq_mfrc522_write_reg(handle, BJQ_MFRC522_REG_COM_IRQ, BJQ_MFRC522_COM_IRQ_CLEAR_ALL);
    if (err != ESP_OK) {
        return err;
    }

    err = bjq_mfrc522_write_reg(handle, BJQ_MFRC522_REG_FIFO_LEVEL, BJQ_MFRC522_FIFO_FLUSH_BIT);
    if (err != ESP_OK) {
        return err;
    }

    err = bjq_mfrc522_write_reg_buf(handle, BJQ_MFRC522_REG_FIFO_DATA, tx_data, tx_len);
    if (err != ESP_OK) {
        return err;
    }

    err = bjq_mfrc522_write_reg(handle, BJQ_MFRC522_REG_BIT_FRAMING, bit_framing);
    if (err != ESP_OK) {
        return err;
    }

    err = bjq_mfrc522_write_reg(handle, BJQ_MFRC522_REG_COMMAND, BJQ_MFRC522_CMD_TRANSCEIVE);
    if (err != ESP_OK) {
        return err;
    }

    err = bjq_mfrc522_set_reg_bits(handle, BJQ_MFRC522_REG_BIT_FRAMING, BJQ_MFRC522_BIT_START_SEND);
    if (err != ESP_OK) {
        return err;
    }

    int64_t deadline_us = esp_timer_get_time() + ((int64_t)handle->config.timeout_ms * 1000LL);
    bool completed = false;
    while (true) {
        uint8_t irq = 0;
        err = bjq_mfrc522_read_reg(handle, BJQ_MFRC522_REG_COM_IRQ, &irq);
        if (err != ESP_OK) {
            return err;
        }
        if ((irq & (BJQ_MFRC522_COM_IRQ_RX | BJQ_MFRC522_COM_IRQ_IDLE)) != 0U) {
            completed = true;
            break;
        }
        if ((irq & BJQ_MFRC522_COM_IRQ_TIMER) != 0U) {
            return ESP_ERR_TIMEOUT;
        }
        if (esp_timer_get_time() >= deadline_us) {
            return ESP_ERR_TIMEOUT;
        }
        vTaskDelay(1);
    }

    if (!completed) {
        return ESP_ERR_TIMEOUT;
    }

    uint8_t err_reg = 0;
    err = bjq_mfrc522_read_reg(handle, BJQ_MFRC522_REG_ERROR, &err_reg);
    if (err != ESP_OK) {
        return err;
    }

    if ((err_reg & (BJQ_MFRC522_ERR_BUFFER_OVFL | BJQ_MFRC522_ERR_PARITY | BJQ_MFRC522_ERR_PROTOCOL)) != 0U) {
        return ESP_ERR_INVALID_RESPONSE;
    }

    uint8_t rx_valid_bits = 0;
    if (rx_data != NULL && rx_len != NULL) {
        uint8_t fifo_len = 0;
        err = bjq_mfrc522_read_reg(handle, BJQ_MFRC522_REG_FIFO_LEVEL, &fifo_len);
        if (err != ESP_OK) {
            return err;
        }
        if (fifo_len > *rx_len) {
            return ESP_ERR_INVALID_SIZE;
        }
        *rx_len = fifo_len;
        if (fifo_len > 0U) {
            if (rx_align == 0U) {
                err = bjq_mfrc522_read_reg_buf(handle, BJQ_MFRC522_REG_FIFO_DATA, rx_data, fifo_len);
                if (err != ESP_OK) {
                    return err;
                }
            } else {
                uint8_t tmp[BJQ_MFRC522_FIFO_SIZE] = {0};
                err = bjq_mfrc522_read_reg_buf(handle, BJQ_MFRC522_REG_FIFO_DATA, tmp, fifo_len);
                if (err != ESP_OK) {
                    return err;
                }
                uint8_t mask = (uint8_t)((0xFFU << rx_align) & 0xFFU);
                rx_data[0] = (uint8_t)((rx_data[0] & ~mask) | (tmp[0] & mask));
                if (fifo_len > 1U) {
                    memcpy(&rx_data[1], &tmp[1], fifo_len - 1U);
                }
            }
        }

        uint8_t ctrl = 0;
        err = bjq_mfrc522_read_reg(handle, BJQ_MFRC522_REG_CONTROL, &ctrl);
        if (err != ESP_OK) {
            return err;
        }
        rx_valid_bits = (uint8_t)(ctrl & BJQ_MFRC522_CONTROL_RX_BITS);
        if (valid_bits != NULL) {
            *valid_bits = rx_valid_bits;
        }
    }

    if ((err_reg & BJQ_MFRC522_ERR_COLL) != 0U) {
        return ESP_ERR_INVALID_STATE;
    }

    if (check_crc && rx_data != NULL && rx_len != NULL) {
        if (*rx_len < 3U || rx_valid_bits != 0U) {
            return ESP_ERR_INVALID_CRC;
        }
        uint8_t crc[2] = {0};
        err = mfrc522_calc_crc(handle, rx_data, *rx_len - 2U, crc);
        if (err != ESP_OK) {
            return err;
        }
        if (rx_data[*rx_len - 2U] != crc[0] || rx_data[*rx_len - 1U] != crc[1]) {
            return ESP_ERR_INVALID_CRC;
        }
    }

    return ESP_OK;
}

static esp_err_t mfrc522_reqa_or_wupa(bjq_mfrc522_handle_t handle, uint8_t cmd, bool *out_present)
{
    if (handle == NULL || out_present == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    *out_present = false;

    /* Reset baud rate and modulation width before polling (ISO14443A 106 kbps). */
    esp_err_t err = bjq_mfrc522_write_reg(handle, BJQ_MFRC522_REG_TX_MODE, 0x00U);
    if (err != ESP_OK) {
        return err;
    }
    err = bjq_mfrc522_write_reg(handle, BJQ_MFRC522_REG_RX_MODE, 0x00U);
    if (err != ESP_OK) {
        return err;
    }
    err = bjq_mfrc522_write_reg(handle, BJQ_MFRC522_REG_MOD_WIDTH, 0x26U);
    if (err != ESP_OK) {
        return err;
    }

    /* ValuesAfterColl = 0 clears received bits after a collision. */
    err = bjq_mfrc522_clear_reg_bits(handle, BJQ_MFRC522_REG_COLL, BJQ_MFRC522_COLL_VALUES_AFTER);
    if (err != ESP_OK) {
        return err;
    }

    uint8_t atqa[2] = {0};
    size_t atqa_len = sizeof(atqa);
    uint8_t valid_bits = 7U; /* Short frame: 7 bits for REQA/WUPA. */

    err = mfrc522_transceive(handle, &cmd, 1U, atqa, &atqa_len, &valid_bits, 0U, false);
    if (err == ESP_ERR_TIMEOUT) {
        return ESP_OK;
    }
    if (err == ESP_ERR_INVALID_STATE) {
        /* Multiple cards collided during ATQA; treat as card present for anticollision. */
        *out_present = true;
        return ESP_OK;
    }
    if (err != ESP_OK) {
        if (err == ESP_ERR_INVALID_RESPONSE || err == ESP_ERR_INVALID_SIZE) {
            return ESP_OK;
        }
        return err;
    }

    if (atqa_len == 2U && valid_bits == 0U) {
        *out_present = true;
    }
    return ESP_OK;
}

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

    err = bjq_mfrc522_reset(ctx);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to reset MFRC522 transceiver: %s", esp_err_to_name(err));
        (void)spi_bus_remove_device(ctx->spi_dev);
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

    (void)bjq_mfrc522_set_antenna(handle, false);

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

esp_err_t bjq_mfrc522_reset(bjq_mfrc522_handle_t handle)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (handle->config.rst_io_num != GPIO_NUM_NC) {
        esp_err_t err = gpio_set_level(handle->config.rst_io_num, 0);
        if (err != ESP_OK) {
            return err;
        }
        esp_rom_delay_us(10);
        err = gpio_set_level(handle->config.rst_io_num, 1);
        if (err != ESP_OK) {
            return err;
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    } else {
        esp_err_t err = bjq_mfrc522_write_reg(handle, BJQ_MFRC522_REG_COMMAND, BJQ_MFRC522_CMD_SOFT_RESET);
        if (err != ESP_OK) {
            return err;
        }

        int64_t deadline_us = esp_timer_get_time() + ((int64_t)handle->config.timeout_ms * 1000LL);
        while (true) {
            uint8_t cmd_reg = 0;
            err = bjq_mfrc522_read_reg(handle, BJQ_MFRC522_REG_COMMAND, &cmd_reg);
            if (err != ESP_OK) {
                return err;
            }
            if ((cmd_reg & BJQ_MFRC522_CMD_POWER_DOWN_BIT) == 0U) {
                break;
            }
            if (esp_timer_get_time() >= deadline_us) {
                return ESP_ERR_TIMEOUT;
            }
            vTaskDelay(pdMS_TO_TICKS(5));
        }
    }

    /* Configure ISO14443A 106 kbps baseline framing and 25 ms internal timer timeout. */
    esp_err_t err = bjq_mfrc522_write_reg(handle, BJQ_MFRC522_REG_TX_MODE, 0x00U);
    if (err != ESP_OK) {
        return err;
    }
    err = bjq_mfrc522_write_reg(handle, BJQ_MFRC522_REG_RX_MODE, 0x00U);
    if (err != ESP_OK) {
        return err;
    }
    err = bjq_mfrc522_write_reg(handle, BJQ_MFRC522_REG_MOD_WIDTH, 0x26U);
    if (err != ESP_OK) {
        return err;
    }
    /* TAuto=1, f_timer = 13.56 MHz / (2*169 + 1) = 40 kHz (25 us/tick); reload=1000 -> 25 ms. */
    err = bjq_mfrc522_write_reg(handle, BJQ_MFRC522_REG_T_MODE, 0x80U);
    if (err != ESP_OK) {
        return err;
    }
    err = bjq_mfrc522_write_reg(handle, BJQ_MFRC522_REG_T_PRESCALER, 0xA9U);
    if (err != ESP_OK) {
        return err;
    }
    err = bjq_mfrc522_write_reg(handle, BJQ_MFRC522_REG_T_RELOAD_H, 0x03U);
    if (err != ESP_OK) {
        return err;
    }
    err = bjq_mfrc522_write_reg(handle, BJQ_MFRC522_REG_T_RELOAD_L, 0xE8U);
    if (err != ESP_OK) {
        return err;
    }
    /* Force 100% ASK modulation regardless of ModGsPReg. */
    err = bjq_mfrc522_write_reg(handle, BJQ_MFRC522_REG_TX_ASK, 0x40U);
    if (err != ESP_OK) {
        return err;
    }
    /* Preset CRC_A initial value to 0x6363 (ISO/IEC 14443-3A). */
    err = bjq_mfrc522_write_reg(handle, BJQ_MFRC522_REG_MODE, 0x3DU);
    if (err != ESP_OK) {
        return err;
    }

    err = bjq_mfrc522_set_rx_gain(handle, (bjq_mfrc522_rx_gain_t)CONFIG_BJQ_MFRC522_DEFAULT_RX_GAIN);
    if (err != ESP_OK) {
        return err;
    }

    return bjq_mfrc522_set_antenna(handle, true);
}

esp_err_t bjq_mfrc522_read_version(bjq_mfrc522_handle_t handle, uint8_t *out_version)
{
    if (handle == NULL || out_version == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    return bjq_mfrc522_read_reg(handle, BJQ_MFRC522_REG_VERSION, out_version);
}

esp_err_t bjq_mfrc522_set_antenna(bjq_mfrc522_handle_t handle, bool enable)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (enable) {
        uint8_t val = 0;
        esp_err_t err = bjq_mfrc522_read_reg(handle, BJQ_MFRC522_REG_TX_CONTROL, &val);
        if (err != ESP_OK) {
            return err;
        }
        if ((val & BJQ_MFRC522_TX_CONTROL_ANTENNA) != BJQ_MFRC522_TX_CONTROL_ANTENNA) {
            return bjq_mfrc522_write_reg(handle, BJQ_MFRC522_REG_TX_CONTROL,
                                         (uint8_t)(val | BJQ_MFRC522_TX_CONTROL_ANTENNA));
        }
        return ESP_OK;
    }

    return bjq_mfrc522_clear_reg_bits(handle, BJQ_MFRC522_REG_TX_CONTROL, BJQ_MFRC522_TX_CONTROL_ANTENNA);
}

esp_err_t bjq_mfrc522_set_rx_gain(bjq_mfrc522_handle_t handle, bjq_mfrc522_rx_gain_t gain)
{
    if (handle == NULL || (int)gain < 0 || (int)gain > (int)BJQ_MFRC522_RX_GAIN_48DB) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t val = 0;
    esp_err_t err = bjq_mfrc522_read_reg(handle, BJQ_MFRC522_REG_RF_CFG, &val);
    if (err != ESP_OK) {
        return err;
    }

    val = (uint8_t)((val & ~BJQ_MFRC522_RF_CFG_GAIN_MASK) |
                    (((uint8_t)gain << BJQ_MFRC522_RF_CFG_GAIN_SHIFT) & BJQ_MFRC522_RF_CFG_GAIN_MASK));
    return bjq_mfrc522_write_reg(handle, BJQ_MFRC522_REG_RF_CFG, val);
}

esp_err_t bjq_mfrc522_write_reg(bjq_mfrc522_handle_t handle, uint8_t reg, uint8_t value)
{
    if (handle == NULL || reg > BJQ_MFRC522_MAX_REG_ADDR) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t tx[2] = {spi_write_addr(reg), value};
    spi_transaction_t trans = {
        .length = 16,
        .tx_buffer = tx,
    };
    return spi_device_transmit(handle->spi_dev, &trans);
}

esp_err_t bjq_mfrc522_write_reg_buf(bjq_mfrc522_handle_t handle, uint8_t reg, const uint8_t *data, size_t len)
{
    if (handle == NULL || data == NULL || reg > BJQ_MFRC522_MAX_REG_ADDR || len == 0U || len > BJQ_MFRC522_FIFO_SIZE) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t tx[BJQ_MFRC522_FIFO_SIZE + 1U];
    tx[0] = spi_write_addr(reg);
    memcpy(&tx[1], data, len);

    spi_transaction_t trans = {
        .length = (len + 1U) * 8U,
        .tx_buffer = tx,
    };
    return spi_device_transmit(handle->spi_dev, &trans);
}

esp_err_t bjq_mfrc522_read_reg(bjq_mfrc522_handle_t handle, uint8_t reg, uint8_t *out_value)
{
    if (handle == NULL || out_value == NULL || reg > BJQ_MFRC522_MAX_REG_ADDR) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t tx[2] = {spi_read_addr(reg), 0x00U};
    uint8_t rx[2] = {0};
    spi_transaction_t trans = {
        .length = 16,
        .tx_buffer = tx,
        .rx_buffer = rx,
    };
    esp_err_t err = spi_device_transmit(handle->spi_dev, &trans);
    if (err != ESP_OK) {
        return err;
    }

    *out_value = rx[1];
    return ESP_OK;
}

esp_err_t bjq_mfrc522_read_reg_buf(bjq_mfrc522_handle_t handle, uint8_t reg, uint8_t *out_data, size_t len)
{
    if (handle == NULL || out_data == NULL || reg > BJQ_MFRC522_MAX_REG_ADDR || len == 0U ||
        len > BJQ_MFRC522_FIFO_SIZE) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t tx[BJQ_MFRC522_FIFO_SIZE + 1U];
    uint8_t rx[BJQ_MFRC522_FIFO_SIZE + 1U] = {0};
    uint8_t addr = spi_read_addr(reg);
    memset(tx, addr, len);
    tx[len] = 0x00U;

    spi_transaction_t trans = {
        .length = (len + 1U) * 8U,
        .tx_buffer = tx,
        .rx_buffer = rx,
    };
    esp_err_t err = spi_device_transmit(handle->spi_dev, &trans);
    if (err != ESP_OK) {
        return err;
    }

    memcpy(out_data, &rx[1], len);
    return ESP_OK;
}

esp_err_t bjq_mfrc522_set_reg_bits(bjq_mfrc522_handle_t handle, uint8_t reg, uint8_t mask)
{
    if (handle == NULL || reg > BJQ_MFRC522_MAX_REG_ADDR) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t val = 0;
    esp_err_t err = bjq_mfrc522_read_reg(handle, reg, &val);
    if (err != ESP_OK) {
        return err;
    }
    return bjq_mfrc522_write_reg(handle, reg, (uint8_t)(val | mask));
}

esp_err_t bjq_mfrc522_clear_reg_bits(bjq_mfrc522_handle_t handle, uint8_t reg, uint8_t mask)
{
    if (handle == NULL || reg > BJQ_MFRC522_MAX_REG_ADDR) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t val = 0;
    esp_err_t err = bjq_mfrc522_read_reg(handle, reg, &val);
    if (err != ESP_OK) {
        return err;
    }
    return bjq_mfrc522_write_reg(handle, reg, (uint8_t)(val & (uint8_t)(~mask)));
}

esp_err_t bjq_mfrc522_is_new_card_present(bjq_mfrc522_handle_t handle, bool *out_present)
{
    return mfrc522_reqa_or_wupa(handle, BJQ_MFRC522_PICC_CMD_REQA, out_present);
}

esp_err_t bjq_mfrc522_wakeup_card(bjq_mfrc522_handle_t handle, bool *out_present)
{
    return mfrc522_reqa_or_wupa(handle, BJQ_MFRC522_PICC_CMD_WUPA, out_present);
}

esp_err_t bjq_mfrc522_read_card_uid(bjq_mfrc522_handle_t handle, bjq_mfrc522_uid_t *out_uid)
{
    if (handle == NULL || out_uid == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(out_uid, 0, sizeof(*out_uid));

    esp_err_t err = bjq_mfrc522_clear_reg_bits(handle, BJQ_MFRC522_REG_COLL, BJQ_MFRC522_COLL_VALUES_AFTER);
    if (err != ESP_OK) {
        return err;
    }

    static const uint8_t cascade_cmds[3] = {
        BJQ_MFRC522_PICC_CMD_SEL_L1,
        BJQ_MFRC522_PICC_CMD_SEL_L2,
        BJQ_MFRC522_PICC_CMD_SEL_L3,
    };

    uint8_t uid_offset = 0U;
    for (uint8_t level = 0U; level < 3U; level++) {
        uint8_t frame[9] = {0};
        frame[0] = cascade_cmds[level];

        uint8_t known_bits = 0U;
        uint8_t sak_resp[3] = {0};
        size_t sak_len = 0U;
        bool selected = false;

        /* Anticollision loop (max 32 bit-resolutions per cascade level + final SELECT). */
        for (uint8_t iter = 0U; iter < 34U && !selected; iter++) {
            size_t tx_len = 0U;
            uint8_t tx_last_bits = 0U;
            uint8_t rx_align = 0U;
            uint8_t *rx_buf = NULL;
            size_t rx_len = 0U;
            bool is_select = (known_bits >= 32U);

            if (is_select) {
                frame[1] = 0x70U;
                frame[6] = (uint8_t)(frame[2] ^ frame[3] ^ frame[4] ^ frame[5]);
                err = mfrc522_calc_crc(handle, frame, 7U, &frame[7]);
                if (err != ESP_OK) {
                    return err;
                }
                tx_len = 9U;
                tx_last_bits = 0U;
                rx_align = 0U;
                rx_buf = sak_resp;
                rx_len = sizeof(sak_resp);
            } else {
                uint8_t full_bytes = (uint8_t)(known_bits / 8U);
                uint8_t rem_bits = (uint8_t)(known_bits % 8U);
                frame[1] = (uint8_t)(((2U + full_bytes) << 4U) | rem_bits);
                tx_len = (size_t)(2U + full_bytes + ((rem_bits > 0U) ? 1U : 0U));
                tx_last_bits = rem_bits;
                rx_align = rem_bits;
                rx_buf = &frame[2U + full_bytes];
                rx_len = sizeof(frame) - (size_t)(2U + full_bytes);
            }

            uint8_t valid_bits = tx_last_bits;
            err = mfrc522_transceive(handle, frame, tx_len, rx_buf, &rx_len, &valid_bits, rx_align, is_select);

            if (err == ESP_ERR_INVALID_STATE && !is_select) {
                /* Bit-level collision resolution per ISO/IEC 14443-3A. */
                uint8_t coll_reg = 0;
                err = bjq_mfrc522_read_reg(handle, BJQ_MFRC522_REG_COLL, &coll_reg);
                if (err != ESP_OK) {
                    return err;
                }
                if ((coll_reg & BJQ_MFRC522_COLL_POS_NOT_VALID) != 0U) {
                    return ESP_ERR_INVALID_RESPONSE;
                }
                uint8_t coll_pos = (uint8_t)(coll_reg & BJQ_MFRC522_COLL_POS_MASK);
                if (coll_pos == 0U) {
                    coll_pos = 32U;
                }
                if (coll_pos <= known_bits) {
                    return ESP_ERR_INVALID_RESPONSE;
                }
                known_bits = coll_pos;
                uint8_t bit_idx = (uint8_t)((known_bits - 1U) % 8U);
                uint8_t byte_idx = (uint8_t)(2U + ((known_bits - 1U) / 8U));
                frame[byte_idx] |= (uint8_t)(1U << bit_idx);
                continue;
            }

            if (err != ESP_OK) {
                return err;
            }

            if (is_select) {
                sak_len = rx_len;
                selected = true;
            } else {
                uint8_t full_bytes = (uint8_t)(known_bits / 8U);
                if ((size_t)full_bytes + rx_len != 5U || valid_bits != 0U) {
                    return ESP_ERR_INVALID_RESPONSE;
                }
                /* Verify received BCC across the 4 UID/CT bytes in frame[2..5]. */
                uint8_t expected_bcc = (uint8_t)(frame[2] ^ frame[3] ^ frame[4] ^ frame[5]);
                if (frame[6] != expected_bcc) {
                    return ESP_ERR_INVALID_CRC;
                }
                known_bits = 32U;
            }
        }

        if (!selected || sak_len != 3U) {
            return ESP_ERR_INVALID_RESPONSE;
        }

        uint8_t sak = sak_resp[0];
        bool has_cascade_tag = (frame[2] == BJQ_MFRC522_PICC_CMD_CT);

        if (has_cascade_tag) {
            if ((size_t)(uid_offset + 3U) > BJQ_MFRC522_UID_MAX_LEN) {
                return ESP_ERR_INVALID_RESPONSE;
            }
            memcpy(&out_uid->bytes[uid_offset], &frame[3], 3U);
            uid_offset = (uint8_t)(uid_offset + 3U);
            if ((sak & BJQ_MFRC522_SAK_CASCADE_BIT) == 0U) {
                return ESP_ERR_INVALID_RESPONSE;
            }
        } else {
            if ((size_t)(uid_offset + 4U) > BJQ_MFRC522_UID_MAX_LEN) {
                return ESP_ERR_INVALID_RESPONSE;
            }
            memcpy(&out_uid->bytes[uid_offset], &frame[2], 4U);
            uid_offset = (uint8_t)(uid_offset + 4U);
            if ((sak & BJQ_MFRC522_SAK_CASCADE_BIT) != 0U) {
                return ESP_ERR_INVALID_RESPONSE;
            }
            out_uid->size = uid_offset;
            out_uid->sak = sak;
            return ESP_OK;
        }
    }

    return ESP_ERR_INVALID_RESPONSE;
}

esp_err_t bjq_mfrc522_halt_card(bjq_mfrc522_handle_t handle)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t cmd[4] = {BJQ_MFRC522_PICC_CMD_HLTA, 0x00U, 0x00U, 0x00U};
    esp_err_t err = mfrc522_calc_crc(handle, cmd, 2U, &cmd[2]);
    if (err != ESP_OK) {
        return err;
    }

    /* Per ISO/IEC 14443-3A, a card acknowledging HLTA must remain silent (timeout == success). */
    err = mfrc522_transceive(handle, cmd, sizeof(cmd), NULL, NULL, NULL, 0U, false);
    if (err == ESP_ERR_TIMEOUT) {
        return ESP_OK;
    }
    if (err == ESP_OK) {
        return ESP_ERR_INVALID_RESPONSE;
    }
    return err;
}

esp_err_t bjq_mfrc522_stop_crypto(bjq_mfrc522_handle_t handle)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    return bjq_mfrc522_clear_reg_bits(handle, BJQ_MFRC522_REG_STATUS2, BJQ_MFRC522_STATUS2_CRYPTO1_ON);
}

static esp_err_t mfrc522_mifare_transceive_ack(bjq_mfrc522_handle_t handle, const uint8_t *send_data, size_t send_len)
{
    if (send_data == NULL || send_len == 0U || send_len > BJQ_MFRC522_MIFARE_BLOCK_SIZE) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t frame[BJQ_MFRC522_MIFARE_BLOCK_SIZE + 2U] = {0};
    memcpy(frame, send_data, send_len);
    esp_err_t err = mfrc522_calc_crc(handle, frame, send_len, &frame[send_len]);
    if (err != ESP_OK) {
        return err;
    }

    uint8_t rx_buf[4] = {0};
    size_t rx_len = sizeof(rx_buf);
    uint8_t valid_bits = 0U;
    err = mfrc522_transceive(handle, frame, send_len + 2U, rx_buf, &rx_len, &valid_bits, 0U, false);
    if (err != ESP_OK) {
        return err;
    }

    if (rx_len != 1U || valid_bits != 4U) {
        return ESP_ERR_INVALID_RESPONSE;
    }

    if ((rx_buf[0] & 0x0FU) != BJQ_MFRC522_MIFARE_ACK) {
        return ESP_ERR_NOT_ALLOWED;
    }

    return ESP_OK;
}

esp_err_t bjq_mfrc522_mifare_authenticate(bjq_mfrc522_handle_t handle, bjq_mfrc522_mifare_key_type_t key_type,
                                          uint8_t block_addr, const bjq_mfrc522_mifare_key_t *key,
                                          const bjq_mfrc522_uid_t *uid)
{
    if (handle == NULL || key == NULL || uid == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (key_type != BJQ_MFRC522_MIFARE_KEY_A && key_type != BJQ_MFRC522_MIFARE_KEY_B) {
        return ESP_ERR_INVALID_ARG;
    }
    if (uid->size != 4U && uid->size != 7U && uid->size != 10U) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t auth_cmd = (key_type == BJQ_MFRC522_MIFARE_KEY_A) ? BJQ_MFRC522_PICC_CMD_MF_AUTH_KEY_A
                                                              : BJQ_MFRC522_PICC_CMD_MF_AUTH_KEY_B;

    /* 12-byte MFAuthent payload: [cmd, block_addr, key(6), last 4 bytes of UID]. */
    uint8_t payload[12] = {0};
    payload[0] = auth_cmd;
    payload[1] = block_addr;
    memcpy(&payload[2], key->bytes, BJQ_MFRC522_MIFARE_KEY_SIZE);
    memcpy(&payload[8], &uid->bytes[uid->size - 4U], 4U);

    esp_err_t err = bjq_mfrc522_write_reg(handle, BJQ_MFRC522_REG_COMMAND, BJQ_MFRC522_CMD_IDLE);
    if (err != ESP_OK) {
        return err;
    }
    err = bjq_mfrc522_write_reg(handle, BJQ_MFRC522_REG_COM_IRQ, BJQ_MFRC522_COM_IRQ_CLEAR_ALL);
    if (err != ESP_OK) {
        return err;
    }
    err = bjq_mfrc522_write_reg(handle, BJQ_MFRC522_REG_FIFO_LEVEL, BJQ_MFRC522_FIFO_FLUSH_BIT);
    if (err != ESP_OK) {
        return err;
    }
    err = bjq_mfrc522_write_reg_buf(handle, BJQ_MFRC522_REG_FIFO_DATA, payload, sizeof(payload));
    if (err != ESP_OK) {
        return err;
    }
    err = bjq_mfrc522_write_reg(handle, BJQ_MFRC522_REG_BIT_FRAMING, 0x00U);
    if (err != ESP_OK) {
        return err;
    }
    err = bjq_mfrc522_write_reg(handle, BJQ_MFRC522_REG_COMMAND, BJQ_MFRC522_CMD_MF_AUTHENT);
    if (err != ESP_OK) {
        return err;
    }

    int64_t deadline_us = esp_timer_get_time() + ((int64_t)handle->config.timeout_ms * 1000LL);
    while (true) {
        uint8_t irq = 0;
        err = bjq_mfrc522_read_reg(handle, BJQ_MFRC522_REG_COM_IRQ, &irq);
        if (err != ESP_OK) {
            return err;
        }
        if ((irq & BJQ_MFRC522_COM_IRQ_IDLE) != 0U) {
            break;
        }
        if ((irq & BJQ_MFRC522_COM_IRQ_TIMER) != 0U) {
            (void)bjq_mfrc522_write_reg(handle, BJQ_MFRC522_REG_COMMAND, BJQ_MFRC522_CMD_IDLE);
            return ESP_ERR_TIMEOUT;
        }
        if (esp_timer_get_time() >= deadline_us) {
            (void)bjq_mfrc522_write_reg(handle, BJQ_MFRC522_REG_COMMAND, BJQ_MFRC522_CMD_IDLE);
            return ESP_ERR_TIMEOUT;
        }
        vTaskDelay(1);
    }

    uint8_t err_reg = 0;
    err = bjq_mfrc522_read_reg(handle, BJQ_MFRC522_REG_ERROR, &err_reg);
    if (err != ESP_OK) {
        return err;
    }
    if ((err_reg & (BJQ_MFRC522_ERR_BUFFER_OVFL | BJQ_MFRC522_ERR_PARITY | BJQ_MFRC522_ERR_PROTOCOL)) != 0U) {
        return ESP_ERR_INVALID_RESPONSE;
    }

    uint8_t status2 = 0;
    err = bjq_mfrc522_read_reg(handle, BJQ_MFRC522_REG_STATUS2, &status2);
    if (err != ESP_OK) {
        return err;
    }
    if ((status2 & BJQ_MFRC522_STATUS2_CRYPTO1_ON) == 0U) {
        return ESP_ERR_NOT_ALLOWED;
    }

    return ESP_OK;
}

esp_err_t bjq_mfrc522_mifare_read_block(bjq_mfrc522_handle_t handle, uint8_t block_addr,
                                        uint8_t out_data[BJQ_MFRC522_MIFARE_BLOCK_SIZE])
{
    if (handle == NULL || out_data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t cmd[4] = {BJQ_MFRC522_PICC_CMD_MF_READ, block_addr, 0x00U, 0x00U};
    esp_err_t err = mfrc522_calc_crc(handle, cmd, 2U, &cmd[2]);
    if (err != ESP_OK) {
        return err;
    }

    uint8_t rx_buf[BJQ_MFRC522_MIFARE_BLOCK_SIZE + 2U] = {0};
    size_t rx_len = sizeof(rx_buf);
    uint8_t valid_bits = 0U;
    err = mfrc522_transceive(handle, cmd, sizeof(cmd), rx_buf, &rx_len, &valid_bits, 0U, false);
    if (err != ESP_OK) {
        return err;
    }

    if (rx_len == 1U && valid_bits == 4U) {
        return ESP_ERR_NOT_ALLOWED;
    }
    if (rx_len != sizeof(rx_buf) || valid_bits != 0U) {
        return ESP_ERR_INVALID_RESPONSE;
    }

    uint8_t expected_crc[2] = {0};
    err = mfrc522_calc_crc(handle, rx_buf, BJQ_MFRC522_MIFARE_BLOCK_SIZE, expected_crc);
    if (err != ESP_OK) {
        return err;
    }
    if (rx_buf[BJQ_MFRC522_MIFARE_BLOCK_SIZE] != expected_crc[0] ||
        rx_buf[BJQ_MFRC522_MIFARE_BLOCK_SIZE + 1U] != expected_crc[1]) {
        return ESP_ERR_INVALID_CRC;
    }

    memcpy(out_data, rx_buf, BJQ_MFRC522_MIFARE_BLOCK_SIZE);
    return ESP_OK;
}

esp_err_t bjq_mfrc522_mifare_write_block_raw(bjq_mfrc522_handle_t handle, uint8_t block_addr,
                                             const uint8_t data[BJQ_MFRC522_MIFARE_BLOCK_SIZE])
{
    if (handle == NULL || data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t cmd[2] = {BJQ_MFRC522_PICC_CMD_MF_WRITE, block_addr};
    esp_err_t err = mfrc522_mifare_transceive_ack(handle, cmd, sizeof(cmd));
    if (err != ESP_OK) {
        return err;
    }

    return mfrc522_mifare_transceive_ack(handle, data, BJQ_MFRC522_MIFARE_BLOCK_SIZE);
}

esp_err_t bjq_mfrc522_mifare_write_block(bjq_mfrc522_handle_t handle, uint8_t block_addr,
                                         const uint8_t data[BJQ_MFRC522_MIFARE_BLOCK_SIZE])
{
    if (handle == NULL || data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (bjq_mfrc522_mifare_is_manufacturer_block(block_addr) || bjq_mfrc522_mifare_is_sector_trailer(block_addr)) {
        ESP_LOGE(TAG, "Refusing safe write to protected block %u (use write_block_raw for trailers)",
                 (unsigned)block_addr);
        return ESP_ERR_INVALID_ARG;
    }

    return bjq_mfrc522_mifare_write_block_raw(handle, block_addr, data);
}

bool bjq_mfrc522_mifare_is_sector_trailer(uint8_t block_addr)
{
    if (block_addr < 128U) {
        return (block_addr % 4U) == 3U;
    }
    return (block_addr % 16U) == 15U;
}

bool bjq_mfrc522_mifare_is_manufacturer_block(uint8_t block_addr)
{
    return block_addr == 0U;
}

bjq_mfrc522_picc_type_t bjq_mfrc522_get_picc_type(uint8_t sak)
{
    uint8_t masked = (uint8_t)(sak & 0x7FU);
    switch (masked) {
    case 0x04U:
        return BJQ_MFRC522_PICC_NOT_COMPLETE;
    case 0x09U:
        return BJQ_MFRC522_PICC_MIFARE_MINI;
    case 0x08U:
        return BJQ_MFRC522_PICC_MIFARE_1K;
    case 0x18U:
        return BJQ_MFRC522_PICC_MIFARE_4K;
    case 0x00U:
        return BJQ_MFRC522_PICC_MIFARE_ULTRALIGHT;
    case 0x10U:
    case 0x11U:
        return BJQ_MFRC522_PICC_MIFARE_PLUS;
    case 0x01U:
        return BJQ_MFRC522_PICC_TNP3XXX;
    case 0x20U:
        return BJQ_MFRC522_PICC_ISO_14443_4;
    case 0x40U:
        return BJQ_MFRC522_PICC_ISO_18092;
    default:
        return BJQ_MFRC522_PICC_UNKNOWN;
    }
}

const char *bjq_mfrc522_picc_type_name(bjq_mfrc522_picc_type_t type)
{
    switch (type) {
    case BJQ_MFRC522_PICC_ISO_14443_4:
        return "ISO/IEC 14443-4";
    case BJQ_MFRC522_PICC_ISO_18092:
        return "ISO/IEC 18092 (NFC)";
    case BJQ_MFRC522_PICC_MIFARE_MINI:
        return "MIFARE Mini (320B)";
    case BJQ_MFRC522_PICC_MIFARE_1K:
        return "MIFARE Classic 1K";
    case BJQ_MFRC522_PICC_MIFARE_4K:
        return "MIFARE Classic 4K";
    case BJQ_MFRC522_PICC_MIFARE_ULTRALIGHT:
        return "MIFARE Ultralight / NTAG";
    case BJQ_MFRC522_PICC_MIFARE_PLUS:
        return "MIFARE Plus";
    case BJQ_MFRC522_PICC_MIFARE_DESFIRE:
        return "MIFARE DESFire";
    case BJQ_MFRC522_PICC_TNP3XXX:
        return "MIFARE TNP3xxx";
    case BJQ_MFRC522_PICC_NOT_COMPLETE:
        return "UID not complete";
    case BJQ_MFRC522_PICC_UNKNOWN:
    default:
        return "Unknown";
    }
}

esp_err_t bjq_mfrc522_uid_to_hex(const bjq_mfrc522_uid_t *uid, char *out_buf, size_t buf_size)
{
    if (uid == NULL || out_buf == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (uid->size != 4U && uid->size != 7U && uid->size != 10U) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t required = (size_t)uid->size * 3U;
    if (buf_size < required) {
        return ESP_ERR_INVALID_SIZE;
    }

    size_t pos = 0U;
    for (uint8_t i = 0U; i < uid->size; i++) {
        int written = snprintf(&out_buf[pos], buf_size - pos, (i + 1U < uid->size) ? "%02X:" : "%02X", uid->bytes[i]);
        if (written < 0) {
            return ESP_FAIL;
        }
        pos += (size_t)written;
    }

    return ESP_OK;
}
