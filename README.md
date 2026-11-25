# **BOJAQU COMPONENTS** — Reusable ESP-IDF v6.1.0 Components

`bjq-esp-component` is the shared ESP-IDF v6.1.0 component monorepo for Bojaqu open-source and multi-product firmware. Components are organized by technical responsibility rather than by product so they can be independently versioned, documented, validated in CI, and consumed via the ESP-IDF Component Manager.

- **Repository:** <https://github.com/blankryan/bjq-esp-component>
- **Documentation Portal:** <https://blankryan.github.io/bjq-esp-component/>
- **License:** Apache-2.0 ([LICENSE](LICENSE))
- **Target Framework:** ESP-IDF `>=6.1.0,<7.0.0`
- **Default CI Target SoCs:** `esp32`, `esp32s3`, `esp32c3`, `esp32c6`

---

## 1. Purpose

This repository defines the engineering, documentation, and continuous integration architecture for sharing ESP-IDF firmware components across multiple embedded and IoT products:

1. Standardize how teams document, build, validate, release, and maintain components.
2. Keep reusable ESP-IDF components decoupled from product-specific firmware.
3. Preserve implementation knowledge alongside the source code of each component.
4. Make every component release traceable to its source commit, ESP-IDF version, target chips, dependencies, configuration, and CI validation status.

---

## 2. Architectural Decisions

| Area | Decision |
|---|---|
| Primary consumption | Git-based ESP-IDF Component Manager (`idf_component.yml` with `git:` + `path:` + pinned tag) |
| Dependency reproducibility | Consuming products commit generated `dependencies.lock` |
| Source control & CI/CD | GitHub (GitHub Actions + GitHub Pages) |
| Visibility & license | Public open-source repository under Apache-2.0 |
| Component versioning | Per-component independent SemVer tags (`bjq_<name>/vX.Y.Z`) and independent `CHANGELOG.md` |
| Naming & namespacing | Full `bjq_` prefix on component directories, headers, Kconfig symbols, C APIs, and release tags |
| ESP-IDF compatibility | `>=6.1.0,<7.0.0` using ESP-IDF v6.x modular drivers (`esp_driver_*`) |
| Default CI target matrix | `esp32`, `esp32s3`, `esp32c3`, `esp32c6` |
| Intra-repo dependencies | Explicit `git:` + `path:` + pinned tag in component `idf_component.yml`; `override_path` only in `examples/` and `test_apps/` |
| Documentation & portal | Markdown + Mermaid diagrams stored in each component, aggregated via MkDocs Material + Doxygen HTML on GitHub Pages |
| Code formatting | ESP-IDF 4-space K&R C style (`.clang-format`), enforced via `pre-commit` and CI |
| Release workflow | Manual GitHub Releases after green CI on `main` (see [CONTRIBUTING.md](CONTRIBUTING.md)) |

---

## 3. Core Principles

### 3.1 Document implementation, not only intention

Documentation must accurately describe implemented and verified behavior. Clearly distinguish:

- **Current behavior:** verified in the current implementation of the component.
- **Design intent:** planned or desired behavior that is not yet implemented.
- **Known limitation:** behavior or hardware condition that is not currently supported.
- **Deprecated behavior:** behavior retained for compatibility but no longer recommended.

### 3.2 Separate reusable component behavior from product integration

A shared component documents its reusable APIs, internal behavior, configuration, resource usage, and supported framework/target versions once.

A consuming product documents how it configures and integrates the component (pinout, credentials/provisioning, product event handling, and operational behavior) without duplicating the component manual.

### 3.3 Make releases reproducible and traceable

Every component release (`bjq_<name>/vX.Y.Z`) must be traceable to:

- Repository URL and Git commit SHA.
- Component SemVer version (`idf_component.yml` and `CHANGELOG.md`).
- ESP-IDF version (`6.1.0`) and validated target chip set.
- Resolved component dependencies (`dependencies.lock` in consuming products).
- Relevant Kconfig/runtime configuration inputs and hardware compatibility notes.
- CI build and validation results.

### 3.4 Keep documentation maintainable

Use scaffolding templates (`tools/new_component.py`) and automated validation (`tools/validate_components.py`, `mkdocs build --strict`, `doxygen`) so documentation never drifts from source code.

---

