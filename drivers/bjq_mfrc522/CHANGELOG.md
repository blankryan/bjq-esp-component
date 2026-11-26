# Changelog

All notable changes to `bjq_mfrc522` will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [0.2.0] - 2026-11-26

### Added

- SPI register access API (`bjq_mfrc522_write_reg`, `bjq_mfrc522_write_reg_buf`, `bjq_mfrc522_read_reg`, `bjq_mfrc522_read_reg_buf`, `bjq_mfrc522_set_reg_bits`, `bjq_mfrc522_clear_reg_bits`).
- Transceiver control API (`bjq_mfrc522_reset`, `bjq_mfrc522_read_version`, `bjq_mfrc522_set_antenna`, `bjq_mfrc522_set_rx_gain`, `bjq_mfrc522_stop_crypto`).
- ISO/IEC 14443-3A card detection and UID reading protocol (`bjq_mfrc522_is_new_card_present`, `bjq_mfrc522_wakeup_card`, `bjq_mfrc522_read_card_uid`, `bjq_mfrc522_halt_card`) supporting 4-, 7-, and 10-byte UIDs with bit-level anticollision, BCC, and MFRC522 hardware CRC_A verification.
- Card metadata helpers (`bjq_mfrc522_get_picc_type`, `bjq_mfrc522_picc_type_name`, `bjq_mfrc522_uid_to_hex`).
- Kconfig option `CONFIG_BJQ_MFRC522_DEFAULT_RX_GAIN` for default `RFCfgReg` antenna gain.
- Continuous card-polling loop in `examples/basic`, expanded Unity unit tests in `test_apps/`, and Wokwi ESP32-S3 + `board-mfrc522` simulation configs (`wokwi.toml`, `diagram.json`) with recorded bench-test evidence in `docs/bench-testing.md`.

### Changed

- `bjq_mfrc522_init()` now resets and configures the MFRC522 transceiver and enables the RF antenna automatically; `bjq_mfrc522_deinit()` disables the RF antenna before detaching the SPI device.

## [0.1.0] - 2026-11-25

### Added

- Renamed component to `bjq_mfrc522` under `drivers/bjq_mfrc522` with full `bjq_` namespacing.
- Added ESP-IDF v6.1.0 modular driver `CMakeLists.txt` (`esp_driver_spi`, `esp_driver_gpio`), `Kconfig`, and `idf_component.yml`.
- Added Doxygen-documented lifecycle API (`bjq_mfrc522_init`, `bjq_mfrc522_deinit`, `bjq_mfrc522_get_config`) with SPI device handle management and optional reset GPIO setup.
- Added component documentation (`README.md`, `docs/`), `examples/basic`, and `test_apps/`.

[Unreleased]: https://github.com/blankryan/bjq-esp-component/compare/bjq_mfrc522/v0.2.0...HEAD
[0.2.0]: https://github.com/blankryan/bjq-esp-component/compare/bjq_mfrc522/v0.1.0...bjq_mfrc522/v0.2.0
[0.1.0]: https://github.com/blankryan/bjq-esp-component/releases/tag/bjq_mfrc522/v0.1.0
