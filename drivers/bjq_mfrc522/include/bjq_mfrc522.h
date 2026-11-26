/*
 * SPDX-FileCopyrightText: 2026 Bojaqu
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file bjq_mfrc522.h
 * @brief Public API for the Bojaqu MFRC522 13.56 MHz RFID/NFC transceiver driver.
 */

/**
 * @brief Maximum UID length in bytes supported by ISO/IEC 14443-3A (triple-size UID).
 */
#define BJQ_MFRC522_UID_MAX_LEN 10U

/**
 * @brief Minimum output buffer size required by bjq_mfrc522_uid_to_hex() for a 10-byte UID.
 *
 * Format is "XX:XX:...:XX" plus a NUL terminator (10 * 3 = 30 bytes).
 */
#define BJQ_MFRC522_UID_HEX_STR_MIN_LEN 30U

/**
 * @brief Length in bytes of a MIFARE Classic Crypto1 authentication key (Key A or Key B).
 */
#define BJQ_MFRC522_MIFARE_KEY_SIZE 6U

/**
 * @brief Length in bytes of a single MIFARE Classic memory block.
 */
#define BJQ_MFRC522_MIFARE_BLOCK_SIZE 16U

/**
 * @brief Initializer for the factory-default MIFARE Classic key (FF:FF:FF:FF:FF:FF).
 */
#define BJQ_MFRC522_MIFARE_DEFAULT_KEY {0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU}

/**
 * @brief Opaque handle representing an initialized MFRC522 driver instance.
 */
typedef struct bjq_mfrc522_ctx *bjq_mfrc522_handle_t;

/**
 * @brief MFRC522 receiver antenna gain levels (RFCfgReg bits 6..4).
 */
typedef enum {
    BJQ_MFRC522_RX_GAIN_18DB = 0,     /**< 18 dB receiver gain. */
    BJQ_MFRC522_RX_GAIN_23DB = 1,     /**< 23 dB receiver gain. */
    BJQ_MFRC522_RX_GAIN_18DB_ALT = 2, /**< 18 dB receiver gain (alternate datasheet code). */
    BJQ_MFRC522_RX_GAIN_23DB_ALT = 3, /**< 23 dB receiver gain (alternate datasheet code). */
    BJQ_MFRC522_RX_GAIN_33DB = 4,     /**< 33 dB receiver gain (MFRC522 reset default). */
    BJQ_MFRC522_RX_GAIN_38DB = 5,     /**< 38 dB receiver gain. */
    BJQ_MFRC522_RX_GAIN_43DB = 6,     /**< 43 dB receiver gain. */
    BJQ_MFRC522_RX_GAIN_48DB = 7,     /**< 48 dB receiver gain (maximum sensitivity). */
} bjq_mfrc522_rx_gain_t;

/**
 * @brief Decoded ISO/IEC 14443-3A PICC (card/tag) type derived from the SAK byte.
 */
typedef enum {
    BJQ_MFRC522_PICC_UNKNOWN = 0,       /**< Unrecognized or unsupported SAK code. */
    BJQ_MFRC522_PICC_ISO_14443_4,       /**< PICC compliant with ISO/IEC 14443-4 (e.g., DESFire, smartcards). */
    BJQ_MFRC522_PICC_ISO_18092,         /**< PICC compliant with ISO/IEC 18092 (NFCIP-1). */
    BJQ_MFRC522_PICC_MIFARE_MINI,       /**< MIFARE Classic Mini (320 bytes). */
    BJQ_MFRC522_PICC_MIFARE_1K,         /**< MIFARE Classic 1K. */
    BJQ_MFRC522_PICC_MIFARE_4K,         /**< MIFARE Classic 4K. */
    BJQ_MFRC522_PICC_MIFARE_ULTRALIGHT, /**< MIFARE Ultralight or NTAG2xx. */
    BJQ_MFRC522_PICC_MIFARE_PLUS,       /**< MIFARE Plus. */
    BJQ_MFRC522_PICC_MIFARE_DESFIRE,    /**< MIFARE DESFire. */
    BJQ_MFRC522_PICC_TNP3XXX,           /**< MIFARE TNP3xxx. */
    BJQ_MFRC522_PICC_NOT_COMPLETE,      /**< Cascade bit set; UID selection not yet complete. */
} bjq_mfrc522_picc_type_t;

