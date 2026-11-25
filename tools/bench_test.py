#!/usr/bin/env python3
"""Interactive pre-release hardware and Wokwi bench-testing helper for Bojaqu ESP-IDF components."""

from __future__ import annotations

import argparse
import datetime as dt
import json
import os
import re
import shutil
import subprocess
import sys
import time
from pathlib import Path
from typing import Any

import yaml

REPO_ROOT = Path(__file__).resolve().parent.parent
BENCH_LOGS_DIR = REPO_ROOT / "bench_logs"

sys.path.insert(0, str(REPO_ROOT / "tools"))
from validate_components import discover_components, load_yaml  # noqa: E402

ANSI_ESCAPE_RE = re.compile(r"\x1B\[[0-?]*[ -/]*[@-~]")
MATRIX_START = "<!-- BENCH_TEST_MATRIX_START -->"
MATRIX_END = "<!-- BENCH_TEST_MATRIX_END -->"

BOOT_PATTERNS = (
    re.compile(r"ESP-IDF\s+v", re.IGNORECASE),
    re.compile(r"App name:", re.IGNORECASE),
    re.compile(r"Project version:", re.IGNORECASE),
    re.compile(r"VersionReg", re.IGNORECASE),
    re.compile(r"Initialized\s+bjq_", re.IGNORECASE),
    re.compile(r"Active\s+MFRC522", re.IGNORECASE),
)
FUNCTIONAL_PATTERNS = (
    re.compile(r"Running\s+.*test", re.IGNORECASE),
    re.compile(r":(?:PASS|FAIL)\b"),
    re.compile(r"\b\d+\s+Tests\s+\d+\s+Failures\s+\d+\s+Ignored\b"),
    re.compile(r"Card detected|UID=|SAK=", re.IGNORECASE),
    re.compile(r"\b(?:OK|FAIL)\b.*\[bjq_"),
)
ERROR_PATTERNS = (
    re.compile(r"^\s*[EW]\s+\(\d+\)"),
    re.compile(r"Guru Meditation|assert failed|Backtrace:|Brownout|task_wdt|ESP_ERROR_CHECK failed", re.IGNORECASE),
)
MEMORY_PATTERNS = (
    re.compile(r"free[_ ]heap|min[_ ]free[_ ]heap|high[_ ]water|stack|Total image size|Used static", re.IGNORECASE),
)
SENSITIVE_HINTS = (
    re.compile(r"password|passwd|secret|token|api[_-]?key|ssid", re.IGNORECASE),
)


def strip_ansi(text: str) -> str:
    return ANSI_ESCAPE_RE.sub("", text).replace("\r\n", "\n").replace("\r", "\n")


def extract_important_evidence(raw_text: str, max_excerpt_lines: int = 40) -> dict[str, Any]:
    """Extract categorized important lines and a curated excerpt from a raw serial monitor log."""
    cleaned = strip_ansi(raw_text)
    lines = [line.rstrip() for line in cleaned.splitlines() if line.strip()]

    boot: list[str] = []
    functional: list[str] = []
    errors: list[str] = []
    memory: list[str] = []
    sensitive_warnings: list[str] = []
    curated: list[str] = []

    def append_unique(bucket: list[str], line: str, limit: int = 20) -> None:
        if len(bucket) < limit and line not in bucket:
            bucket.append(line)

    for line in lines:
        matched = False
        if any(pat.search(line) for pat in ERROR_PATTERNS):
            append_unique(errors, line, limit=25)
            matched = True
        if any(pat.search(line) for pat in BOOT_PATTERNS):
            append_unique(boot, line, limit=15)
            matched = True
        if any(pat.search(line) for pat in FUNCTIONAL_PATTERNS):
            append_unique(functional, line, limit=25)
            matched = True
        if any(pat.search(line) for pat in MEMORY_PATTERNS):
            append_unique(memory, line, limit=15)
            matched = True
        if any(pat.search(line) for pat in SENSITIVE_HINTS):
            append_unique(sensitive_warnings, line, limit=10)
        if matched and len(curated) < max_excerpt_lines:
            curated.append(line)

    if not curated:
        curated = lines[:max_excerpt_lines]

    return {
        "total_lines": len(lines),
        "boot_and_hw_id": boot,
        "functional_proof": functional,
        "errors_warnings_panics": errors,
        "memory_metrics": memory,
        "sensitive_line_hints": sensitive_warnings,
        "curated_excerpt": curated,
    }


