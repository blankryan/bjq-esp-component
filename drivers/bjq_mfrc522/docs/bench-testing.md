# Bench Testing & Hardware Validation — `bjq_mfrc522`

This document tracks real-hardware and Wokwi verification results for `bjq_mfrc522`, including per-chip availability status, observed hardware behavior, measured resource usage, and curated serial monitor evidence.

## Test Environment Notes

- **Wokwi (`test_apps/` & `examples/basic`):** Both apps include `wokwi.toml` (`build/flasher_args.json`) and `diagram.json` wired with `board-esp32-s3-devkitc-1` + `board-mfrc522` (`GPIO2=MISO`, `GPIO4=MOSI`, `GPIO5=SCK`, `GPIO6=SDA/CS`, `3.3V`, `GND`) to verify Unity unit tests, SPI2 register I/O, `VersionReg` probing, and simulated ISO/IEC 14443-3A card UID reads.
- **Real Hardware:** Recommended before `1.0.0` (or production deployment) to confirm physical RF range, antenna tuning (`bjq_mfrc522_set_rx_gain()`), 7-/10-byte cascade cards, and physical SPI signal integrity across PCB/breadboard setups.
- **Run interactively before release:**
  ```bash
  python tools/bench_test.py --component drivers/bjq_mfrc522
  ```
  *(Serial COM ports are never auto-selected — the tool always requires explicit engineer confirmation.)*

## Verification Matrix

<!-- BENCH_TEST_MATRIX_START -->
| Target SoC | Environment | Board / Setup | App | Version / Commit | Date | Status | Notes |
|---|---|---|---|---|---|---|---|
| `esp32` | Build-only (CI) | N/A | `examples/ + test_apps/` | `0.2.0` (`f0dc497`) | 2026-11-26 | **BUILD-ONLY** | Board unavailable on bench |
| `esp32s3` | Wokwi | `board-esp32-s3-devkitc-1` + `board-mfrc522` | `test_apps + examples/basic` | `0.2.0` (`f0dc497`) | 2026-11-26 | **PASS** | Unity 5/5 PASS; `VersionReg=0x92`; MIFARE Classic 1K UIDs `01:02:03:04` and `11:22:33:44` read cleanly over SPI2 @ 4 MHz |
| `esp32c3` | Build-only (CI) | N/A | `examples/ + test_apps/` | `0.2.0` (`f0dc497`) | 2026-11-26 | **BUILD-ONLY** | Board unavailable on bench |
| `esp32c6` | Build-only (CI) | N/A | `examples/ + test_apps/` | `0.2.0` (`f0dc497`) | 2026-11-26 | **BUILD-ONLY** | Board unavailable on bench |
<!-- BENCH_TEST_MATRIX_END -->

## Bench-Test Sessions

### 2026-11-26 — `v0.2.0` (`f0dc497`) — `esp32s3` (Wokwi) — PASS (`test_apps`)

- **Board / Setup:** Wokwi `board-esp32-s3-devkitc-1` + `board-mfrc522` (`MISO=GPIO2`, `MOSI=GPIO4`, `SCK=GPIO5`, `SDA/CS=GPIO6`, `3.3V`, `GND`)
- **App Tested:** `test_apps` (`bjq_mfrc522_test_app`, ESP-IDF `v6.1`)
- **Measured Size / Memory:** Total image `210,436` bytes (`Flash Code: 89,910 B`, `Flash Data: 46,964 B`, `DIRAM: 60,014 B`, `.data: 13,319 B`, `.bss: 2,872 B`); boot dynamic RAM available: `330 KiB + 21 KiB + 32 KiB DRAM`
- **Captured Errors/Warnings/Panics:** Only the 2 expected validation error logs asserted by Unity negative test cases (`Invalid CS GPIO: -1` and `SPI clock 20000000 Hz exceeds MFRC522 limit`); zero panics or unexpected errors
- **Engineer Notes:** All 5 Unity test cases passed (`5 Tests 0 Failures 0 Ignored`) and returned cleanly from `app_main()`.

