#!/usr/bin/env python3
"""Validate all Bojaqu ESP-IDF components against repository standards."""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path
from typing import Any

import yaml

REPO_ROOT = Path(__file__).resolve().parent.parent
REPO_URL = "https://github.com/blankryan/bjq-esp-component.git"
LAYERS = ("utilities", "drivers", "platform", "features")
LAYER_RANK = {layer: idx for idx, layer in enumerate(LAYERS)}
VALID_TARGETS = {
    "esp32",
    "esp32s2",
    "esp32s3",
    "esp32c2",
    "esp32c3",
    "esp32c5",
    "esp32c6",
    "esp32c61",
    "esp32h2",
    "esp32p4",
}
COMPONENT_NAME_RE = re.compile(r"^bjq_[a-z][a-z0-9_]*$")
SEMVER_RE = re.compile(r"^\d+\.\d+\.\d+(?:-[0-9A-Za-z.-]+)?$")
TAG_VERSION_RE = re.compile(r"^(bjq_[a-z][a-z0-9_]*)/v\d+\.\d+\.\d+(?:-[0-9A-Za-z.-]+)?$")
KCONFIG_SYMBOL_RE = re.compile(r"^\s*(?:menu)?config\s+([A-Z0-9_]+)\b", re.MULTILINE)

REQUIRED_DOCS = (
    "docs/architecture.md",
    "docs/configuration.md",
    "docs/concurrency-and-resources.md",
    "docs/troubleshooting.md",
    "docs/bench-testing.md",
)


def discover_components(repo_root: Path) -> list[tuple[str, Path]]:
    """Return sorted (layer, component_dir) pairs across all architectural layers."""
    found: list[tuple[str, Path]] = []
    for layer in LAYERS:
        layer_dir = repo_root / layer
        if not layer_dir.is_dir():
            continue
        for child in sorted(layer_dir.iterdir()):
            if child.is_dir() and not child.name.startswith("."):
                found.append((layer, child))
    return found


def load_yaml(path: Path) -> Any:
    with path.open("r", encoding="utf-8") as fh:
        return yaml.safe_load(fh)


def has_heading_keywords(text: str, keywords: tuple[str, ...]) -> bool:
    headings = [line.strip().lower() for line in text.splitlines() if line.lstrip().startswith("#")]
    return any(any(kw in heading for kw in keywords) for heading in headings)


def validate_app_manifest(
    app_dir: Path,
    component_name: str,
    component_dir: Path,
    errors: list[str],
    prefix: str,
) -> None:
    app_cmake = app_dir / "CMakeLists.txt"
    main_dir = app_dir / "main"
    manifest_path = main_dir / "idf_component.yml"
    if not app_cmake.is_file():
        errors.append(f"{prefix}: missing {app_cmake.relative_to(component_dir)}")
    if not manifest_path.is_file():
        errors.append(f"{prefix}: missing {manifest_path.relative_to(component_dir)}")
        return

    try:
        manifest = load_yaml(manifest_path)
    except yaml.YAMLError as exc:
        errors.append(f"{prefix}: invalid YAML in {manifest_path.relative_to(component_dir)}: {exc}")
        return

    if not isinstance(manifest, dict):
        errors.append(f"{prefix}: {manifest_path.relative_to(component_dir)} must be a YAML mapping")
        return

    deps = manifest.get("dependencies")
    if not isinstance(deps, dict):
        errors.append(f"{prefix}: {manifest_path.relative_to(component_dir)} must define 'dependencies'")
        return

    idf_dep = deps.get("idf")
    idf_ver = idf_dep.get("version") if isinstance(idf_dep, dict) else idf_dep
    if not isinstance(idf_ver, str) or "6.1" not in idf_ver:
        errors.append(
            f"{prefix}: {manifest_path.relative_to(component_dir)} must constrain 'idf' to ESP-IDF 6.1 (found: {idf_ver!r})"
        )

    comp_dep = deps.get(component_name)
    if not isinstance(comp_dep, dict) or "override_path" not in comp_dep:
        errors.append(
            f"{prefix}: {manifest_path.relative_to(component_dir)} must reference '{component_name}' via 'override_path'"
        )
        return

    resolved = (main_dir / str(comp_dep["override_path"])).resolve()
    if resolved != component_dir.resolve():
        errors.append(
            f"{prefix}: override_path '{comp_dep['override_path']}' in {manifest_path.relative_to(component_dir)} "
            f"resolves to {resolved}, expected {component_dir.resolve()}"
        )

    # Optional Wokwi configuration sanity checks (only enforced when present)
    wokwi_toml = app_dir / "wokwi.toml"
    diagram_json = app_dir / "diagram.json"
    if wokwi_toml.is_file():
        toml_text = wokwi_toml.read_text(encoding="utf-8", errors="replace")
        for key in ("[wokwi]", "firmware", "elf"):
            if key not in toml_text:
                errors.append(f"{prefix}: wokwi.toml missing required field '{key}'")
        if not diagram_json.is_file():
            errors.append(f"{prefix}: wokwi.toml present without companion diagram.json")
    if diagram_json.is_file():
        try:
            diagram_data = json.loads(diagram_json.read_text(encoding="utf-8"))
            if not isinstance(diagram_data, dict) or not isinstance(diagram_data.get("parts"), list):
                errors.append(f"{prefix}: diagram.json must be a JSON object with a 'parts' list")
        except json.JSONDecodeError as exc:
            errors.append(f"{prefix}: invalid JSON in diagram.json: {exc}")