def list_serial_ports_reference() -> list[str]:
    """Return available serial ports for display ONLY. Never auto-select a port."""
    ports: list[str] = []
    try:
        from serial.tools import list_ports  # type: ignore[import-not-found]

        for info in sorted(list_ports.comports(), key=lambda item: item.device):
            desc = info.description or "Serial Port"
            ports.append(f"{info.device} ({desc})")
    except Exception:
        pass
    return ports


def get_git_short_sha() -> str:
    result = subprocess.run(
        ["git", "rev-parse", "--short", "HEAD"],
        cwd=REPO_ROOT,
        capture_output=True,
        text=True,
        check=False,
    )
    if result.returncode == 0 and result.stdout.strip():
        return result.stdout.strip()
    return "unknown"


def get_component_info(component_arg: str | None) -> tuple[str, Path, dict[str, Any]]:
    discovered = discover_components(REPO_ROOT)
    if not discovered:
        raise SystemExit("ERROR: no components found under utilities/, drivers/, platform/, or features/.")

    if component_arg:
        norm = component_arg.replace("\\", "/").strip("/")
        for layer, comp_dir in discovered:
            if f"{layer}/{comp_dir.name}" == norm or comp_dir.name == norm:
                manifest = load_yaml(comp_dir / "idf_component.yml")
                return layer, comp_dir, manifest
        raise SystemExit(f"ERROR: component '{component_arg}' not found.")

    print("Available components:")
    for idx, (layer, comp_dir) in enumerate(discovered, start=1):
        print(f"  [{idx}] {layer}/{comp_dir.name}")
    choice = input("Select component number: ").strip()
    try:
        layer, comp_dir = discovered[int(choice) - 1]
    except (ValueError, IndexError) as exc:
        raise SystemExit("ERROR: invalid component selection.") from exc
    manifest = load_yaml(comp_dir / "idf_component.yml")
    return layer, comp_dir, manifest


def discover_component_apps(comp_dir: Path) -> list[Path]:
    apps: list[Path] = []
    examples_dir = comp_dir / "examples"
    if examples_dir.is_dir():
        for child in sorted(examples_dir.iterdir()):
            if child.is_dir() and (child / "CMakeLists.txt").is_file():
                apps.append(child)
    test_apps_dir = comp_dir / "test_apps"
    if test_apps_dir.is_dir() and (test_apps_dir / "CMakeLists.txt").is_file():
        apps.append(test_apps_dir)
    return apps


def capture_serial_pyserial(port: str, baud: int, log_path: Path, duration_s: int | None) -> str:
    """Stream live serial output to stdout while writing raw capture to log_path."""
    try:
        import serial  # type: ignore[import-not-found]
    except ImportError as exc:
        raise RuntimeError(
            "pyserial is not installed in this Python interpreter. Activate your ESP-IDF v6.1 environment "
            "or capture manually with 'idf.py -p <PORT> monitor' and import the log file."
        ) from exc

    log_path.parent.mkdir(parents=True, exist_ok=True)
    captured_chunks: list[str] = []
    print(f"\n--- Live serial monitor on {port} @ {baud} baud (press Ctrl+C to stop) ---")
    if duration_s:
        print(f"Auto-stop timeout: {duration_s}s")

    start = time.monotonic()
    with serial.Serial(port=port, baudrate=baud, timeout=0.25) as ser, log_path.open("w", encoding="utf-8") as fh:
        try:
            while True:
                if duration_s and (time.monotonic() - start) >= duration_s:
                    break
                data = ser.read(ser.in_waiting or 1)
                if data:
                    text = data.decode("utf-8", errors="replace")
                    sys.stdout.write(text)
                    sys.stdout.flush()
                    fh.write(text)
                    fh.flush()
                    captured_chunks.append(text)
        except KeyboardInterrupt:
            print("\n--- Serial capture stopped by engineer ---")

    return "".join(captured_chunks)


