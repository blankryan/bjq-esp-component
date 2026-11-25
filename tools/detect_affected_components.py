#!/usr/bin/env python3
"""Determine which ESP-IDF example/test apps need to be built in CI."""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from pathlib import Path
from typing import Any

from validate_components import LAYERS, REPO_ROOT, discover_components, validate_component

GLOBAL_TRIGGER_PREFIXES = (
    ".github/workflows/",
    "tools/",
)


def git_changed_files(base_ref: str, head_ref: str) -> list[str]:
    cmd = ["git", "diff", "--name-only", f"{base_ref}...{head_ref}"]
    result = subprocess.run(cmd, cwd=REPO_ROOT, capture_output=True, text=True, check=False)
    if result.returncode != 0:
        return []
    return [line.strip().replace("\\", "/") for line in result.stdout.splitlines() if line.strip()]


def main() -> int:
    parser = argparse.ArgumentParser(description="Compute CI build matrix for affected components.")
    parser.add_argument("--base-ref", help="Git base ref/SHA for PR diff")
    parser.add_argument("--head-ref", default="HEAD", help="Git head ref/SHA")
    parser.add_argument("--all", action="store_true", help="Include all components regardless of diff")
    args = parser.parse_args()

    discovered = discover_components(REPO_ROOT)
    known = {comp_dir.name: (layer, comp_dir) for layer, comp_dir in discovered}

    metas: dict[str, dict[str, Any]] = {}
    reverse_deps: dict[str, set[str]] = {name: set() for name in known}
    for layer, comp_dir in discovered:
        _, intra_deps, meta = validate_component(layer, comp_dir, known)
        metas[comp_dir.name] = meta
        for dep in intra_deps:
            reverse_deps.setdefault(dep, set()).add(comp_dir.name)

    selected: set[str] = set()
    if args.all or not args.base_ref:
        selected = set(metas.keys())
    else:
        changed = git_changed_files(args.base_ref, args.head_ref)
        if not changed or any(
            any(path.startswith(prefix) for prefix in GLOBAL_TRIGGER_PREFIXES) for path in changed
        ):
            selected = set(metas.keys())
        else:
            queue: list[str] = []
            for path in changed:
                parts = path.split("/")
                if len(parts) >= 2 and parts[0] in LAYERS and parts[1] in metas:
                    if parts[1] not in selected:
                        selected.add(parts[1])
                        queue.append(parts[1])
            # Propagate to dependent upper-layer components
            while queue:
                curr = queue.pop()
                for dependent in reverse_deps.get(curr, ()):
                    if dependent not in selected:
                        selected.add(dependent)
                        queue.append(dependent)

    # Build matrix grouped by target chip -> list of app directories
    target_apps: dict[str, list[str]] = {}
    for name in sorted(selected):
        meta = metas[name]
        apps = [*meta["examples"], *meta["test_apps"]]
        for target in meta["targets"]:
            target_apps.setdefault(target, []).extend(apps)

    matrix = [
        {"target": target, "apps": sorted(set(apps))}
        for target, apps in sorted(target_apps.items())
        if apps
    ]

    output = {
        "components": sorted(selected),
        "matrix": matrix,
    }
    print(json.dumps(output))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