def validate_component(
    layer: str,
    component_dir: Path,
    known_components: dict[str, tuple[str, Path]],
) -> tuple[list[str], list[str], dict[str, Any]]:
    """Validate a single component. Returns (errors, intra_repo_deps, metadata)."""
    errors: list[str] = []
    intra_deps: list[str] = []
    name = component_dir.name
    rel_path = f"{layer}/{name}"
    upper_name = name.upper()
    meta: dict[str, Any] = {
        "name": name,
        "layer": layer,
        "path": rel_path,
        "version": "",
        "description": "",
        "targets": [],
        "examples": [],
        "test_apps": [],
        "intra_repo_deps": intra_deps,
    }

    if not COMPONENT_NAME_RE.match(name):
        errors.append(
            f"{rel_path}: component directory must match 'bjq_<lowercase_snake_case>' (found '{name}')"
        )

    # Required top-level files
    required_files = [
        "CMakeLists.txt",
        "Kconfig",
        "idf_component.yml",
        "LICENSE",
        "README.md",
        "CHANGELOG.md",
        f"include/{name}.h",
        f"src/{name}.c",
        *REQUIRED_DOCS,
    ]
    for rel_file in required_files:
        file_path = component_dir / rel_file
        if not file_path.is_file():
            errors.append(f"{rel_path}: missing required file '{rel_file}'")
        elif file_path.stat().st_size == 0:
            errors.append(f"{rel_path}: required file '{rel_file}' is empty")

    # LICENSE check
    license_path = component_dir / "LICENSE"
    if license_path.is_file():
        license_text = license_path.read_text(encoding="utf-8", errors="replace")
        if "Apache License" not in license_text or "Version 2.0" not in license_text:
            errors.append(f"{rel_path}: LICENSE must contain the Apache License, Version 2.0 text")

    # CMakeLists.txt basic check
    cmake_path = component_dir / "CMakeLists.txt"
    if cmake_path.is_file():
        cmake_text = cmake_path.read_text(encoding="utf-8", errors="replace")
        if "idf_component_register" not in cmake_text:
            errors.append(f"{rel_path}: CMakeLists.txt must call idf_component_register()")
        if re.search(r"\bREQUIRES\b[^\n]*\bdriver\b", cmake_text):
            errors.append(
                f"{rel_path}: CMakeLists.txt uses legacy monolithic 'driver' component; use ESP-IDF v6.1 'esp_driver_*'"
            )

    # Kconfig prefix check
    kconfig_path = component_dir / "Kconfig"
    if kconfig_path.is_file():
        kconfig_text = kconfig_path.read_text(encoding="utf-8", errors="replace")
        symbols = KCONFIG_SYMBOL_RE.findall(kconfig_text)
        expected_prefix = f"{upper_name}_"
        for sym in symbols:
            if not sym.startswith(expected_prefix):
                errors.append(
                    f"{rel_path}: Kconfig symbol '{sym}' must start with '{expected_prefix}'"
                )

    # idf_component.yml validation
    manifest_path = component_dir / "idf_component.yml"
    if manifest_path.is_file():
        try:
            manifest = load_yaml(manifest_path)
        except yaml.YAMLError as exc:
            errors.append(f"{rel_path}: invalid YAML in idf_component.yml: {exc}")
            manifest = None

        if isinstance(manifest, dict):
            version = str(manifest.get("version", ""))
            meta["version"] = version
            if not SEMVER_RE.match(version):
                errors.append(f"{rel_path}: idf_component.yml 'version' must be valid SemVer (found {version!r})")

            description = manifest.get("description")
            if not isinstance(description, str) or not description.strip():
                errors.append(f"{rel_path}: idf_component.yml must define a non-empty 'description'")
            else:
                meta["description"] = description.strip()

            if manifest.get("license") != "Apache-2.0":
                errors.append(f"{rel_path}: idf_component.yml 'license' must be 'Apache-2.0'")

            if manifest.get("repository") != REPO_URL:
                errors.append(f"{rel_path}: idf_component.yml 'repository' must be '{REPO_URL}'")

            if not manifest.get("url"):
                errors.append(f"{rel_path}: idf_component.yml must define 'url'")

            targets = manifest.get("targets")
            if not isinstance(targets, list) or not targets:
                errors.append(f"{rel_path}: idf_component.yml must specify a non-empty 'targets' list")
            else:
                invalid_targets = [t for t in targets if t not in VALID_TARGETS]
                if invalid_targets:
                    errors.append(f"{rel_path}: invalid ESP-IDF targets in idf_component.yml: {invalid_targets}")
                meta["targets"] = targets

            deps = manifest.get("dependencies")
            if not isinstance(deps, dict):
                errors.append(f"{rel_path}: idf_component.yml must define 'dependencies'")
            else:
                idf_dep = deps.get("idf")
                idf_ver = idf_dep.get("version") if isinstance(idf_dep, dict) else idf_dep
                if not isinstance(idf_ver, str) or ">=6.1.0" not in idf_ver:
                    errors.append(
                        f"{rel_path}: idf_component.yml must require ESP-IDF '>=6.1.0,<7.0.0' (found {idf_ver!r})"
                    )

                for dep_name, dep_spec in deps.items():
                    if dep_name == "idf":
                        continue
                    if isinstance(dep_spec, dict):
                        if "override_path" in dep_spec:
                            errors.append(
                                f"{rel_path}: dependency '{dep_name}' must NOT use 'override_path' in component idf_component.yml"
                            )
                        dep_git = dep_spec.get("git", "")
                        if isinstance(dep_git, str) and "blankryan/bjq-esp-component" in dep_git:
                            dep_path = dep_spec.get("path", "")
                            dep_ver = dep_spec.get("version", "")
                            if not isinstance(dep_path, str) or "/" not in dep_path:
                                errors.append(
                                    f"{rel_path}: intra-repo dependency '{dep_name}' must specify 'path: <layer>/<component>'"
                                )
                            else:
                                dep_layer, dep_comp = dep_path.split("/", 1)
                                if dep_layer not in LAYER_RANK:
                                    errors.append(
                                        f"{rel_path}: intra-repo dependency '{dep_name}' uses unknown layer '{dep_layer}'"
                                    )
                                else:
                                    if LAYER_RANK[dep_layer] > LAYER_RANK[layer]:
                                        errors.append(
                                            f"{rel_path}: forbidden upward layer dependency '{layer}' -> '{dep_layer}/{dep_comp}'"
                                        )
                                    if dep_comp not in known_components:
                                        errors.append(
                                            f"{rel_path}: intra-repo dependency path '{dep_path}' does not exist in repository"
                                        )
                                    else:
                                        intra_deps.append(dep_comp)
                            if not isinstance(dep_ver, str) or not TAG_VERSION_RE.match(dep_ver):
                                errors.append(
                                    f"{rel_path}: intra-repo dependency '{dep_name}' must pin a release tag '<component>/vX.Y.Z' (found {dep_ver!r})"
                                )
        elif manifest is not None:
            errors.append(f"{rel_path}: idf_component.yml must be a YAML mapping")

    # CHANGELOG check
    changelog_path = component_dir / "CHANGELOG.md"
    if changelog_path.is_file():
        changelog_text = changelog_path.read_text(encoding="utf-8", errors="replace")
        if "## [Unreleased]" not in changelog_text:
            errors.append(f"{rel_path}: CHANGELOG.md must contain a '## [Unreleased]' section")

    # Documentation coverage checks
    readme_path = component_dir / "README.md"
    if readme_path.is_file():
        readme_text = readme_path.read_text(encoding="utf-8", errors="replace")
        required_readme_sections = [
            (("overview",), "Overview"),
            (("compatibility", "supported targets", "targets"), "Compatibility / Supported Targets"),
            (("dependencies",), "Dependencies"),
            (("integration", "usage", "installation"), "Integration"),
            (("testing",), "Testing"),
        ]
        for keywords, label in required_readme_sections:
            if not has_heading_keywords(readme_text, keywords):
                errors.append(f"{rel_path}: README.md missing required section heading for '{label}'")

    concurrency_doc = component_dir / "docs/concurrency-and-resources.md"
    if concurrency_doc.is_file():
        text = concurrency_doc.read_text(encoding="utf-8", errors="replace")
        if not has_heading_keywords(text, ("resource", "memory")):
            errors.append(f"{rel_path}: docs/concurrency-and-resources.md missing Resource Usage section")
        if not has_heading_keywords(text, ("concurrency", "thread", "isr")):
            errors.append(f"{rel_path}: docs/concurrency-and-resources.md missing Concurrency section")

    trouble_doc = component_dir / "docs/troubleshooting.md"
    if trouble_doc.is_file():
        text = trouble_doc.read_text(encoding="utf-8", errors="replace")
        if not has_heading_keywords(text, ("error",)):
            errors.append(f"{rel_path}: docs/troubleshooting.md missing Error Handling section")
        if not has_heading_keywords(text, ("limitation",)):
            errors.append(f"{rel_path}: docs/troubleshooting.md missing Known Limitations section")

    bench_doc = component_dir / "docs/bench-testing.md"
    if bench_doc.is_file():
        text = bench_doc.read_text(encoding="utf-8", errors="replace")
        if not has_heading_keywords(text, ("matrix", "verification")):
            errors.append(f"{rel_path}: docs/bench-testing.md missing Verification Matrix section")
        if not has_heading_keywords(text, ("session", "evidence", "serial", "bench-test")):
            errors.append(f"{rel_path}: docs/bench-testing.md missing Bench-Test Sessions / Evidence section")

    # Examples check
    examples_dir = component_dir / "examples"
    if not examples_dir.is_dir():
        errors.append(f"{rel_path}: missing required 'examples/' directory")
    else:
        example_apps = [p for p in sorted(examples_dir.iterdir()) if p.is_dir() and not p.name.startswith(".")]
        if not example_apps:
            errors.append(f"{rel_path}: 'examples/' must contain at least one buildable example project")
        for app_dir in example_apps:
            validate_app_manifest(app_dir, name, component_dir, errors, f"{rel_path}/examples/{app_dir.name}")
            meta["examples"].append(str(app_dir.relative_to(REPO_ROOT)).replace("\\", "/"))

    # test_apps check
    test_apps_dir = component_dir / "test_apps"
    if not test_apps_dir.is_dir():
        errors.append(f"{rel_path}: missing required 'test_apps/' directory")
    else:
        validate_app_manifest(test_apps_dir, name, component_dir, errors, f"{rel_path}/test_apps")
        meta["test_apps"].append(str(test_apps_dir.relative_to(REPO_ROOT)).replace("\\", "/"))

    return errors, intra_deps, meta


