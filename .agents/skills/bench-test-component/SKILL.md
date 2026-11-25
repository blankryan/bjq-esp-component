---
name: bench-test-component
description: >-
  Guides pre-release hardware and Wokwi bench-testing for Bojaqu ESP-IDF v6.1.0 components,
  explicitly confirming skip/run decisions, per-chip availability, and COM ports with the
  engineer, capturing important serial monitor evidence, and updating docs/bench-testing.md
  and release traceability notes. Use before preparing or publishing any component release,
  or whenever the user asks to bench-test a component on real hardware or Wokwi.
---

# Pre-Release Component Bench-Testing Workflow

Follow this procedure whenever preparing a component release (`bjq_<name>/vX.Y.Z`) or running a hardware/Wokwi bench-test.

## Guardrails

- **Never assume** a release should be bench-tested or skipped — **ask the engineer first**.
- **Never assume** all target chips are available — **ask the engineer per target chip** (`Real HW`, `Wokwi`, `Both`, or `Build-only / Unavailable`).
- **Never auto-select a COM/serial port** — always require explicit COM port input and confirmation from the engineer before flashing or monitoring.
- **Never commit raw unredacted serial dumps** — raw logs stay in gitignored `bench_logs/`; commit only curated, redacted excerpts in `<component>/docs/bench-testing.md`.
- **Distinguish measured vs. estimated values** — only mark resource/timing claims as `Measured` when backed by an actual hardware or `idf.py size` run.

## Step-by-Step Procedure

### 1. Inspect Component Context
Read `<layer>/bjq_<name>/idf_component.yml`, `CHANGELOG.md`, and `docs/bench-testing.md` to identify:
- Current SemVer version and pending `[Unreleased]` changes.
- Declared `targets:` list (e.g., `esp32`, `esp32s3`, `esp32c3`, `esp32c6`).
- Available buildable apps (`examples/*` and `test_apps/`) and whether `wokwi.toml` + `diagram.json` exist.

### 2. Ask the Engineer (Mandatory Confirmation Gate)
Before running any build, flash, or release prep, ask the engineer:
1. **Run bench-test or skip?** If skipping, ask for the skip reason (e.g., docs-only release) and record it with:
   ```bash
   python tools/bench_test.py --record-skip --component <layer>/bjq_<name> --reason "<reason>"
   ```
   Then proceed directly to Step 6.
2. **Which app(s) to test?** (`examples/basic`, `test_apps`, or both).
3. **Per-chip availability & environment:** For each declared target chip, ask whether it will be tested on **Real Hardware**, **Wokwi**, **Both**, or recorded as **Build-only (board unavailable)**.
4. **Explicit COM port (only if Real Hardware selected):** List detected serial ports for reference if helpful, then ask the engineer for the exact COM port (e.g., `COM5`), baud rate (default `115200`), and board/PCB revision.

### 3. Execute Bench-Test & Capture Serial Output
- **Interactive CLI (preferred for human engineers):**
  ```bash
  python tools/bench_test.py --component <layer>/bjq_<name>
  ```
- **Real Hardware (guided commands when driven by agent after explicit COM confirmation):**
  1. Load ESP-IDF v6.1 (`export.ps1` / `export.sh`).
  2. Build and capture size summary: `idf.py set-target <target> build` and `idf.py size`.
  3. Flash the explicitly confirmed port: `idf.py -p <EXPLICIT_PORT> flash`.
  4. Capture live serial output into `bench_logs/<component>/<timestamp>_<target>_realhw.log`.
- **Wokwi Simulation:**
  - If `wokwi-cli` and `WOKWI_CLI_TOKEN` are available and `wokwi.toml` + `diagram.json` exist in the app directory: build for `<target>` and run `wokwi-cli <app_dir> --timeout 15000 --serial-log-file bench_logs/<component>/<timestamp>_<target>_wokwi.log`.
  - Otherwise, instruct the engineer to run **Wokwi: Start Simulator** in VS Code and save/paste the serial output.

### 4. Extract Important Evidence & Redact
Run the log extractor on each raw capture:
```bash
python tools/bench_test.py --extract-log bench_logs/<component>/<file>.log
```
Verify the curated evidence bundle covers:
- **Boot & hardware ID** (ESP-IDF version, target chip, peripheral silicon ID such as `VersionReg`).
- **Functional proof** (Unity `Tests / Failures / Ignored` summary or real peripheral transactions such as card UID/SAK detection).
- **Errors, warnings, or panics** (or explicit confirmation that none occurred).
- **Measured size/memory metrics** (`idf.py size` and runtime heap/stack lines if logged).
- **Redaction check:** Remove any Wi-Fi credentials, tokens, private endpoints, or sensitive real tag/device IDs before writing into documentation.

### 5. Update Component Documentation
1. Update `<layer>/bjq_<name>/docs/bench-testing.md` (using `tools/bench_test.py --record-result ...` or updating the verification matrix and session log directly) so every declared chip shows its exact status (`Real HW`, `Wokwi`, `Both`, or `Build-only` + reason).
2. If real hardware measurements were obtained, update `<layer>/bjq_<name>/docs/concurrency-and-resources.md` (upgrading estimates to `Measured` with target/board context) and record any observed wiring/clock/errata notes in `docs/troubleshooting.md`.
3. Update the `Testing` section of `<layer>/bjq_<name>/README.md` to match the verified hardware status.

### 6. Validate & Prepare Release Traceability
1. Run `python tools/validate_components.py` and confirm zero errors.
2. Include the per-chip bench-test summary in the release PR description and manual GitHub Release traceability block.
