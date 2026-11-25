# Concurrency & Resource Usage — `__COMPONENT_NAME__`

## Concurrency & Thread Safety

- **Task model:** Does not spawn background FreeRTOS tasks in the current scaffold.
- **Thread safety:** Handle operations are **not** internally mutex-protected unless explicitly documented; callers must synchronize concurrent access to the same handle.
- **ISR constraints:** All public APIs must be called from task context only (not ISR-safe).

## Resource Usage

> Values below distinguish **measured** measurements from **estimates**. Do not rely on unmeasured estimates for safety margins.

| Resource | Value | Classification | Notes |
|---|---|---|---|
| Heap per instance | `sizeof(struct __COMPONENT_NAME___ctx)` | Estimated (compile-time struct size) | Allocated in `__COMPONENT_NAME___init()`, freed in `__COMPONENT_NAME___deinit()`. |
| Static RAM | Negligible (`TAG` string) | Estimated | Update with `.map` measurements after full implementation. |
| Background tasks / queues | `0` | Current behavior | None created. |
