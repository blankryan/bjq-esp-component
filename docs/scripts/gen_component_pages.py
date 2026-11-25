"""Dynamically aggregate per-component Markdown docs into the MkDocs build."""

from __future__ import annotations

import sys
from pathlib import Path

import mkdocs_gen_files

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
sys.path.insert(0, str(REPO_ROOT / "tools"))

from validate_components import LAYERS, discover_components, validate_component  # noqa: E402

REPO_LICENSE_URL = "https://github.com/blankryan/bjq-esp-component/blob/main/LICENSE"

LAYER_LABELS = {
    "utilities": "Utilities",
    "drivers": "Drivers",
    "platform": "Platform",
    "features": "Features",
}

DOC_PAGES = [
    ("docs/architecture.md", "architecture.md", "Architecture"),
    ("docs/configuration.md", "configuration.md", "Configuration"),
    ("docs/concurrency-and-resources.md", "concurrency-and-resources.md", "Concurrency & Resources"),
    ("docs/troubleshooting.md", "troubleshooting.md", "Troubleshooting"),
    ("CHANGELOG.md", "changelog.md", "Changelog"),
]


def rewrite_readme_links(text: str) -> str:
    replacements = {
        "(docs/architecture.md)": "(architecture.md)",
        "(docs/configuration.md)": "(configuration.md)",
        "(docs/concurrency-and-resources.md)": "(concurrency-and-resources.md)",
        "(docs/troubleshooting.md)": "(troubleshooting.md)",
        "(CHANGELOG.md)": "(changelog.md)",
        "(LICENSE)": f"({REPO_LICENSE_URL})",
    }
    for old, new in replacements.items():
        text = text.replace(old, new)
    return text


def main() -> None:
    discovered = discover_components(REPO_ROOT)
    known = {comp_dir.name: (layer, comp_dir) for layer, comp_dir in discovered}

    by_layer: dict[str, list[dict]] = {layer: [] for layer in LAYERS}
    for layer, comp_dir in discovered:
        _, _, meta = validate_component(layer, comp_dir, known)
        meta["dir"] = comp_dir
        by_layer[layer].append(meta)

    # Generate live component catalog index
    catalog_lines = [
        "# Component Catalog",
        "",
        "All reusable ESP-IDF `>=6.1.0,<7.0.0` components organized by architectural layer.",
        "",
        "| Layer | Component | Version | Supported Targets | Description |",
        "|---|---|---|---|---|",
    ]

    summary_lines = [
        "- [Component Catalog](index.md)",
    ]

    for layer in LAYERS:
        comps = by_layer[layer]
        if not comps:
            continue
        summary_lines.append(f"- {LAYER_LABELS[layer]}")
        for meta in comps:
            name = meta["name"]
            comp_dir: Path = meta["dir"]
            targets_md = ", ".join(f"`{t}`" for t in meta["targets"])
            catalog_lines.append(
                f"| `{layer}` | [`{name}`]({layer}/{name}/index.md) | `{meta['version']}` | {targets_md} | {meta['description']} |"
            )

            # Copy component README.md as virtual index.md
            readme_dst = f"components/{layer}/{name}/index.md"
            readme_content = rewrite_readme_links((comp_dir / "README.md").read_text(encoding="utf-8"))
            with mkdocs_gen_files.open(readme_dst, "w") as fd:
                fd.write(readme_content)

            summary_lines.append(f"    - {name}")
            summary_lines.append(f"        - [Overview]({layer}/{name}/index.md)")

            for src_rel, dst_name, title in DOC_PAGES:
                src_file = comp_dir / src_rel
                if src_file.is_file():
                    dst_path = f"components/{layer}/{name}/{dst_name}"
                    with mkdocs_gen_files.open(dst_path, "w") as fd:
                        fd.write(src_file.read_text(encoding="utf-8"))
                    summary_lines.append(f"        - [{title}]({layer}/{name}/{dst_name})")

    with mkdocs_gen_files.open("components/index.md", "w") as fd:
        fd.write("\n".join(catalog_lines) + "\n")

    with mkdocs_gen_files.open("components/SUMMARY.md", "w") as fd:
        fd.write("\n".join(summary_lines) + "\n")


main()