def run_idf_build_and_size(app_dir: Path, target: str) -> tuple[bool, str]:
    """Run idf.py set-target/build/size if IDF_PATH is active; return (ok, size_summary)."""
    if not os.environ.get("IDF_PATH"):
        print("NOTE: IDF_PATH is not set in this shell. Skipping automatic idf.py build.")
        return False, "Not captured (IDF_PATH not active)"

    build_dir = app_dir / f"build_bench_{target}"
    cmds = [
        ["idf.py", "-B", str(build_dir), "set-target", target],
        ["idf.py", "-B", str(build_dir), "build"],
    ]
    for cmd in cmds:
        print(f"Running: {' '.join(cmd)}")
        res = subprocess.run(cmd, cwd=app_dir, check=False)
        if res.returncode != 0:
            return False, f"Build failed (exit {res.returncode})"

    size_res = subprocess.run(
        ["idf.py", "-B", str(build_dir), "size"],
        cwd=app_dir,
        capture_output=True,
        text=True,
        check=False,
    )
    size_summary = "Unavailable"
    if size_res.returncode == 0:
        interesting = [
            line.strip()
            for line in size_res.stdout.splitlines()
            if any(k in line for k in ("Total image size", "Used static", "Flash code", "Flash rodata"))
        ]
        if interesting:
            size_summary = "; ".join(interesting[:4])
    return True, size_summary


def run_idf_flash(app_dir: Path, target: str, port: str) -> bool:
    build_dir = app_dir / f"build_bench_{target}"
    cmd = ["idf.py", "-B", str(build_dir), "-p", port, "flash"]
    print(f"Running: {' '.join(cmd)}")
    res = subprocess.run(cmd, cwd=app_dir, check=False)
    return res.returncode == 0


def run_wokwi_cli(app_dir: Path, target: str, log_path: Path, timeout_ms: int) -> tuple[bool, str]:
    wokwi_bin = shutil.which("wokwi-cli")
    if not wokwi_bin or not os.environ.get("WOKWI_CLI_TOKEN"):
        return False, ""
    if not (app_dir / "wokwi.toml").is_file() or not (app_dir / "diagram.json").is_file():
        print(f"NOTE: {app_dir} does not contain wokwi.toml + diagram.json.")
        return False, ""

    log_path.parent.mkdir(parents=True, exist_ok=True)
    cmd = [
        wokwi_bin,
        str(app_dir),
        "--timeout",
        str(timeout_ms),
        "--serial-log-file",
        str(log_path),
    ]
    print(f"Running: {' '.join(cmd)}")
    res = subprocess.run(cmd, cwd=app_dir, check=False)
    text = log_path.read_text(encoding="utf-8", errors="replace") if log_path.is_file() else ""
    return res.returncode == 0, text


def prompt_import_or_paste_log(raw_log_path: Path) -> str:
    path_str = input("Path to saved serial log file (or press Enter to paste lines manually): ").strip()
    if path_str:
        src = Path(path_str).expanduser()
        if not src.is_file():
            print(f"WARNING: file '{src}' not found; continuing with empty log.")
            return ""
        text = src.read_text(encoding="utf-8", errors="replace")
        raw_log_path.parent.mkdir(parents=True, exist_ok=True)
        raw_log_path.write_text(text, encoding="utf-8", newline="\n")
        return text

    print("Paste important serial output lines below (enter a single line containing END to finish):")
    lines: list[str] = []
    while True:
        try:
            line = input()
        except EOFError:
            break
        if line.strip() == "END":
            break
        lines.append(line)
    text = "\n".join(lines) + ("\n" if lines else "")
    if text:
        raw_log_path.parent.mkdir(parents=True, exist_ok=True)
        raw_log_path.write_text(text, encoding="utf-8", newline="\n")
    return text


