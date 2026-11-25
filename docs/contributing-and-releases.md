# Contributing & Release Process

This repository uses GitHub Actions for continuous integration and manual GitHub Releases for per-component versioning (`bjq_<name>/vX.Y.Z`).

## Scaffolding a New Component

```bash
python tools/new_component.py \
  --layer drivers \
  --name bjq_ads111x \
  --description "ESP-IDF v6.1 driver for ADS111x ADC"
```

## Automated CI Quality Gates

Every Pull Request and push to `main` runs `.github/workflows/ci.yml`:

1. **Format & Lint:** `pre-commit` (`clang-format` with ESP-IDF 4-space K&R style, `yamllint`, EOF/LF/whitespace hygiene, component validator).
2. **Component & Manifest Validation:** `tools/validate_components.py` verifies `bjq_` naming, `idf_component.yml` fields (`>=6.1.0,<7.0.0`, Apache-2.0, no `override_path`), one-way layer rules, and minimum documentation coverage.
3. **Strict Docs Check:** `doxygen Doxyfile` (warnings as errors) and `mkdocs build --strict`.
4. **ESP-IDF v6.1 Build Matrix:** `idf-build-apps` compiles `examples/` and `test_apps/` across `esp32`, `esp32s3`, `esp32c3`, and `esp32c6` inside `espressif/idf:v6.1`.

## Pre-Release Hardware & Wokwi Bench-Testing

Before cutting a component release, always confirm with the engineer whether to run bench-tests or skip (with a recorded reason), and explicitly choose available target chips (`Real HW`, `Wokwi`, `Both`, or `Build-only / Unavailable`). Serial COM ports are never auto-selected.

```bash
python tools/bench_test.py --component drivers/bjq_mfrc522
```

Curated, redacted serial monitor evidence and per-chip results are stored in each component's `docs/bench-testing.md` (raw logs remain local in gitignored `bench_logs/`).

## Manual GitHub Release Checklist

1. Release lower-layer components first if an upper-layer component depends on new APIs.
2. Run `python tools/bench_test.py --component <layer>/bjq_<name>` (or explicitly record a skip reason) to update `docs/bench-testing.md` and real hardware metrics.
3. In a PR to `main`, bump `version:` in `<layer>/bjq_<name>/idf_component.yml` and move `## [Unreleased]` items into a dated SemVer section in `CHANGELOG.md`.
4. Confirm green `CI Gate` on `main`.
5. Create a GitHub Release manually with tag `bjq_<name>/vX.Y.Z`, title `bjq_<name> vX.Y.Z`, changelog notes, and the per-chip bench-test traceability summary.
6. Update consuming products' `idf_component.yml` to pin the new tag and commit `dependencies.lock`.
