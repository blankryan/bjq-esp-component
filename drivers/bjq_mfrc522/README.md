# bjq_mfrc522

ESP-IDF v6.1 driver for the NXP MFRC522 13.56 MHz RFID/NFC transceiver.

## Overview

- **Layer:** `drivers`
- **Current status:** Reference skeleton (`0.1.0` — lifecycle, SPI device registration, and optional reset GPIO configuration are implemented; RF register transactions and ISO14443A card detection are planned).
- **Scope:** Reusable SPI-based driver for MFRC522 readers using ESP-IDF v6.1 modular drivers (`esp_driver_spi`, `esp_driver_gpio`).
- **Non-goals:** Product-specific access-control policies, card UID databases, or SPI bus pin allocation (the consuming product initializes and owns the SPI host bus).

## Compatibility

| Item | Supported |
|---|---|
| ESP-IDF | `>=6.1.0,<7.0.0` |
| Target SoCs | `esp32`, `esp32s3`, `esp32c3`, `esp32c6` |
| License | Apache-2.0 |

## Dependencies

- **ESP-IDF components:** `esp_driver_spi`, `esp_driver_gpio`, `log` (private).
- **Intra-repo components:** None.
- **Hardware requirements:**
  - 3.3 V MFRC522 module connected to an initialized ESP-IDF SPI host bus (MOSI, MISO, SCLK, CS).
  - Optional reset GPIO (`rst_io_num`, set to `GPIO_NUM_NC` if tied high in hardware).

## Integration

Add `bjq_mfrc522` to your product's `main/idf_component.yml`:

```yaml
dependencies:
  idf:
    version: ">=6.1.0,<7.0.0"
  bjq_mfrc522:
    git: "https://github.com/blankryan/bjq-esp-component.git"
    path: "drivers/bjq_mfrc522"
    version: "bjq_mfrc522/v0.1.0"
```

Initialize the SPI bus in your product firmware, then attach `bjq_mfrc522`:

```c
#include "driver/spi_master.h"
#include "bjq_mfrc522.h"

bjq_mfrc522_config_t cfg = {
    .spi_host = SPI2_HOST,
    .cs_io_num = GPIO_NUM_5,
    .rst_io_num = GPIO_NUM_NC,
    .spi_clock_hz = 0, // Uses CONFIG_BJQ_MFRC522_DEFAULT_SPI_CLOCK_HZ
    .timeout_ms = 0,   // Uses CONFIG_BJQ_MFRC522_DEFAULT_TIMEOUT_MS
};

bjq_mfrc522_handle_t reader = NULL;
ESP_ERROR_CHECK(bjq_mfrc522_init(&cfg, &reader));
```

## Documentation

- [Architecture](docs/architecture.md)
- [Configuration](docs/configuration.md)
- [Concurrency & Resources](docs/concurrency-and-resources.md)
- [Troubleshooting & Error Handling](docs/troubleshooting.md)
- [Bench Testing & Hardware Validation](docs/bench-testing.md)
- [Changelog](CHANGELOG.md)

## Testing

- **Examples:** `examples/basic` — initializes an SPI2 bus, attaches `bjq_mfrc522`, logs active configuration, and deinitializes cleanly (compiled in CI on `esp32`, `esp32s3`, `esp32c3`, `esp32c6`).
- **Test App:** `test_apps/` — Unity test suite covering parameter validation and SPI handle lifecycle (compiled in CI on all 4 targets).
- **Hardware verification:** Not yet executed on physical MFRC522 hardware (build-verified only).

## License

Licensed under the [Apache License 2.0](LICENSE).