/**
 * @brief ISO/IEC 14443-3A card UID and Select Acknowledge (SAK) metadata.
 */
typedef struct {
    uint8_t bytes[BJQ_MFRC522_UID_MAX_LEN]; /**< Raw UID bytes (4, 7, or 10 valid bytes). */
    uint8_t size;                           /**< Number of valid bytes in @p bytes (4, 7, or 10). */
    uint8_t sak;                            /**< Select Acknowledge (SAK) byte returned by the card. */
} bjq_mfrc522_uid_t;

/**
 * @brief MIFARE Classic key selector used with bjq_mfrc522_mifare_authenticate().
 */
typedef enum {
    BJQ_MFRC522_MIFARE_KEY_A = 0, /**< Authenticate using sector Key A (PICC command 0x60). */
    BJQ_MFRC522_MIFARE_KEY_B = 1, /**< Authenticate using sector Key B (PICC command 0x61). */
} bjq_mfrc522_mifare_key_type_t;

/**
 * @brief 6-byte MIFARE Classic Crypto1 sector authentication key.
 */
typedef struct {
    uint8_t bytes[BJQ_MFRC522_MIFARE_KEY_SIZE]; /**< Raw 6-byte key material. */
} bjq_mfrc522_mifare_key_t;

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
 * @brief Initialize an MFRC522 driver instance, attach it to an SPI host, and reset the transceiver.
 *
 * Performs SPI device attachment, optional hardware reset, soft reset, timer/modulation
 * initialization, default RxGain setup, and enables the 13.56 MHz antenna driver.
 *
 * @param[in]  config     Pointer to device configuration. Must not be NULL.
 * @param[out] out_handle Returned opaque driver handle on success. Must not be NULL.
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if @p config or @p out_handle is NULL or parameters are out of range
 *      - ESP_ERR_NO_MEM if memory allocation fails
 *      - ESP_ERR_TIMEOUT if the MFRC522 does not exit soft reset within the configured timeout
 *      - Other error codes propagated from esp_driver_spi or esp_driver_gpio
 */
esp_err_t bjq_mfrc522_init(const bjq_mfrc522_config_t *config, bjq_mfrc522_handle_t *out_handle);

/**
 * @brief Turn off the antenna, detach the MFRC522 SPI device, and release all driver resources.
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

/**
 * @brief Perform a hardware (if configured) and software reset and reapply baseline MFRC522 registers.
 *
 * @param[in] handle Active driver handle. Must not be NULL.
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if @p handle is NULL
 *      - ESP_ERR_TIMEOUT if the transceiver does not clear PowerDown after soft reset
 *      - Other error codes from SPI or GPIO operations
 */
esp_err_t bjq_mfrc522_reset(bjq_mfrc522_handle_t handle);

/**
 * @brief Read the MFRC522 silicon version register (VersionReg, address 0x37).
 *
 * Common values: 0x91 (MFRC522 v1.0), 0x92 (MFRC522 v2.0), 0x88 (FM17522 clone).
 * Values 0x00 or 0xFF typically indicate an unconnected bus or wiring fault.
 *
 * @param[in]  handle      Active driver handle. Must not be NULL.
 * @param[out] out_version Returned 8-bit VersionReg value. Must not be NULL.
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if @p handle or @p out_version is NULL
 *      - Other error codes from SPI operations
 */
esp_err_t bjq_mfrc522_read_version(bjq_mfrc522_handle_t handle, uint8_t *out_version);

