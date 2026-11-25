# bjq_mfrc522

ESP-IDF v6.1 driver for the NXP MFRC522 13.56 MHz RFID/NFC transceiver.

## Overview

- **Layer:** `drivers`
- **Current behavior:** Full SPI register I/O (single/burst read/write, bit set/clear), hardware/software reset, `VersionReg` probing, antenna TX1/TX2 & receiver gain control, and ISO/IEC 14443-3A card polling (`REQA`/`WUPA`), multi-cascade anticollision and selection (4-, 7-, and 10-byte UIDs with BCC & hardware CRC_A verification), `HLTA`, SAK PICC type decoding, and UID hex formatting.
- **Design intent:** Optional MIFARE Classic Crypto1 authentication and block read/write APIs in a future release.
- **Scope:** Reusable SPI-based MFRC522 driver built on ESP-IDF v6.1 modular drivers (`esp_driver_spi`, `esp_driver_gpio`).
- **Non-goals:** Product-specific access-control databases, card authorization logic, or SPI host bus pin allocation (the consuming product initializes and owns the SPI host bus).

## Compatibility

| Item | Supported |
|---|---|
| ESP-IDF | `>=6.1.0,<7.0.0` |
| Target SoCs | `esp32`, `esp32s3`, `esp32c3`, `esp32c6` |
| License | Apache-2.0 |

## Dependencies

- **ESP-IDF components:** `esp_driver_spi`, `esp_driver_gpio` (public); `log`, `esp_timer`, `freertos` (private).
- **Intra-repo components:** None.
- **Hardware requirements:**
  - 3.3 V MFRC522 module connected to an initialized ESP-IDF SPI host bus (MOSI, MISO, SCLK, CS; SPI mode 0, up to 10 MHz).
  - Optional hardware reset GPIO (`rst_io_num`, set to `GPIO_NUM_NC` if tied to 3.3 V in hardware).

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

Initialize the SPI bus in your product firmware, attach `bjq_mfrc522`, and poll for ISO14443A cards:

```c
#include "driver/spi_master.h"
#include "esp_log.h"
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

bool present = false;
if (bjq_mfrc522_is_new_card_present(reader, &present) == ESP_OK && present) {
    bjq_mfrc522_uid_t uid = {0};
    if (bjq_mfrc522_read_card_uid(reader, &uid) == ESP_OK) {
        char hex[BJQ_MFRC522_UID_HEX_STR_MIN_LEN];
        bjq_mfrc522_uid_to_hex(&uid, hex, sizeof(hex));
        ESP_LOGI("app", "Card UID=%s (%s)", hex,
                 bjq_mfrc522_picc_type_name(bjq_mfrc522_get_picc_type(uid.sak)));
        bjq_mfrc522_halt_card(reader);
    }
}
```

## Documentation

- [Architecture](docs/architecture.md)
- [Configuration](docs/configuration.md)
- [Concurrency & Resources](docs/concurrency-and-resources.md)
- [Troubleshooting & Error Handling](docs/troubleshooting.md)
- [Bench Testing & Hardware Validation](docs/bench-testing.md)
- [Changelog](CHANGELOG.md)

## Testing

- **Examples:** `examples/basic` — initializes an SPI2 bus, attaches `bjq_mfrc522`, logs `VersionReg`, and polls for ISO14443A cards in a FreeRTOS loop (compiled in CI on `esp32`, `esp32s3`, `esp32c3`, `esp32c6`).
- **Test App:** `test_apps/` — Unity test suite verifying parameter/bounds validation across register and card APIs, SAK PICC-type decoding, and 4/7/10-byte UID hex formatting (compiled in CI on all 4 targets).
- **Hardware verification:** Pending physical MFRC522 bench validation (currently build-verified across all 4 targets).

## License

Licensed under the [Apache License 2.0](LICENSE).
