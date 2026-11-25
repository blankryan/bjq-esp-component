# Concurrency & Resource Usage — `bjq_mfrc522`

## Concurrency & Thread Safety

- **Task model:** `bjq_mfrc522` creates no background FreeRTOS tasks or timers.
- **Thread safety:** Operations on a single `bjq_mfrc522_handle_t` are **not** internally mutex-protected. Callers sharing one handle across multiple tasks must serialize access externally. Multiple distinct handles on the same SPI host bus are serialized by ESP-IDF's `esp_driver_spi`.
- **ISR constraints:** No API function in `bjq_mfrc522.h` is ISR-safe. Call all APIs from task context only.

## Resource Usage

> Values below explicitly distinguish **measured** values from **estimates**. Do not treat estimates as measured worst-case budgets.

| Resource | Value | Classification | Notes |
|---|---|---|---|
| Driver context heap per handle | `sizeof(struct bjq_mfrc522_ctx)` (~32 bytes) | Estimated (struct size only) | Excludes internal `spi_device_handle_t` allocation inside `esp_driver_spi`. |
| Static RAM | Negligible (`TAG` string literal) | Estimated | Update with measured `.map` output after protocol implementation. |
| FreeRTOS tasks / queues | `0` | Current behavior | Synchronous caller-driven API. |
| Hardware peripherals | 1 SPI device slot + 1 CS GPIO (+ optional 1 RST GPIO) | Current behavior | SPI host bus is shared and owned by the caller. |
