# Bench Testing & Hardware Validation — `bjq_mfrc522`

This document tracks real-hardware and Wokwi verification results for `bjq_mfrc522`, including per-chip availability status, observed hardware behavior, measured resource usage, and curated serial monitor evidence.

## Test Environment Notes

- **Wokwi (`test_apps/`):** Starter `test_apps/wokwi.toml` and `test_apps/diagram.json` support running the Unity test app (`bjq_mfrc522_test_app`) on simulated ESP32 hardware for argument validation, SAK decoding, and UID formatting tests.
- **Real Hardware (`examples/basic` & `test_apps/`):** Required to verify live SPI Mode 0 timing, `VersionReg` silicon identification (`0x91`/`0x92`/`0x88`), RF antenna drive, and physical ISO/IEC 14443-3A card reads (unless a Wokwi Custom Chip is added in the future).
- **Run interactively before release:**
  ```bash
  python tools/bench_test.py --component drivers/bjq_mfrc522
  ```
  *(Serial COM ports are never auto-selected — the tool always requires explicit engineer confirmation.)*

## Verification Matrix

<!-- BENCH_TEST_MATRIX_START -->
| Target SoC | Environment | Board / Setup | App | Version / Commit | Date | Status | Notes |
|---|---|---|---|---|---|---|---|
| `esp32` | Build-only (CI) | N/A | `examples/ + test_apps/` | `0.1.0` (`db24208`) | 2026-11-25 | **BUILD-ONLY** | Reference skeleton release — hardware bench-test planned for SPI protocol release |
| `esp32s3` | Build-only (CI) | N/A | `examples/ + test_apps/` | `0.1.0` (`db24208`) | 2026-11-25 | **BUILD-ONLY** | Reference skeleton release — hardware bench-test planned for SPI protocol release |
| `esp32c3` | Build-only (CI) | N/A | `examples/ + test_apps/` | `0.1.0` (`db24208`) | 2026-11-25 | **BUILD-ONLY** | Reference skeleton release — hardware bench-test planned for SPI protocol release |
| `esp32c6` | Build-only (CI) | N/A | `examples/ + test_apps/` | `0.1.0` (`db24208`) | 2026-11-25 | **BUILD-ONLY** | Reference skeleton release — hardware bench-test planned for SPI protocol release |
<!-- BENCH_TEST_MATRIX_END -->

## Bench-Test Sessions

### 2026-11-25 — `v0.1.0` (`db24208`) — `all` (Skipped) — BUILD-ONLY

- **Board / Setup:** N/A
- **App Tested:** `examples/basic` + `test_apps` (GitHub Actions ESP-IDF v6.1 matrix)
- **Measured Size / Memory:** Not measured on hardware (`0.1.0` reference skeleton)
- **Engineer Notes:** Initial `0.1.0` skeleton release validated repository tooling, CI builds across all 4 targets, and Git-based ESP-IDF Component Manager consumption; RF protocol bench-testing will be captured before `0.2.0`/`1.0.0`.