def detect_cycles(graph: dict[str, list[str]]) -> list[str]:
    """Detect cycles in intra-repo component dependency graph."""
    visited: set[str] = set()
    in_stack: set[str] = set()
    errors: list[str] = []

    def dfs(node: str, trail: list[str]) -> None:
        visited.add(node)
        in_stack.add(node)
        for dep in graph.get(node, []):
            if dep in in_stack:
                cycle = " -> ".join([*trail[trail.index(dep) :], dep])
                errors.append(f"Cyclic intra-repo dependency detected: {cycle}")
            elif dep not in visited:
                dfs(dep, [*trail, dep])
        in_stack.remove(node)

    for node in sorted(graph):
        if node not in visited:
            dfs(node, [node])
    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description="Validate Bojaqu ESP-IDF shared components.")
    parser.add_argument(
        "--component",
        help="Validate a single component path relative to repo root (e.g., drivers/bjq_mfrc522)",
    )
    parser.add_argument(
        "--list-json",
        action="store_true",
        help="Print discovered component metadata as JSON after validation",
    )
    args = parser.parse_args()

    all_discovered = discover_components(REPO_ROOT)
    known_components = {comp_dir.name: (layer, comp_dir) for layer, comp_dir in all_discovered}

    targets_to_check = all_discovered
    if args.component:
        norm = args.component.replace("\\", "/").strip("/")
        targets_to_check = [
            (layer, comp_dir) for layer, comp_dir in all_discovered if f"{layer}/{comp_dir.name}" == norm
        ]
        if not targets_to_check:
            print(f"ERROR: component '{args.component}' not found.", file=sys.stderr)
            return 1

    all_errors: list[str] = []
    dep_graph: dict[str, list[str]] = {}
    metadata_list: list[dict[str, Any]] = []

    if not targets_to_check:
        all_errors.append("No components discovered under utilities/, drivers/, platform/, or features/.")

    for layer, comp_dir in targets_to_check:
        errors, intra_deps, meta = validate_component(layer, comp_dir, known_components)
        all_errors.extend(errors)
        dep_graph[comp_dir.name] = intra_deps
        metadata_list.append(meta)

    all_errors.extend(detect_cycles(dep_graph))

    if all_errors:
        print("Component validation FAILED:", file=sys.stderr)
        for err in all_errors:
            print(f"  - {err}", file=sys.stderr)
        return 1

    if args.list_json:
        print(json.dumps(metadata_list, indent=2))
    else:
        print(f"OK: validated {len(targets_to_check)} component(s) successfully.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