def render_matrix_table(rows: list[dict[str, str]]) -> str:
    header = [
        MATRIX_START,
        "| Target SoC | Environment | Board / Setup | App | Version / Commit | Date | Status | Notes |",
        "|---|---|---|---|---|---|---|---|",
    ]
    for row in rows:
        header.append(
            f"| `{row['target']}` | {row['env']} | {row['board']} | `{row['app']}` | "
            f"`{row['version']}` (`{row['commit']}`) | {row['date']} | **{row['status']}** | {row['notes']} |"
        )
    header.append(MATRIX_END)
    return "\n".join(header)


def format_session_block(
    *,
    date_str: str,
    version: str,
    commit: str,
    target: str,
    env_label: str,
    board: str,
    app_rel: str,
    status: str,
    size_summary: str,
    notes: str,
    evidence: dict[str, Any] | None,
) -> str:
    lines = [
        f"### {date_str} — `v{version}` (`{commit}`) — `{target}` ({env_label}) — {status}",
        "",
        f"- **Board / Setup:** {board}",
        f"- **App Tested:** `{app_rel}`",
        f"- **Measured Size / Memory:** {size_summary}",
        f"- **Engineer Notes:** {notes}",
    ]
    if evidence:
        if evidence["errors_warnings_panics"]:
            lines.append(f"- **Captured Errors/Warnings/Panics:** {len(evidence['errors_warnings_panics'])} line(s)")
        else:
            lines.append("- **Captured Errors/Warnings/Panics:** None")
        if evidence["memory_metrics"]:
            lines.append(f"- **Serial Memory Lines:** `{'; '.join(evidence['memory_metrics'][:4])}`")
        if evidence["curated_excerpt"]:
            lines.extend(
                [
                    "",
                    "```text",
                    *evidence["curated_excerpt"],
                    "```",
                ]
            )
    lines.append("")
    return "\n".join(lines)


def update_bench_testing_doc(
    comp_dir: Path,
    matrix_rows: list[dict[str, str]],
    session_blocks: list[str],
) -> Path:
    doc_path = comp_dir / "docs" / "bench-testing.md"
    if not doc_path.is_file():
        raise SystemExit(f"ERROR: missing {doc_path}; create docs/bench-testing.md first.")

    content = doc_path.read_text(encoding="utf-8")
    matrix_md = render_matrix_table(matrix_rows)
    if MATRIX_START in content and MATRIX_END in content:
        pattern = re.compile(rf"{re.escape(MATRIX_START)}.*?{re.escape(MATRIX_END)}", re.DOTALL)
        content = pattern.sub(matrix_md, content, count=1)
    else:
        content = content.rstrip() + "\n\n## Verification Matrix\n\n" + matrix_md + "\n"

    if session_blocks:
        if "## Bench-Test Sessions" not in content:
            content = content.rstrip() + "\n\n## Bench-Test Sessions\n"
        content = content.rstrip() + "\n\n" + "\n".join(session_blocks)

    doc_path.write_text(content.rstrip() + "\n", encoding="utf-8", newline="\n")
    return doc_path


def print_release_traceability_snippet(
    layer: str,
    comp_name: str,
    version: str,
    commit: str,
    matrix_rows: list[dict[str, str]],
) -> None:
    print("\n=== Copy-Pasteable Release Traceability Block ===")
    print("### Release Traceability")
    print(f"- **Component:** `{comp_name}` (`{layer}/{comp_name}`)")
    print(f"- **Version:** `{version}`")
    print(f"- **Source Commit:** `{commit}`")
    print("- **ESP-IDF Version:** `6.1.0` (`>=6.1.0,<7.0.0`)")
    print("- **Bench-Test Summary:**")
    for row in matrix_rows:
        print(f"  - `{row['target']}`: {row['env']} — **{row['status']}** ({row['notes']})")