/**
 * @brief Enable or disable the TX1/TX2 RF antenna driver pins.
 *
 * @param[in] handle Active driver handle. Must not be NULL.
 * @param[in] enable True to turn the RF field on, false to turn it off.
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if @p handle is NULL
 *      - Other error codes from SPI operations
 */
esp_err_t bjq_mfrc522_set_antenna(bjq_mfrc522_handle_t handle, bool enable);

/**
 * @brief Configure the receiver antenna gain in RFCfgReg.
 *
 * @param[in] handle Active driver handle. Must not be NULL.
 * @param[in] gain   Receiver gain enum value (0..7).
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if @p handle is NULL or @p gain is out of range
 *      - Other error codes from SPI operations
 */
esp_err_t bjq_mfrc522_set_rx_gain(bjq_mfrc522_handle_t handle, bjq_mfrc522_rx_gain_t gain);

/**
 * @brief Write a single byte to an MFRC522 register over SPI.
 *
 * @param[in] handle Active driver handle. Must not be NULL.
 * @param[in] reg    6-bit register address (0x00..0x3F).
 * @param[in] value  Byte value to write.
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if @p handle is NULL or @p reg exceeds 0x3F
 *      - Other error codes from SPI operations
 */
esp_err_t bjq_mfrc522_write_reg(bjq_mfrc522_handle_t handle, uint8_t reg, uint8_t value);

/**
 * @brief Write multiple bytes sequentially to an MFRC522 register (such as FIFODataReg).
 *
 * @param[in] handle Active driver handle. Must not be NULL.
 * @param[in] reg    6-bit register address (0x00..0x3F).
 * @param[in] data   Pointer to bytes to transmit. Must not be NULL.
 * @param[in] len    Number of bytes to write (1..64).
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if arguments are NULL or @p len is 0 or exceeds 64
 *      - Other error codes from SPI operations
 */
esp_err_t bjq_mfrc522_write_reg_buf(bjq_mfrc522_handle_t handle, uint8_t reg, const uint8_t *data, size_t len);

/**
 * @brief Read a single byte from an MFRC522 register over SPI.
 *
 * @param[in]  handle    Active driver handle. Must not be NULL.
 * @param[in]  reg       6-bit register address (0x00..0x3F).
 * @param[out] out_value Returned register byte. Must not be NULL.
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if @p handle or @p out_value is NULL or @p reg exceeds 0x3F
 *      - Other error codes from SPI operations
 */
esp_err_t bjq_mfrc522_read_reg(bjq_mfrc522_handle_t handle, uint8_t reg, uint8_t *out_value);

/**
 * @brief Read multiple bytes sequentially from an MFRC522 register (such as FIFODataReg).
 *
 * @param[in]  handle   Active driver handle. Must not be NULL.
 * @param[in]  reg      6-bit register address (0x00..0x3F).
 * @param[out] out_data Destination buffer. Must not be NULL.
 * @param[in]  len      Number of bytes to read (1..64).
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if arguments are NULL or @p len is 0 or exceeds 64
 *      - Other error codes from SPI operations
 */
esp_err_t bjq_mfrc522_read_reg_buf(bjq_mfrc522_handle_t handle, uint8_t reg, uint8_t *out_data, size_t len);

/**
 * @brief Set specific bits in an MFRC522 register via read-modify-write.
 *
 * @param[in] handle Active driver handle. Must not be NULL.
 * @param[in] reg    6-bit register address (0x00..0x3F).
 * @param[in] mask   Bitmask of bits to set to 1.
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if @p handle is NULL or @p reg exceeds 0x3F
 *      - Other error codes from SPI operations
 */
esp_err_t bjq_mfrc522_set_reg_bits(bjq_mfrc522_handle_t handle, uint8_t reg, uint8_t mask);

/**
 * @brief Clear specific bits in an MFRC522 register via read-modify-write.
 *
 * @param[in] handle Active driver handle. Must not be NULL.
 * @param[in] reg    6-bit register address (0x00..0x3F).
 * @param[in] mask   Bitmask of bits to clear to 0.
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if @p handle is NULL or @p reg exceeds 0x3F
 *      - Other error codes from SPI operations
 */
