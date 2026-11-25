# Architecture — `bjq_mfrc522`

## Current Behavior vs. Design Intent

- **Current behavior:** Implements full SPI register I/O (single and 64-byte FIFO burst transfers), hardware/software reset, `VersionReg` reading, antenna TX1/TX2 & receiver gain control, MFRC522 hardware CRC_A calculation, and ISO/IEC 14443-3A card detection (`REQA`/`WUPA`), multi-cascade anticollision/select (4-, 7-, and 10-byte UIDs), `HLTA`, SAK PICC type decoding, and UID formatting.
- **Design intent:** Optional MIFARE Classic Crypto1 authentication and memory block read/write APIs in future minor releases, keeping existing lifecycle and UID APIs backward-compatible.

## Module Overview

```mermaid
flowchart LR
    APP["Consuming Product Firmware"] -->|Initializes SPI bus| SPIBUS["ESP-IDF SPI Host Bus (esp_driver_spi)"]
    APP -->|Public C API| DRV["bjq_mfrc522 Driver"]
    DRV -->|SPI Mode 0 Register & FIFO I/O| SPIBUS
    DRV -->|Optional RST Pulse| GPIO["ESP-IDF GPIO (esp_driver_gpio)"]
    DRV -->|Polling Deadlines| TIMER["ESP-IDF Timer & FreeRTOS"]
```

## ISO/IEC 14443-3A Card-Reading Sequence

```mermaid
sequenceDiagram
    participant App as Product Task
    participant Drv as bjq_mfrc522
    participant Chip as MFRC522 (SPI)
    participant Card as ISO14443A PICC

    App->>Drv: bjq_mfrc522_is_new_card_present()
    Drv->>Chip: Transceive REQA (0x26, 7-bit short frame)
    Chip->>Card: RF REQA
    Card-->>Chip: 2-byte ATQA
    Drv-->>App: out_present = true

    App->>Drv: bjq_mfrc522_read_card_uid()
    loop Cascade Levels 1..3 (0x93, 0x95, 0x97)
        Drv->>Chip: ANTICOLL (NVB=0x20, resolve bit collisions if CollErr)
        Chip->>Card: RF ANTICOLL
        Card-->>Chip: 4 bytes UID/CT + BCC (verified)
        Drv->>Chip: PCD_CALC_CRC + SELECT (NVB=0x70 + UID + BCC + CRC_A)
        Chip->>Card: RF SELECT
        Card-->>Chip: SAK + CRC_A (verified via hardware CRC_A)
    end
    Drv-->>App: bjq_mfrc522_uid_t (bytes, size, sak)

    App->>Drv: bjq_mfrc522_halt_card()
    Drv->>Chip: Transceive HLTA (0x50, 0x00, CRC_A)
    Chip->>Card: RF HLTA (silent acknowledgement -> timeout == ESP_OK)
```

## Ownership & Lifecycle

1. The consuming product calls `spi_bus_initialize()` once for the shared SPI host.
2. The product calls `bjq_mfrc522_init()`, which attaches a dedicated `spi_device_handle_t`, optionally configures `rst_io_num`, runs `bjq_mfrc522_reset()`, applies default `RFCfgReg` gain, and turns on TX1/TX2 antenna drivers.
3. `bjq_mfrc522` owns the opaque `bjq_mfrc522_handle_t` and `spi_device_handle_t`, and never frees the caller-owned SPI host bus.
4. Calling `bjq_mfrc522_deinit()` disables the RF antenna, detaches the SPI device, and frees the handle.
