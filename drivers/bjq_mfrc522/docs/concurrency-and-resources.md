# Concurrency & Resource Usage — `bjq_mfrc522`

## Concurrency & Thread Safety

- **Task model:** `bjq_mfrc522` creates no background FreeRTOS tasks or timers. All SPI transfers and RF polling run synchronously in the caller's task context.
- **Blocking behavior:** SPI transfers use `spi_device_transmit()` (blocking/yielding cleanly to FreeRTOS). RF transceive waits poll `ComIrqReg` with `vTaskDelay(1)` between checks; CRC_A waits use 100 us `esp_rom_delay_us()` intervals because hardware CRC completes in tens of microseconds.
- **Thread safety:** Operations on a single `bjq_mfrc522_handle_t` are **not** internally mutex-protected. Callers sharing one handle across multiple tasks must serialize access externally. Multiple distinct handles on the same SPI host bus are serialized automatically by ESP-IDF's `esp_driver_spi`.
- **ISR constraints:** No API function in `bjq_mfrc522.h` is ISR-safe (except pure stateless helpers `bjq_mfrc522_get_picc_type()` and `bjq_mfrc522_picc_type_name()`). Call all handle-based APIs from task context only.

## Resource Usage

> Values below explicitly distinguish **measured** values from **estimates**. Do not treat estimates as measured worst-case budgets.

| Resource | Value | Classification | Notes |
|---|---|---|---|
| Driver context heap per handle | `sizeof(struct bjq_mfrc522_ctx)` (~32 bytes) | Estimated (compile-time struct size) | Allocated once in `bjq_mfrc522_init()`, freed in `bjq_mfrc522_deinit()`. Excludes `spi_device_handle_t` internal allocation inside `esp_driver_spi`. |
| Dynamic heap during polling / UID read | `0` bytes | Current behavior | All register, FIFO burst, CRC, and anticollision buffers are fixed-size stack arrays. |
| Peak stack usage in driver calls | ~256–384 bytes (65-byte TX + 65-byte RX burst buffers + local frames) | Estimated | Update with FreeRTOS high-water-mark measurements during hardware bench testing. |
| Static RAM | Negligible (`TAG` and PICC name string literals in `.rodata`) | Estimated | Update with `.map` measurements after hardware validation. |
| FreeRTOS tasks / queues | `0` | Current behavior | Synchronous caller-driven API. |
| Hardware peripherals | 1 SPI device slot + 1 CS GPIO (+ optional 1 RST GPIO) | Current behavior | SPI host bus is shared and owned by the caller. |