esp_err_t bjq_mfrc522_clear_reg_bits(bjq_mfrc522_handle_t handle, uint8_t reg, uint8_t mask);

/**
 * @brief Poll the RF field for an ISO/IEC 14443-3A card in IDLE state using REQA (0x26).
 *
 * @param[in]  handle      Active driver handle. Must not be NULL.
 * @param[out] out_present Set to true if a card responded with a valid 2-byte ATQA, false otherwise.
 *
 * @return
 *      - ESP_OK on successful poll (check @p out_present for card presence)
 *      - ESP_ERR_INVALID_ARG if @p handle or @p out_present is NULL
 *      - Other error codes from SPI transport failures
 */
esp_err_t bjq_mfrc522_is_new_card_present(bjq_mfrc522_handle_t handle, bool *out_present);

/**
 * @brief Poll the RF field for any ISO/IEC 14443-3A card (IDLE or HALT) using WUPA (0x52).
 *
 * @param[in]  handle      Active driver handle. Must not be NULL.
 * @param[out] out_present Set to true if a card responded with a valid 2-byte ATQA, false otherwise.
 *
 * @return
 *      - ESP_OK on successful poll (check @p out_present for card presence)
 *      - ESP_ERR_INVALID_ARG if @p handle or @p out_present is NULL
 *      - Other error codes from SPI transport failures
 */
esp_err_t bjq_mfrc522_wakeup_card(bjq_mfrc522_handle_t handle, bool *out_present);

/**
 * @brief Run ISO/IEC 14443-3A anticollision and selection across Cascade Levels 1-3 to read a card UID.
 *
 * Call after bjq_mfrc522_is_new_card_present() or bjq_mfrc522_wakeup_card() reports a card present.
 * Supports single (4-byte), double (7-byte), and triple (10-byte) UIDs with BCC and CRC_A verification.
 *
 * @param[in]  handle  Active driver handle. Must not be NULL.
 * @param[out] out_uid Destination struct populated with UID bytes, size, and SAK. Must not be NULL.
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if @p handle or @p out_uid is NULL
 *      - ESP_ERR_TIMEOUT if the card left the field or stopped responding
 *      - ESP_ERR_INVALID_CRC if BCC or SAK CRC_A verification failed
 *      - ESP_ERR_INVALID_RESPONSE if the card returned a malformed frame or framing error
 *      - Other error codes from SPI operations
 */
esp_err_t bjq_mfrc522_read_card_uid(bjq_mfrc522_handle_t handle, bjq_mfrc522_uid_t *out_uid);

/**
 * @brief Put the currently selected ISO/IEC 14443-3A card into HALT state (HLTA command).
 *
 * @param[in] handle Active driver handle. Must not be NULL.
 *
 * @return
 *      - ESP_OK if the card accepted HLTA (silent per ISO/IEC 14443-3A)
 *      - ESP_ERR_INVALID_ARG if @p handle is NULL
 *      - ESP_ERR_INVALID_RESPONSE if the card unexpectedly replied to HLTA
 *      - Other error codes from SPI operations
 */
esp_err_t bjq_mfrc522_halt_card(bjq_mfrc522_handle_t handle);

/**
 * @brief Clear the MIFARE Crypto1 state bit (MFCrypto1On) in Status2Reg.
 *
 * Call this after completing an authenticated MIFARE Classic session (and calling
 * bjq_mfrc522_halt_card()) so subsequent REQA/WUPA polls operate unencrypted.
 *
 * @param[in] handle Active driver handle. Must not be NULL.
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if @p handle is NULL
 *      - Other error codes from SPI operations
 */
esp_err_t bjq_mfrc522_stop_crypto(bjq_mfrc522_handle_t handle);

