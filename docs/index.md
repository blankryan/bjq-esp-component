# Bojaqu ESP-IDF v6.1.0 Components

Welcome to the documentation portal for **`bjq-esp-component`** — the shared, Apache-2.0 licensed ESP-IDF v6.1.0 component repository for Bojaqu embedded and IoT products.

## Architectural Layers

Components are organized into four strict layers with one-way downward dependencies:

```mermaid
flowchart TD
    PROD["Consuming Product Firmware"] --> FEAT["features/ (Reusable Product Capabilities)"]
    PROD --> PLAT["platform/ (Cross-Cutting System Services)"]
    PROD --> DRV["drivers/ (Device, Chip & Bus Drivers)"]
    PROD --> UTIL["utilities/ (Pure Support Libraries)"]

    FEAT --> PLAT
    FEAT --> DRV
    FEAT --> UTIL
    PLAT --> DRV
    PLAT --> UTIL
    DRV --> UTIL
```

## Quick Links

- [Component Catalog](components/index.md) — browse all components, versions, and target SoCs
- [Consuming Components](consuming-components.md) — add `bjq_*` components to your ESP-IDF v6.1 project
- [C API Reference (Doxygen)](api-reference.md) — generated public C header documentation
- [Contributing & Releases](contributing-and-releases.md) — scaffold components, run validation, and cut releases
