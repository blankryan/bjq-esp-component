# __COMPONENT_NAME__

__DESCRIPTION__

## Overview

- **Layer:** `__LAYER__`
- **Status:** Initial scaffold (`__VERSION__`) — replace with verified implementation details as features are completed.
- **Scope:** Reusable ESP-IDF component independent of product-specific logic.
- **Non-goals:** Product-specific pin assignments, provisioning, or application business logic.

## Compatibility

| Item | Supported |
|---|---|
| ESP-IDF | `>=6.1.0,<7.0.0` |
| Target SoCs | __TARGETS_MD__ |
| License | Apache-2.0 |

## Dependencies

- **ESP-IDF components:** `esp_common` (update `CMakeLists.txt` as modular `esp_driver_*` dependencies are added).
- **Intra-repo components:** None.
- **Hardware requirements:** Document required bus, voltage levels, and pull-ups here.

## Integration

Add `__COMPONENT_NAME__` to your product's `main/idf_component.yml`:

```yaml
dependencies:
  idf:
    version: ">=6.1.0,<7.0.0"
  __COMPONENT_NAME__:
    git: "https://github.com/blankryan/bjq-esp-component.git"
    path: "__LAYER__/__COMPONENT_NAME__"
    version: "__COMPONENT_NAME__/v__VERSION__"
```

Include the public header in your firmware:

```c
#include "__COMPONENT_NAME__.h"
```

## Documentation

- [Architecture](docs/architecture.md)
- [Configuration](docs/configuration.md)
- [Concurrency & Resources](docs/concurrency-and-resources.md)
- [Troubleshooting & Error Handling](docs/troubleshooting.md)
- [Changelog](CHANGELOG.md)

## Testing

- **Examples:** `examples/basic` (compiled in CI across all supported targets).
- **Test App:** `test_apps/` (Unity test app compiled in CI across all supported targets).
- **Hardware verification:** TBD — record tested devkit/PCB revision and verification steps once hardware testing is performed.

## License

Licensed under the [Apache License 2.0](LICENSE).
