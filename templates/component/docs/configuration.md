# Configuration — `__COMPONENT_NAME__`

## Kconfig Options

| Option | Type | Default | Range | Description |
|---|---|---|---|---|
| `CONFIG___COMPONENT_UPPER___DEFAULT_TIMEOUT_MS` | `int` | `100` | `1..60000` | Default operation timeout in milliseconds when runtime `timeout_ms` is `0`. |

## Runtime Configuration

Pass a `__COMPONENT_NAME___config_t` structure to `__COMPONENT_NAME___init()`:

- `timeout_ms`: Overrides `CONFIG___COMPONENT_UPPER___DEFAULT_TIMEOUT_MS` when non-zero.