/**
 * @brief Authenticate a MIFARE Classic sector using the MFRC522 Crypto1 engine (PCD_MFAuthent).
 *
 * The card must already be selected via bjq_mfrc522_read_card_uid(). Authentication applies to the
 * entire sector containing @p block_addr until a different sector is authenticated, the card leaves
 * the field, or bjq_mfrc522_stop_crypto() is called.
 *
 * @param[in] handle     Active driver handle. Must not be NULL.
 * @param[in] key_type   Key A or Key B selector.
 * @param[in] block_addr Target block address within the sector to authenticate (0..255).
 * @param[in] key        Pointer to 6-byte MIFARE key. Must not be NULL.
 * @param[in] uid        Pointer to the selected card UID (size 4, 7, or 10). Must not be NULL.
 *
 * @return
 *      - ESP_OK if Crypto1 authentication succeeded (MFCrypto1On is set)
 *      - ESP_ERR_INVALID_ARG if arguments are NULL, @p key_type is invalid, or @p uid->size is invalid
 *      - ESP_ERR_NOT_ALLOWED if the card rejected the key (MFCrypto1On remained cleared)
 *      - ESP_ERR_TIMEOUT if the card did not respond within the configured timeout
 *      - ESP_ERR_INVALID_RESPONSE if the transceiver reported a framing, parity, or overflow error
 *      - Other error codes from SPI operations
 */
esp_err_t bjq_mfrc522_mifare_authenticate(bjq_mfrc522_handle_t handle, bjq_mfrc522_mifare_key_type_t key_type,
                                          uint8_t block_addr, const bjq_mfrc522_mifare_key_t *key,
                                          const bjq_mfrc522_uid_t *uid);

/**
 * @brief Read a 16-byte block from an authenticated MIFARE Classic card (PICC command 0x30).
 *
 * Verifies the 2-byte ISO/IEC 14443-3A CRC_A returned by the card using the MFRC522 hardware CRC engine.
 *
 * @param[in]  handle     Active driver handle. Must not be NULL.
 * @param[in]  block_addr Block address to read (0..255). Sector must be authenticated first.
 * @param[out] out_data   Destination buffer receiving 16 bytes on success. Must not be NULL.
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if @p handle or @p out_data is NULL
 *      - ESP_ERR_NOT_ALLOWED if the card returned a 4-bit NACK (e.g., sector not authenticated)
 *      - ESP_ERR_INVALID_CRC if the returned 2-byte CRC_A did not match
 *      - ESP_ERR_TIMEOUT if the card did not respond
 *      - ESP_ERR_INVALID_RESPONSE if the returned frame length or framing was invalid
 *      - Other error codes from SPI operations
 */
esp_err_t bjq_mfrc522_mifare_read_block(bjq_mfrc522_handle_t handle, uint8_t block_addr,
                                        uint8_t out_data[BJQ_MFRC522_MIFARE_BLOCK_SIZE]);

/**
 * @brief Safely write a 16-byte user data block to an authenticated MIFARE Classic card.
 *
 * Rejects block 0 (manufacturer block) and sector trailers (checked via
 * bjq_mfrc522_mifare_is_sector_trailer()) with ESP_ERR_INVALID_ARG to prevent accidentally
 * corrupting access bits or permanently locking a sector. Use bjq_mfrc522_mifare_write_block_raw()
 * only when intentionally updating keys or access conditions in a sector trailer.
 *
 * @param[in] handle     Active driver handle. Must not be NULL.
 * @param[in] block_addr Non-trailer user data block address (must not be block 0 or a sector trailer).
 * @param[in] data       16-byte payload to write. Must not be NULL.
 *
 * @return
 *      - ESP_OK if both command and payload phases received a 4-bit MIFARE ACK (0x0A)
 *      - ESP_ERR_INVALID_ARG if arguments are NULL or @p block_addr is block 0 or a sector trailer
 *      - ESP_ERR_NOT_ALLOWED if the card returned a 4-bit NACK
 *      - ESP_ERR_TIMEOUT if the card did not respond
 *      - ESP_ERR_INVALID_RESPONSE if the transceiver reported a framing or protocol error
 *      - Other error codes from SPI operations
 */
