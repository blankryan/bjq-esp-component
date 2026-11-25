## Summary

- **Affected component(s):** `drivers/bjq_<name>` (or repo tooling/CI/docs)
- **Change type:** `feat` / `fix` / `docs` / `refactor` / `ci` / `chore`
- **Description:** Briefly explain what changed and why.

## Checklist

- [ ] Component follows `bjq_` naming convention (directory, header, Kconfig, C symbols)
- [ ] `CMakeLists.txt` uses ESP-IDF v6.1 modular drivers (`esp_driver_*`) with minimal `REQUIRES`
- [ ] `idf_component.yml` targets `idf: ">=6.1.0,<7.0.0"` and contains **no** `override_path`
- [ ] Layer dependency direction is respected (`features` -> `platform` -> `drivers` -> `utilities`)
- [ ] Code formatted with `.clang-format` (ESP-IDF 4-space K&R style)
- [ ] Public headers have complete Doxygen documentation
- [ ] Component `README.md`, `docs/*.md` (including `docs/bench-testing.md` for release PRs), and `CHANGELOG.md` (`[Unreleased]`) are updated
- [ ] `python tools/validate_components.py` passes locally
- [ ] `examples/` and `test_apps/` build cleanly under ESP-IDF v6.1

## Testing & Verification Notes

- **ESP-IDF Version:** `6.1.0`
- **Targets built locally:** (e.g., `esp32`, `esp32s3`, `esp32c3`, `esp32c6`)
- **Bench-test decision (for releases):** Confirmed with engineer — `Ran (Real HW / Wokwi / Both)` or `Skipped (reason: ...)`
- **Per-chip bench-test summary:** (e.g., `esp32s3: Real HW PASS (DevKitC + MFRC522); esp32: Wokwi PASS; esp32c3/esp32c6: Build-only (boards unavailable)`)
