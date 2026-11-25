# Configuration — `bjq_mfrc522`

## Kconfig Options

| Option | Type | Default | Range | Description |
|---|---|---|---|---|
| `CONFIG_BJQ_MFRC522_DEFAULT_SPI_CLOCK_HZ` | `int` | `4000000` | `100000..10000000` | Default SPI clock speed in Hz when `config->spi_clock_hz` is `0`. |
| `CONFIG_BJQ_MFRC522_DEFAULT_TIMEOUT_MS` | `int` | `100` | `1..5000` | Default host polling timeout in milliseconds for reset, CRC, and RF transceive waits when `config->timeout_ms` is `0`. |
| `CONFIG_BJQ_MFRC522_DEFAULT_RX_GAIN` | `int` | `4` | `0..7` | Default `RFCfgReg` receiver gain index applied during `bjq_mfrc522_reset()` (`4` = 33 dB datasheet default, `7` = 48 dB max). |

## Runtime Configuration (`bjq_mfrc522_config_t`)

| Field | Required | Notes |
|---|---|---|
| `spi_host` | Yes | Must reference an already initialized ESP-IDF SPI host (for example `SPI2_HOST`). |
| `cs_io_num` | Yes | Must pass `GPIO_IS_VALID_OUTPUT_GPIO()`. |
| `rst_io_num` | No | Set to `GPIO_NUM_NC` if hardware reset is tied high; when wired, `bjq_mfrc522_reset()` drives a 10 us low pulse followed by a 50 ms settle delay. |
| `spi_clock_hz` | No | Set to `0` to inherit `CONFIG_BJQ_MFRC522_DEFAULT_SPI_CLOCK_HZ`. Maximum allowed is `10000000` Hz. |
| `timeout_ms` | No | Set to `0` to inherit `CONFIG_BJQ_MFRC522_DEFAULT_TIMEOUT_MS`. |

## Dynamic RF Controls

- **Antenna field:** Call `bjq_mfrc522_set_antenna(handle, true/false)` to enable or disable TX1/TX2 RF output for power saving or interference control.
- **Receiver gain:** Call `bjq_mfrc522_set_rx_gain(handle, gain)` with `BJQ_MFRC522_RX_GAIN_18DB` through `BJQ_MFRC522_RX_GAIN_48DB` to tune read distance for a specific antenna PCB.