esp_err_t bjq_mfrc522_mifare_write_block(bjq_mfrc522_handle_t handle, uint8_t block_addr,
                                         const uint8_t data[BJQ_MFRC522_MIFARE_BLOCK_SIZE]);

/**
 * @brief Write a 16-byte block to an authenticated MIFARE Classic card without trailer guards.
 *
 * @warning Writing malformed access bits to a sector trailer can permanently lock the sector.
 *          Prefer bjq_mfrc522_mifare_write_block() for normal user data blocks.
 *
 * @param[in] handle     Active driver handle. Must not be NULL.
 * @param[in] block_addr Target block address (0..255). Sector must be authenticated first.
 * @param[in] data       16-byte payload to write. Must not be NULL.
 *
 * @return
 *      - ESP_OK if both write phases received a 4-bit MIFARE ACK (0x0A)
 *      - ESP_ERR_INVALID_ARG if @p handle or @p data is NULL
 *      - ESP_ERR_NOT_ALLOWED if the card returned a 4-bit NACK
 *      - ESP_ERR_TIMEOUT if the card did not respond
 *      - ESP_ERR_INVALID_RESPONSE if the transceiver reported a framing or protocol error
 *      - Other error codes from SPI operations
 */
esp_err_t bjq_mfrc522_mifare_write_block_raw(bjq_mfrc522_handle_t handle, uint8_t block_addr,
                                             const uint8_t data[BJQ_MFRC522_MIFARE_BLOCK_SIZE]);

/**
 * @brief Check whether a MIFARE Classic 1K/4K block address is a sector trailer.
 *
 * Sectors 0..31 (blocks 0..127) have 4 blocks per sector (trailers at block % 4 == 3).
 * Sectors 32..39 on MIFARE Classic 4K (blocks 128..255) have 16 blocks per sector (trailers at block % 16 == 15).
 *
 * @param[in] block_addr Block address (0..255).
 *
 * @return True if @p block_addr is a sector trailer, false otherwise.
 */
bool bjq_mfrc522_mifare_is_sector_trailer(uint8_t block_addr);

/**
 * @brief Check whether a MIFARE Classic block address is the read-only manufacturer block (block 0).
 *
 * @param[in] block_addr Block address (0..255).
 *
 * @return True if @p block_addr is 0, false otherwise.
 */
bool bjq_mfrc522_mifare_is_manufacturer_block(uint8_t block_addr);

/**
 * @brief Decode a card's Select Acknowledge (SAK) byte into a PICC type enum.
 *
 * @param[in] sak SAK byte from bjq_mfrc522_uid_t::sak.
 *
 * @return Decoded @ref bjq_mfrc522_picc_type_t value.
 */
bjq_mfrc522_picc_type_t bjq_mfrc522_get_picc_type(uint8_t sak);

/**
 * @brief Return a human-readable static string describing a PICC type.
 *
 * @param[in] type PICC type enum value.
 *
 * @return NUL-terminated static string (never NULL).
 */
const char *bjq_mfrc522_picc_type_name(bjq_mfrc522_picc_type_t type);

/**
 * @brief Format a card UID as an uppercase colon-separated hexadecimal string (e.g., "04:A1:B2:C3").
 *
 * @param[in]  uid      Pointer to a populated UID structure (size must be 4, 7, or 10).
 * @param[out] out_buf  Destination character buffer. Must not be NULL.
 * @param[in]  buf_size Size of @p out_buf in bytes (must be at least uid->size * 3).
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if @p uid or @p out_buf is NULL, or @p uid->size is invalid
 *      - ESP_ERR_INVALID_SIZE if @p buf_size is too small for the formatted string
 */
esp_err_t bjq_mfrc522_uid_to_hex(const bjq_mfrc522_uid_t *uid, char *out_buf, size_t buf_size);

#ifdef __cplusplus
}
#endif
