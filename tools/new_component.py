#!/usr/bin/env python3
"""Scaffold a new Bojaqu ESP-IDF v6.1.0 shared component from templates/component."""

from __future__ import annotations

import argparse
import datetime as dt
import re
import shutil
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
TEMPLATE_DIR = REPO_ROOT / "templates" / "component"
LAYERS = ("utilities", "drivers", "platform", "features")
DEFAULT_TARGETS = ["esp32", "esp32s3", "esp32c3", "esp32c6"]
COMPONENT_NAME_RE = re.compile(r"^bjq_[a-z][a-z0-9_]*$")
SEMVER_RE = re.compile(r"^\d+\.\d+\.\d+(?:-[0-9A-Za-z.-]+)?$")


def to_title(component_name: str) -> str:
    short = component_name.removeprefix("bjq_")
    return " ".join(part.upper() if len(part) <= 3 else part.capitalize() for part in short.split("_"))


def render_text(content: str, replacements: dict[str, str]) -> str:
    for key, value in replacements.items():
        content = content.replace(key, value)
    return content


def main() -> int:
    parser = argparse.ArgumentParser(description="Create a new compliant Bojaqu ESP-IDF component.")
    parser.add_argument("--layer", required=True, choices=LAYERS, help="Architectural layer directory")
    parser.add_argument(
        "--name",
        required=True,
        help="Component name with bjq_ prefix (e.g., bjq_ads111x)",
    )
    parser.add_argument("--description", required=True, help="One-line component summary")
    parser.add_argument("--version", default="0.1.0", help="Initial SemVer version (default: 0.1.0)")
    parser.add_argument(
        "--targets",
        nargs="+",
        default=DEFAULT_TARGETS,
        help="Supported ESP32 target chips (default: esp32 esp32s3 esp32c3 esp32c6)",
    )
    parser.add_argument("--force", action="store_true", help="Overwrite target directory if it already exists")
    args = parser.parse_args()

    if not COMPONENT_NAME_RE.match(args.name):
        print(
            f"ERROR: --name must match 'bjq_<lowercase_snake_case>' (e.g., bjq_ads111x), got '{args.name}'",
            file=sys.stderr,
        )
        return 1

    if not SEMVER_RE.match(args.version):
        print(f"ERROR: --version must be valid SemVer, got '{args.version}'", file=sys.stderr)
        return 1

    dest_dir = REPO_ROOT / args.layer / args.name
    if dest_dir.exists():
        if not args.force:
            print(f"ERROR: destination already exists: {dest_dir} (use --force to overwrite)", file=sys.stderr)
            return 1
        shutil.rmtree(dest_dir)

    today_str = dt.date.today().isoformat()
    bench_rows = "\n".join(
        f"| `{target}` | Build-only (CI) | N/A | `examples/ + test_apps/` | `{args.version}` (`unreleased`) | "
        f"{today_str} | **BUILD-ONLY** | Initial scaffold — not yet bench-tested |"
        for target in args.targets
    )
    replacements = {
        "__COMPONENT_NAME__": args.name,
        "__COMPONENT_UPPER__": args.name.upper(),
        "__COMPONENT_TITLE__": to_title(args.name),
        "__LAYER__": args.layer,
        "__DESCRIPTION__": args.description.strip(),
        "__VERSION__": args.version,
        "__TARGETS_YAML__": "\n".join(f"  - {target}" for target in args.targets),
        "__TARGETS_MD__": ", ".join(f"`{target}`" for target in args.targets),
        "__TARGETS_BENCH_ROWS__": bench_rows,
        "__DATE__": today_str,
    }

    for src_path in sorted(TEMPLATE_DIR.rglob("*")):
        rel_str = str(src_path.relative_to(TEMPLATE_DIR))
        rendered_rel = render_text(rel_str, replacements)
        dst_path = dest_dir / rendered_rel
        if src_path.is_dir():
            dst_path.mkdir(parents=True, exist_ok=True)
        else:
            dst_path.parent.mkdir(parents=True, exist_ok=True)
            content = src_path.read_text(encoding="utf-8")
            dst_path.write_text(render_text(content, replacements), encoding="utf-8", newline="\n")

    # Copy root Apache-2.0 LICENSE into component for standalone extraction
    shutil.copyfile(REPO_ROOT / "LICENSE", dest_dir / "LICENSE")

    print(f"Created component scaffold at {dest_dir.relative_to(REPO_ROOT)}")
    print("Next steps:")
    print(f"  1. Implement API in {args.layer}/{args.name}/include/{args.name}.h and src/{args.name}.c")
    print(f"  2. Update docs under {args.layer}/{args.name}/docs/ and README.md")
    print("  3. Run: python tools/validate_components.py")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
