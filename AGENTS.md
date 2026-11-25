# Bojaqu ESP-IDF v6.1.0 Component Repository Rules

These rules apply to all engineering work and AI agent actions inside `bjq-esp-component`.

## Core Architecture & Coding Standards

- **Target framework:** ESP-IDF `>=6.1.0,<7.0.0` using modular peripheral drivers (`esp_driver_*`), never the legacy monolithic `driver` component.
- **Namespace:** Prefix component directories, headers, Kconfig symbols, C APIs, and release tags with `bjq_` (for example `drivers/bjq_mfrc522`, `CONFIG_BJQ_MFRC522_*`, `bjq_mfrc522_*`, `bjq_mfrc522/vX.Y.Z`).
- **One-way layers:** Dependencies may only point downward (`features` -> `platform` -> `drivers` -> `utilities`).
- **Manifest hygiene:** Never place `override_path` inside a component's own `idf_component.yml`; use `override_path` only inside `examples/` and `test_apps/`.
- **Formatting & validation:** Follow `.clang-format` (ESP-IDF 4-space K&R), Doxygen docstrings on all public headers, LF line endings, and run `python tools/validate_components.py` before opening PRs.

## Mandatory Pre-Release Hardware & Wokwi Bench-Testing Policy

Before preparing or tagging **any** component release (`bjq_<name>/vX.Y.Z`):

1. **Always ask the engineer first** whether this release should be bench-tested or skipped. Bench-testing is not mandatory for every release (for example, docs-only or metadata patches), but skipping **must** be explicitly confirmed by the engineer and recorded with a clear reason.
2. **Explicitly ask about target chip availability and test environment:** Not all supported chips (`esp32`, `esp32s3`, `esp32c3`, `esp32c6`) are always physically available at the same time. Ask the engineer for each chip whether to run on **Real Hardware**, **Wokwi**, **Both**, or record as **Build-only / Unavailable** with reason.
3. **Never auto-select a serial/COM port:** You may list detected COM ports for reference, but you must **always** obtain an explicit COM port choice and confirmation from the engineer before flashing or opening a serial monitor.
4. **Capture and document real hardware behavior:** Extract important serial monitor evidence (boot/hardware ID, functional/Unity results, errors/warnings/panics, measured size/memory, and observed hardware quirks), redact sensitive data, and record it in `<component>/docs/bench-testing.md` (and upgrade estimates to measured values in `docs/concurrency-and-resources.md` when measured). Keep raw full `.log` captures local under gitignored `bench_logs/`.
5. **Use the workflow skill:** Activate the workspace skill `bench-test-component` (`.agents/skills/bench-test-component/SKILL.md`) whenever planning, running, or documenting a pre-release bench-test.