## 4. Component Layers & Dependency Rules

Components are organized into four strict layers. Dependencies may only point **downward** (`features` -> `platform` -> `drivers` -> `utilities`):

| Layer | Directory | Responsibility | Examples |
|---|---|---|---|
| Utilities | `utilities/` | Small reusable support libraries (no higher-layer deps) | `bjq_json_utils`, `bjq_time_utils`, `bjq_sys_monitor` |
| Drivers | `drivers/` | Single device, chip, bus, or protocol driver | `bjq_mfrc522`, `bjq_ads111x`, `bjq_ds18b20` |
| Platform | `platform/` | Cross-cutting infrastructure and system services | `bjq_net_mgr`, `bjq_mqtt_mgr`, `bjq_ota_mgr`, `bjq_cfg_mgr` |
| Features | `features/` | Reusable product-level capabilities | `bjq_sensor_mgr`, `bjq_relay_mgr`, `bjq_scheduler` |

---

## 5. Consuming a Component in an ESP-IDF v6.1 Project

Add the desired component to your product's `main/idf_component.yml`, pinning a released tag:

```yaml
dependencies:
  idf:
    version: ">=6.1.0,<7.0.0"
  bjq_mfrc522:
    git: "https://github.com/blankryan/bjq-esp-component.git"
    path: "drivers/bjq_mfrc522"
    version: "bjq_mfrc522/v0.1.0"
```

Run `idf.py reconfigure` (or `idf.py build`) and commit the resulting `dependencies.lock` file in your product repository.

---

## 6. Component Structure & Documentation Requirements

### 6.1 Standard Component Layout

```text
<layer>/bjq_<name>/
├── CMakeLists.txt
├── Kconfig
├── idf_component.yml
├── LICENSE
├── README.md
├── CHANGELOG.md
├── include/
│   └── bjq_<name>.h
├── src/
│   └── bjq_<name>.c
├── docs/
│   ├── architecture.md
│   ├── configuration.md
│   ├── concurrency-and-resources.md
│   ├── troubleshooting.md
│   └── bench-testing.md
├── examples/
│   └── basic/
└── test_apps/
    └── main/
```

### 6.2 Minimum Component Documentation Requirements

Every component is checked by `tools/validate_components.py` for coverage of the following areas:

| Document area | Required information |
|---|---|
| Overview | Purpose, scope, non-goals, supported use cases, current status |
| Architecture | Internal modules, event flow, state machine / sequence diagrams (Mermaid) |
| API reference | Doxygen-documented public functions, types, ownership/lifetime rules, return values |
| Configuration | Kconfig options (`CONFIG_BJQ_<NAME>_*`), defaults, dependencies, runtime config |
| Dependencies | ESP-IDF v6.1 modular components (`esp_driver_*`), intra-repo components, hardware requirements |
| Resource usage | Stack, heap, static buffers, queues, task priorities (explicitly distinguishing measured vs. estimated values) |
| Concurrency | Mutexes, queues, callbacks, thread-safety, and ISR constraints |
| Error handling | Error codes, retry policy, recovery behavior, known limitations |
| Integration | Copy-pasteable `idf_component.yml` snippet and initialization example |
| Testing & compatibility | Tested ESP-IDF versions, supported target chips, manual/automated test procedure |
| Release history | SemVer entries in `CHANGELOG.md` with API changes, fixes, and migration notes |

Resource usage must distinguish **measured** values from **estimates**. Do not publish unsupported numeric claims.

---

## 7. Quickstart for Maintainers

### Create a new component

```bash
python tools/new_component.py --layer drivers --name bjq_ads111x --description "ESP-IDF driver for ADS111x ADC"
```

### Validate all components locally

```bash
python tools/validate_components.py
```

### Run a pre-release hardware / Wokwi bench-test

```bash
python tools/bench_test.py --component drivers/bjq_mfrc522
```

### Build an example or test app with ESP-IDF v6.1

```powershell
# Load ESP-IDF v6.1 environment first (e.g. C:\esp\v6.1\esp-idf\export.ps1)
cd drivers\bjq_mfrc522\examples\basic
idf.py set-target esp32
idf.py build
```

See [CONTRIBUTING.md](CONTRIBUTING.md) for the full PR workflow, coding standards, and manual GitHub Release checklist.
