# Changelog

All notable changes to `bjq_mfrc522` will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- Renamed component to `bjq_mfrc522` under `drivers/bjq_mfrc522` with full `bjq_` namespacing.
- Added ESP-IDF v6.1.0 modular driver `CMakeLists.txt` (`esp_driver_spi`, `esp_driver_gpio`), `Kconfig`, and `idf_component.yml`.
- Added Doxygen-documented lifecycle API (`bjq_mfrc522_init`, `bjq_mfrc522_deinit`, `bjq_mfrc522_get_config`) with SPI device handle management and optional reset GPIO setup.
- Added component documentation (`README.md`, `docs/`), `examples/basic`, and `test_apps/`.