def handle_extract_log(args: argparse.Namespace) -> int:
    log_path = Path(args.extract_log)
    if not log_path.is_file():
        print(f"ERROR: log file not found: {log_path}", file=sys.stderr)
        return 1
    evidence = extract_important_evidence(log_path.read_text(encoding="utf-8", errors="replace"))
    if args.json:
        print(json.dumps(evidence, indent=2))
    else:
        print(f"Total non-empty lines: {evidence['total_lines']}")
        if evidence["sensitive_line_hints"]:
            print("WARNING: Potential sensitive lines detected — redact before committing:")
            for line in evidence["sensitive_line_hints"]:
                print(f"  ! {line}")
        print("\nCurated Excerpt:")
        print("```text")
        for line in evidence["curated_excerpt"]:
            print(line)
        print("```")
    return 0


def handle_record_skip(args: argparse.Namespace) -> int:
    layer, comp_dir, manifest = get_component_info(args.component)
    comp_name = comp_dir.name
    version = args.version or str(manifest.get("version", "0.1.0"))
    commit = get_git_short_sha()
    today = dt.date.today().isoformat()
    reason = args.reason.strip()
    if not reason:
        print("ERROR: --reason must be non-empty when recording a bench-test skip.", file=sys.stderr)
        return 1

    rows = [
        {
            "target": target,
            "env": "Build-only (CI)",
            "board": "N/A",
            "app": "examples/ + test_apps/",
            "version": version,
            "commit": commit,
            "date": today,
            "status": "SKIPPED",
            "notes": f"Skipped by engineer: {reason}",
        }
        for target in manifest.get("targets", [])
    ]
    session = format_session_block(
        date_str=today,
        version=version,
        commit=commit,
        target="all",
        env_label="Skipped",
        board="N/A",
        app_rel="N/A",
        status="SKIPPED",
        size_summary="Not measured (bench-test skipped)",
        notes=f"Engineer confirmed bench-test skipped for this release: {reason}",
        evidence=None,
    )
    doc_path = update_bench_testing_doc(comp_dir, rows, [session])
    print(f"Updated {doc_path.relative_to(REPO_ROOT)}")
    print_release_traceability_snippet(layer, comp_name, version, commit, rows)
    return 0


def handle_record_result(args: argparse.Namespace) -> int:
    layer, comp_dir, manifest = get_component_info(args.component)
    comp_name = comp_dir.name
    version = args.version or str(manifest.get("version", "0.1.0"))
    commit = get_git_short_sha()
    today = dt.date.today().isoformat()
    targets = list(manifest.get("targets", []))
    if args.target not in targets:
        print(f"ERROR: target '{args.target}' is not in {comp_name} targets ({targets}).", file=sys.stderr)
        return 1

    evidence = None
    if args.log_file:
        log_path = Path(args.log_file)
        if not log_path.is_file():
            print(f"ERROR: log file not found: {log_path}", file=sys.stderr)
            return 1
        evidence = extract_important_evidence(log_path.read_text(encoding="utf-8", errors="replace"))

    env_label = "Real HW" if args.env == "real-hw" else ("Wokwi" if args.env == "wokwi" else "Real HW + Wokwi")
    status_label = args.status.upper()

    rows: list[dict[str, str]] = []
    for target in targets:
        if target == args.target:
            rows.append(
                {
                    "target": target,
                    "env": env_label,
                    "board": args.board,
                    "app": args.app,
                    "version": version,
                    "commit": commit,
                    "date": today,
                    "status": status_label,
                    "notes": args.notes,
                }
            )
        else:
            rows.append(
                {
                    "target": target,
                    "env": "Build-only (CI)",
                    "board": "N/A",
                    "app": "examples/ + test_apps/",
                    "version": version,
                    "commit": commit,
                    "date": today,
                    "status": "BUILD-ONLY",
                    "notes": args.untested_reason,
                }
            )

    session = format_session_block(
        date_str=today,
        version=version,
        commit=commit,
        target=args.target,
        env_label=env_label,
        board=args.board,
        app_rel=args.app,
        status=status_label,
        size_summary=args.size_summary,
        notes=args.notes,
        evidence=evidence,
    )
    doc_path = update_bench_testing_doc(comp_dir, rows, [session])
    print(f"Updated {doc_path.relative_to(REPO_ROOT)}")
    print_release_traceability_snippet(layer, comp_name, version, commit, rows)
    return 0


