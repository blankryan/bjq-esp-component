# Configuration — `bjq_mfrc522`

## Kconfig Options

| Option | Type | Default | Range | Description |
|---|---|---|---|---|
| `CONFIG_BJQ_MFRC522_DEFAULT_SPI_CLOCK_HZ` | `int` | `4000000` | `100000..10000000` | Default SPI clock speed in Hz when `config->spi_clock_hz` is `0`. |
| `CONFIG_BJQ_MFRC522_DEFAULT_TIMEOUT_MS` | `int` | `100` | `1..5000` | Default operation timeout in milliseconds when `config->timeout_ms` is `0`. |

## Runtime Configuration (`bjq_mfrc522_config_t`)

| Field | Required | Notes |
|---|---|---|
| `spi_host` | Yes | Must reference an already initialized ESP-IDF SPI host (for example `SPI2_HOST`). |
| `cs_io_num` | Yes | Must pass `GPIO_IS_VALID_OUTPUT_GPIO()`. |
| `rst_io_num` | No | Set to `GPIO_NUM_NC` if hardware reset is not connected to an MCU GPIO. |
| `spi_clock_hz` | No | Set to `0` to inherit `CONFIG_BJQ_MFRC522_DEFAULT_SPI_CLOCK_HZ`. Maximum allowed is `10000000` Hz. |
| `timeout_ms` | No | Set to `0` to inherit `CONFIG_BJQ_MFRC522_DEFAULT_TIMEOUT_MS`. |