```text
I (5) boot: ESP-IDF v6.1-dirty 2nd stage bootloader
E (250) bjq_mfrc522: Invalid CS GPIO: -1
./main/test_bjq_mfrc522.c:15:init rejects null and invalid arguments:PASS
E (270) bjq_mfrc522: SPI clock 20000000 Hz exceeds MFRC522 limit (10000000 Hz)
./main/test_bjq_mfrc522.c:33:init rejects spi clock above MFRC522 datasheet limit:PASS
./main/test_bjq_mfrc522.c:47:protocol and register APIs reject null handles and invalid arguments:PASS
./main/test_bjq_mfrc522.c:71:get_picc_type decodes standard ISO14443A SAK values:PASS
./main/test_bjq_mfrc522.c:84:uid_to_hex formats 4, 7, and 10 byte UIDs and enforces bounds:PASS
5 Tests 0 Failures 0 Ignored
```

### 2026-11-26 — `v0.2.0` (`f0dc497`) — `esp32s3` (Wokwi) — PASS (`examples/basic`)

- **Board / Setup:** Wokwi `board-esp32-s3-devkitc-1` + `board-mfrc522` (`MISO=GPIO2`, `MOSI=GPIO4`, `SCK=GPIO5`, `SDA/CS=GPIO6`, `3.3V`, `GND`, `RST=NC`)
- **App Tested:** `examples/basic` (`bjq_mfrc522_basic_example`, ESP-IDF `v6.1`)
- **Measured Size / Memory:** Total image `220,776` bytes (`Flash Code: 97,082 B`, `Flash Data: 49,672 B`, `DIRAM: 60,314 B`, `.data: 13,159 B`, `.bss: 2,712 B`); `libbjq_mfrc522.a` measured `.text: 4,371 B`, `.data: 0 B`, `.bss: 0 B`
- **Captured Errors/Warnings/Panics:** None
- **Engineer Notes:** Verified SPI2 initialization at 4 MHz, soft reset, `VersionReg=0x92` (MFRC522 v2.0), antenna enable, ISO/IEC 14443-3A `REQA` polling, anticollision + BCC + hardware `CRC_A` + `SELECT` + `HLTA`, and SAK decoding (`0x08` -> `MIFARE Classic 1K`) across two simulated cards (`01:02:03:04` and `11:22:33:44`). Note: Wokwi's `board-mfrc522` re-presents the simulated card while held in the simulator UI (~1.1–1.3 s interval). Physical hardware RF bench-testing remains planned for a future release.

```text
I (5) boot: ESP-IDF v6.1-dirty 2nd stage bootloader
I (244) bjq_mfrc522: Initialized bjq_mfrc522 on host 1 (CS=6, clock=4000000 Hz, timeout=100 ms)
I (244) bjq_mfrc522_example: MFRC522 VersionReg=0x92
I (6874) bjq_mfrc522_example: Card detected: UID=01:02:03:04 (4 bytes) SAK=0x08 Type=MIFARE Classic 1K
I (17284) bjq_mfrc522_example: Card detected: UID=11:22:33:44 (4 bytes) SAK=0x08 Type=MIFARE Classic 1K
I (24694) bjq_mfrc522_example: Card detected: UID=01:02:03:04 (4 bytes) SAK=0x08 Type=MIFARE Classic 1K
I (31824) bjq_mfrc522_example: Card detected: UID=11:22:33:44 (4 bytes) SAK=0x08 Type=MIFARE Classic 1K
```

### 2026-11-25 — `v0.1.0` (`db24208`) — `all` (Skipped) — BUILD-ONLY

- **Board / Setup:** N/A
- **App Tested:** `examples/basic` + `test_apps` (GitHub Actions ESP-IDF v6.1 matrix)
- **Measured Size / Memory:** Not measured on hardware (`0.1.0` reference skeleton)
- **Engineer Notes:** Initial `0.1.0` skeleton release validated repository tooling, CI builds across all 4 targets, and Git-based ESP-IDF Component Manager consumption.