def run_interactive(args: argparse.Namespace) -> int:
    layer, comp_dir, manifest = get_component_info(args.component)
    comp_name = comp_dir.name
    version = args.version or str(manifest.get("version", "0.1.0"))
    commit = get_git_short_sha()
    today = dt.date.today().isoformat()
    timestamp = dt.datetime.now().strftime("%Y%m%d_%H%M%S")
    targets: list[str] = list(manifest.get("targets", []))

    print(f"\nComponent: {layer}/{comp_name} (v{version}, commit {commit})")
    print(f"Declared targets: {', '.join(targets)}")

    # Mandatory Prompt 1: Ask engineer whether to bench-test this release
    answer = input("\nRun pre-release hardware/Wokwi bench-test for this release? [y/N]: ").strip().lower()
    if answer not in ("y", "yes"):
        reason = input("Reason for skipping bench-test (e.g., docs-only release): ").strip() or "Engineer skipped"
        skip_args = argparse.Namespace(component=f"{layer}/{comp_name}", version=version, reason=reason)
        return handle_record_skip(skip_args)

    apps = discover_component_apps(comp_dir)
    if not apps:
        print(f"ERROR: no buildable apps found under {comp_dir}", file=sys.stderr)
        return 1

    print("\nSelect app to bench-test:")
    for idx, app in enumerate(apps, start=1):
        print(f"  [{idx}] {app.relative_to(comp_dir).as_posix()}")
    app_choice = input("App number [1]: ").strip() or "1"
    try:
        app_dir = apps[int(app_choice) - 1]
    except (ValueError, IndexError) as exc:
        raise SystemExit("ERROR: invalid app selection.") from exc
    app_rel = app_dir.relative_to(comp_dir).as_posix()

    # Mandatory Prompt 2: Ask per target chip (Real HW / Wokwi / Both / Unavailable)
    print("\nFor each supported target chip, select bench-test environment:")
    print("  [r] Real Hardware   [w] Wokwi   [b] Both   [s] Skip / Unavailable (Build-only)")
    chip_plan: dict[str, tuple[str, str]] = {}
    for target in targets:
        mode = input(f"  Target '{target}' [r/w/b/s] (default s): ").strip().lower() or "s"
        if mode not in ("r", "w", "b", "s"):
            mode = "s"
        skip_reason = ""
        if mode == "s":
            skip_reason = input(f"    Reason '{target}' is not bench-tested [Board not available]: ").strip()
            skip_reason = skip_reason or "Board not available on bench"
        chip_plan[target] = (mode, skip_reason)

    matrix_rows: list[dict[str, str]] = []
    session_blocks: list[str] = []

    for target in targets:
        mode, skip_reason = chip_plan[target]
        if mode == "s":
            matrix_rows.append(
                {
                    "target": target,
                    "env": "Build-only (CI)",
                    "board": "N/A",
                    "app": app_rel,
                    "version": version,
                    "commit": commit,
                    "date": today,
                    "status": "BUILD-ONLY",
                    "notes": skip_reason,
                }
            )
            continue

        envs_to_run = ["real-hw", "wokwi"] if mode == "b" else (["real-hw"] if mode == "r" else ["wokwi"])
        per_chip_statuses: list[str] = []
        per_chip_boards: list[str] = []
        per_chip_notes: list[str] = []

        for env_key in envs_to_run:
            env_label = "Real HW" if env_key == "real-hw" else "Wokwi"
            raw_log_path = BENCH_LOGS_DIR / comp_name / f"{timestamp}_{target}_{env_key}.log"
            raw_text = ""
            size_summary = "Not measured"

            print(f"\n=== Bench-Testing {comp_name} on {target} ({env_label}) ===")
            if env_key == "real-hw":
                board = input("Board / PCB revision & wiring summary (e.g., ESP32-S3 DevKitC + MFRC522): ").strip()
                board = board or f"{target} devkit"

                ports = list_serial_ports_reference()
                if ports:
                    print("Detected serial ports (reference only — NOT auto-selected):")
                    for p in ports:
                        print(f"  - {p}")
                else:
                    print("No serial ports detected automatically (you can still type a COM port or import a log).")

                capture_mode = input("Capture method: [l] Live COM flash/monitor, [i] Import/paste log [l/i]: ").strip().lower()
                if capture_mode == "i":
                    raw_text = prompt_import_or_paste_log(raw_log_path)
                else:
                    # Safety rule: ALWAYS require explicit COM port from engineer
                    port = ""
                    while not port:
                        port = input(f"Enter EXPLICIT serial port for '{target}' (e.g., COM5 or /dev/ttyUSB0): ").strip()
                    confirm = input(f"Confirm using serial port '{port}' for '{target}'? [y/N]: ").strip().lower()
                    if confirm not in ("y", "yes"):
                        print("Aborted live serial capture for this target; falling back to log import.")
                        raw_text = prompt_import_or_paste_log(raw_log_path)
                    else:
                        baud_str = input("Serial baud rate [115200]: ").strip() or "115200"
                        baud = int(baud_str)
                        do_flash = input("Build and flash app with idf.py before monitoring? [y/N]: ").strip().lower()
                        if do_flash in ("y", "yes"):
                            ok_build, size_summary = run_idf_build_and_size(app_dir, target)
                            if ok_build:
                                if not run_idf_flash(app_dir, target, port):
                                    print("WARNING: idf.py flash returned non-zero exit code.")
                        dur_str = input("Auto-stop timeout in seconds (leave empty for manual Ctrl+C): ").strip()
                        duration_s = int(dur_str) if dur_str.isdigit() else None
                        try:
                            raw_text = capture_serial_pyserial(port, baud, raw_log_path, duration_s)
                        except RuntimeError as exc:
                            print(f"NOTE: {exc}")
                            raw_text = prompt_import_or_paste_log(raw_log_path)
            else:
                board = f"Wokwi ({target})"
                ok_build, size_summary = run_idf_build_and_size(app_dir, target)
                if ok_build:
                    ok_wokwi, raw_text = run_wokwi_cli(app_dir, target, raw_log_path, args.wokwi_timeout_ms)
                    if not ok_wokwi:
                        print(
                            "NOTE: wokwi-cli unavailable or WOKWI_CLI_TOKEN not set. "
                            "Run via VS Code Wokwi extension ('Wokwi: Start Simulator') and import/paste output."
                        )
                        raw_text = prompt_import_or_paste_log(raw_log_path)
                else:
                    raw_text = prompt_import_or_paste_log(raw_log_path)

            evidence = extract_important_evidence(raw_text) if raw_text else None
            if evidence:
                print(f"\nExtracted {len(evidence['curated_excerpt'])} important line(s) from {evidence['total_lines']} total.")
                if evidence["errors_warnings_panics"]:
                    print(f"  Errors/Warnings/Panics captured: {len(evidence['errors_warnings_panics'])}")
                if evidence["sensitive_line_hints"]:
                    print("  WARNING: Potential sensitive values detected — review & redact docs/bench-testing.md before commit!")

            status = input(f"Result for {target} ({env_label}) [PASS/FAIL] (default PASS): ").strip().upper() or "PASS"
            notes = input("Observed hardware behavior / notes (e.g., VersionReg=0x92, UID verified): ").strip()
            notes = notes or "Verified on bench"

            per_chip_statuses.append(f"{env_label}: {status}")
            per_chip_boards.append(board)
            per_chip_notes.append(notes)
            session_blocks.append(
                format_session_block(
                    date_str=today,
                    version=version,
                    commit=commit,
                    target=target,
                    env_label=env_label,
                    board=board,
                    app_rel=app_rel,
                    status=status,
                    size_summary=size_summary,
                    notes=notes,
                    evidence=evidence,
                )
            )

        combined_env = "Both" if mode == "b" else ("Real HW" if mode == "r" else "Wokwi")
        combined_status = "FAIL" if any("FAIL" in s for s in per_chip_statuses) else "PASS"
        matrix_rows.append(
            {
                "target": target,
                "env": combined_env,
                "board": "; ".join(per_chip_boards),
                "app": app_rel,
                "version": version,
                "commit": commit,
                "date": today,
                "status": combined_status,
                "notes": "; ".join(per_chip_notes),
            }
        )

    doc_path = update_bench_testing_doc(comp_dir, matrix_rows, session_blocks)
    print(f"\nUpdated {doc_path.relative_to(REPO_ROOT)}")
    print("Review and redact any sensitive serial lines before committing docs/bench-testing.md.")
    print_release_traceability_snippet(layer, comp_name, version, commit, matrix_rows)
    return 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Interactive pre-release hardware/Wokwi bench-test and serial evidence capture tool."
    )
    parser.add_argument("--component", help="Component path or name (e.g., drivers/bjq_mfrc522)")
    parser.add_argument("--version", help="Override component version recorded in the report")
    parser.add_argument("--wokwi-timeout-ms", type=int, default=15000, help="Wokwi CLI timeout in ms (default: 15000)")

    # Utility / non-interactive modes
    parser.add_argument("--extract-log", help="Extract curated evidence from an existing raw serial log file")
    parser.add_argument("--json", action="store_true", help="Output --extract-log result as JSON")

    parser.add_argument("--record-skip", action="store_true", help="Record an engineer-approved bench-test skip")
    parser.add_argument("--reason", default="", help="Skip justification (required with --record-skip)")

    parser.add_argument("--record-result", action="store_true", help="Record a bench-test result non-interactively")
    parser.add_argument("--target", help="Target chip for --record-result (e.g., esp32s3)")
    parser.add_argument("--env", choices=("real-hw", "wokwi", "both"), default="real-hw")
    parser.add_argument("--board", default="DevKit", help="Board / PCB revision description")
    parser.add_argument("--app", default="examples/basic", help="Tested app relative path")
    parser.add_argument("--status", choices=("pass", "fail"), default="pass")
    parser.add_argument("--log-file", help="Optional raw serial monitor log file to extract evidence from")
    parser.add_argument("--size-summary", default="Not measured", help="Measured flash/RAM size summary")
    parser.add_argument("--notes", default="Verified on bench", help="Observed hardware behavior notes")
    parser.add_argument(
        "--untested-reason",
        default="Board not available on bench",
        help="Reason recorded for other declared targets not tested in this session",
    )
    return parser


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()

    if args.extract_log:
        return handle_extract_log(args)
    if args.record_skip:
        return handle_record_skip(args)
    if args.record_result:
        if not args.target:
            print("ERROR: --target is required with --record-result", file=sys.stderr)
            return 1
        return handle_record_result(args)
    return run_interactive(args)


if __name__ == "__main__":
    raise SystemExit(main())
