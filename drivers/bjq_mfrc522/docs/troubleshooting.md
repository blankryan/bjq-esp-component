# Troubleshooting & Error Handling — `bjq_mfrc522`

## Error Handling

| Return Code | Cause | Recommended Recovery |
|---|---|---|
| `ESP_OK` | Operation succeeded | None |
| `ESP_ERR_INVALID_ARG` | `NULL` pointer, invalid CS/RST GPIO, or `spi_clock_hz > 10 MHz` | Check `bjq_mfrc522_config_t` fields and target chip GPIO capabilities |
| `ESP_ERR_INVALID_STATE` | `spi_bus_add_device()` called before `spi_bus_initialize()` | Initialize the SPI host bus prior to calling `bjq_mfrc522_init()` |
| `ESP_ERR_NO_MEM` | Heap allocation failed for driver context or SPI device descriptor | Inspect heap availability and SPI device slots |

## Known Limitations

- **Current behavior:** Only driver lifecycle and SPI/GPIO attachment are implemented in `0.1.0`; MFRC522 RF register transactions and card UID polling are not yet implemented.
- **Bus mode:** Only SPI mode 0 is planned; I2C and UART interfaces of the MFRC522 chip are out of scope for `0.1.0`.
- **Hardware testing:** Not yet verified on physical MFRC522 hardware in CI.
