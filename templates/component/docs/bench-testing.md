# Bench Testing & Hardware Validation — `__COMPONENT_NAME__`

This document records verified runtime behavior on **Real Hardware** and/or **Wokwi** simulation before component releases, alongside curated serial monitor evidence.

## Pre-Release Policy

- Before every release, the engineer explicitly decides whether to run a bench-test or skip it (with a documented justification).
- Target chip coverage is tracked **per chip** (`Real HW`, `Wokwi`, `Both`, or `Build-only` with reason when a devkit is unavailable).
- Raw full serial monitor logs are stored locally under gitignored `bench_logs/`. Only curated, redacted excerpts for important matters (boot/hardware ID, functional proof, errors/warnings/panics, and measured memory/size) are committed here.
- Run the interactive helper before releasing:
  ```bash
  python tools/bench_test.py --component __LAYER__/__COMPONENT_NAME__
  ```

## Verification Matrix

<!-- BENCH_TEST_MATRIX_START -->
| Target SoC | Environment | Board / Setup | App | Version / Commit | Date | Status | Notes |
|---|---|---|---|---|---|---|---|
__TARGETS_BENCH_ROWS__
<!-- BENCH_TEST_MATRIX_END -->

## Bench-Test Sessions

No bench-test sessions recorded yet. Run `python tools/bench_test.py --component __LAYER__/__COMPONENT_NAME__` to capture a Wokwi or real-hardware session.
