# Troubleshooting & Error Handling — `bjq_mfrc522`

## Error Handling

| Return Code | Cause | Recommended Recovery |
|---|---|---|
| `ESP_OK` | Operation succeeded | None |
| `ESP_ERR_INVALID_ARG` | `NULL` pointer, invalid CS/RST GPIO, register > `0x3F`, buffer length `0`/`>64`, invalid UID size, or `spi_clock_hz > 10 MHz` | Validate caller inputs and target chip GPIO capabilities |
| `ESP_ERR_INVALID_SIZE` | Output buffer too small (e.g., `bjq_mfrc522_uid_to_hex()` buffer `< uid->size * 3`) or FIFO overflow | Provide at least `BJQ_MFRC522_UID_HEX_STR_MIN_LEN` (30 bytes) for UID strings |
| `ESP_ERR_INVALID_STATE` | `spi_bus_add_device()` called before `spi_bus_initialize()`, or unresolved RF collision | Initialize the SPI bus before `bjq_mfrc522_init()`; retry card poll |
| `ESP_ERR_TIMEOUT` | MFRC522 failed to exit soft reset (`PowerDown` bit stayed set), CRC timed out, or card left the RF field mid-transaction | Check 3.3 V power, SPI wiring (MOSI/MISO/SCLK/CS), and RST pin polarity; retry card read |
| `ESP_ERR_INVALID_CRC` | Anticollision BCC mismatch or SAK hardware CRC_A mismatch | Card moved out of field or RF noise; retry poll or adjust `bjq_mfrc522_set_rx_gain()` |
| `ESP_ERR_INVALID_RESPONSE` | Parity/protocol/buffer-overflow error in `ErrorReg`, malformed frame length, or unexpected reply to `HLTA` | Retry poll; verify SPI clock speed (`<= 4 MHz` recommended on breadboards) |
| `ESP_ERR_NO_MEM` | Heap allocation failed for driver context or SPI device descriptor | Inspect free heap (`esp_get_free_heap_size()`) |

## Common Hardware Checks

1. **`bjq_mfrc522_read_version()` returns `0x00` or `0xFF`:**
   - Expected values are `0x91` (MFRC522 v1.0), `0x92` (MFRC522 v2.0), or `0x88` (FM17522 clone).
   - `0x00` or `0xFF` indicates MOSI/MISO swap, disconnected CS/SCLK, missing 3.3 V power, or `RST` held low.
2. **Card never detected (`out_present == false`):**
   - Confirm `bjq_mfrc522_read_version()` returns a valid silicon ID first.
   - Verify the card is ISO/IEC 14443-3A (13.56 MHz, e.g., MIFARE Classic / Ultralight / NTAG / DESFire — 125 kHz EM4100 cards are not compatible).
   - Try increasing receiver gain with `bjq_mfrc522_set_rx_gain(handle, BJQ_MFRC522_RX_GAIN_48DB)`.

## Known Limitations

- **Current scope:** Covers SPI register I/O, transceiver control, and ISO/IEC 14443-3A card detection + UID reading (`REQA`, `WUPA`, Cascade Levels 1–3 anticollision/select, `HLTA`). MIFARE Classic Crypto1 authentication and block read/write commands are not implemented yet.
- **Bus interface:** SPI mode 0 only; I2C and UART interfaces of the MFRC522 chip are out of scope.
- **Hardware testing:** Wokwi-verified on `esp32s3` (`board-esp32-s3-devkitc-1` + `board-mfrc522`, see [bench-testing.md](bench-testing.md)) and build-verified in CI across `esp32`, `esp32s3`, `esp32c3`, and `esp32c6`; physical MFRC522 RF bench testing is pending.
