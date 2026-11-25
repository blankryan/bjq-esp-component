# Contributing & Release Guide

Thank you for contributing to `bjq-esp-component`. This guide covers component authoring, local validation, CI expectations, and the manual release process.

---

## 1. Repository Rules

1. **ESP-IDF Version:** All components target ESP-IDF `>=6.1.0,<7.0.0` and use v6.x modular peripheral drivers (`esp_driver_spi`, `esp_driver_gpio`, `esp_driver_i2c`, etc.) instead of legacy monolithic drivers.
2. **Naming Convention (`bjq_` prefix everywhere):**
   - Directory & CMake component name: `bjq_<name>` (lowercase snake_case)
   - Public header: `include/bjq_<name>.h`
   - Public C symbols & types: `bjq_<name>_*`
   - Kconfig symbols: `CONFIG_BJQ_<NAME>_*`
   - Release Git tags: `bjq_<name>/vX.Y.Z`
3. **One-Way Layer Dependencies:**
   - Allowed dependency direction: `features` -> `platform` -> `drivers` -> `utilities`.
   - Upward or cyclic dependencies between layers are forbidden and fail `tools/validate_components.py`.
4. **Intra-Repo Dependencies:**
   - In a component's own `idf_component.yml`, always reference another component from this repo using `git: "https://github.com/blankryan/bjq-esp-component.git"`, `path: "<layer>/bjq_<dep>"`, and a pinned release tag (`version: "bjq_<dep>/vX.Y.Z"`).
   - **Never** use `override_path` inside a component's own `idf_component.yml`.
   - Use `override_path` inside `examples/*/main/idf_component.yml` and `test_apps/main/idf_component.yml` so local changes are built and verified in PR CI before tagging.
5. **Formatting & Documentation:**
   - C/H files must follow `.clang-format` (ESP-IDF 4-space K&R style).
   - All public APIs in `include/*.h` must have clean Doxygen docstrings (`doxygen Doxyfile` runs with warnings treated as errors in CI).
   - Resource numbers must clearly state whether they are **measured** or **estimated** (or `TBD — not yet measured`).

---

## 2. Scaffolding a New Component

Use the generator script to create a compliant component skeleton:

```bash
python tools/new_component.py \
  --layer drivers \
  --name bjq_my_sensor \
  --description "ESP-IDF driver for My Sensor"
```

Optional flags:
- `--targets esp32 esp32s3 esp32c3 esp32c6` (defaults to all 4 core targets)
- `--version 0.1.0`

---

## 3. Local Verification Before Opening a PR

1. **Run the component validator:**
   ```bash
   python tools/validate_components.py
   ```
2. **Run pre-commit hooks (formatting, YAML, hygiene):**
   ```bash
   pre-commit install
   pre-commit run --all-files
   ```
3. **Build the component's example and test app under ESP-IDF v6.1:**
   ```bash
   cd <layer>/bjq_<name>/examples/basic
   idf.py set-target esp32
   idf.py build

   cd ../../test_apps
   idf.py set-target esp32
   idf.py build
   ```
4. **Preview the documentation site locally (optional):**
   ```bash
   doxygen Doxyfile
   mkdocs build --strict
   mkdocs serve
   ```

---

## 4. Pull Request Workflow

1. Create a feature branch from `main` (e.g., `feat/bjq_mfrc522-spi-init` or `fix/bjq_mfrc522-timeout`).
2. Keep changes focused on one component (or a clearly linked stack of dependent changes).
3. Record user-visible changes under `## [Unreleased]` in `<layer>/bjq_<name>/CHANGELOG.md`.
4. Open a Pull Request against `main` and complete the PR template checklist.
5. Ensure all GitHub Actions checks pass (`formatting`, `component validation`, `strict docs`, and `ESP-IDF v6.1 build matrix`).

---

## 5. Pre-Release Hardware & Wokwi Bench-Testing

Before preparing a release PR, always confirm bench-testing with the engineer:

1. **Ask first whether to bench-test or skip:** Bench-testing is not required for every release (e.g., docs-only or metadata patches), but skipping must be explicitly approved and recorded with a reason in `<layer>/bjq_<name>/docs/bench-testing.md`.
2. **Choose per-chip environment (`Real HW`, `Wokwi`, `Both`, or `Build-only / Unavailable`):** Because not every target chip (`esp32`, `esp32s3`, `esp32c3`, `esp32c6`) is always available on the bench simultaneously, explicitly select what is available and record the status for every declared chip.
3. **Explicit COM port confirmation:** When running on real hardware, `tools/bench_test.py` displays detected serial ports for reference only and always requires explicit COM port selection and confirmation from the engineer before flashing or monitoring.
4. **Capture and curate important serial evidence:**
   - Run the interactive helper:
     ```bash
     python tools/bench_test.py --component <layer>/bjq_<name>
     ```
   - Or extract curated evidence from an existing log file (from `idf.py monitor` or VS Code Wokwi):
     ```bash
     python tools/bench_test.py --extract-log bench_logs/<component>/<session>.log
     ```
   - Raw full logs stay local in gitignored `bench_logs/`. Commit only curated, redacted serial excerpts (boot/HW ID, functional/Unity results, errors/warnings/panics, measured size/memory, and engineer notes) into `<layer>/bjq_<name>/docs/bench-testing.md`, and upgrade estimates to measured values in `docs/concurrency-and-resources.md` when real measurements are captured.

---

## 6. Manual GitHub Release Checklist

Releases are per-component and published manually on GitHub after validation on `main`.

1. **Check dependency order (bottom-up):**
   - If this release requires newly added APIs in a lower-layer component (e.g., `utilities/bjq_time_utils`), release and tag that lower-layer component first, then update this component's `idf_component.yml` to pin the new tag.
2. **Run or explicitly record bench-test decision (Section 5):**
   - Confirm with the engineer whether to bench-test on Real Hardware, Wokwi, Both, or skip (with reason), and update `<layer>/bjq_<name>/docs/bench-testing.md` (plus `docs/concurrency-and-resources.md` / `docs/troubleshooting.md` for measured numbers and hardware quirks).
3. **Prepare a release PR to `main`:**
   - Bump `version:` in `<layer>/bjq_<name>/idf_component.yml` using Semantic Versioning (`MAJOR.MINOR.PATCH`).
   - Move items from `## [Unreleased]` into `## [X.Y.Z] - YYYY-MM-DD` in `<layer>/bjq_<name>/CHANGELOG.md` (include migration notes for breaking changes).
   - Update integration snippets and testing status in `<layer>/bjq_<name>/README.md`.
   - Verify documentation distinguishes current behavior, design intent, and known limitations.
4. **Merge and confirm green CI on `main`:**
   - Wait for `.github/workflows/ci.yml` (`CI Gate`) and `.github/workflows/docs.yml` to succeed on the merged `main` commit.
5. **Create the Git tag and GitHub Release manually:**
   - Open GitHub -> **Releases** -> **Draft a new release**.
   - Tag: `bjq_<name>/vX.Y.Z` targeting the validated `main` commit.
   - Title: `bjq_<name> vX.Y.Z`.
   - Body: paste the `CHANGELOG.md` entry plus traceability metadata (generated automatically by `tools/bench_test.py`):
     ```markdown
     ### Release Traceability
     - **Component:** `bjq_<name>` (`<layer>/bjq_<name>`)
     - **Version:** `X.Y.Z`
     - **Source Commit:** `<commit-sha>`
     - **ESP-IDF Version:** `6.1.0` (`>=6.1.0,<7.0.0`)
     - **Bench-Test Summary:**
       - `esp32`: Real HW — **PASS** (<board/notes>)
       - `esp32s3`: Wokwi — **PASS** (<notes>)
       - `esp32c3`: Build-only (CI) — **BUILD-ONLY** (board unavailable)
       - `esp32c6`: Build-only (CI) — **BUILD-ONLY** (board unavailable)
     ```
6. **Adopt in consuming products:**
   - Update `version: "bjq_<name>/vX.Y.Z"` in the product's `idf_component.yml`, run `idf.py reconfigure`, verify build/tests, and commit `dependencies.lock`.
