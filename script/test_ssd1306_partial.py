#!/usr/bin/env python3
"""Run offline SSD1306 planner, fault-injection, legacy-parity and cost tests.

This runner compiles the actual component sources against small host-only runtime
and bus substitutes. It complements, not replaces, cpp_unit_test.py and the target
firmware build matrix. Header templates are kept as .h.txt so they cannot shadow
ESPHome headers in ordinary repository builds.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import shlex
import shutil
import subprocess
import sys
import tempfile


def execute(command: list[str], output: Path, *, quiet: bool = False) -> str:
    """Run a command, retain its full output, and fail on any unsuccessful step."""
    print("+", shlex.join(command), flush=True)
    environment = os.environ.copy()
    environment["ASAN_OPTIONS"] = "detect_leaks=1:allocator_may_return_null=1"
    environment["UBSAN_OPTIONS"] = "halt_on_error=1:print_stacktrace=1"
    result = subprocess.run(
        command,
        check=False,
        capture_output=True,
        text=True,
        env=environment,
        timeout=240,
    )
    output.write_text(result.stdout + result.stderr, encoding="utf-8")
    if not quiet or result.returncode:
        print(result.stdout, end="")
        print(result.stderr, end="", file=sys.stderr)
    if result.returncode:
        raise RuntimeError(f"Exit {result.returncode}; see {output}")
    return result.stdout


def main() -> int:
    """Compile and run the self-contained host validation suite."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", default=os.environ.get("CXX", "g++"))
    parser.add_argument("--output-dir", type=Path)
    parser.add_argument(
        "--no-sanitizers", action="store_true", help="Explicitly disable ASan/UBSan"
    )
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    fixture = root / "tests" / "ssd1306_host"
    output = args.output_dir or Path(tempfile.mkdtemp(prefix="ssd1306-validation-"))
    output = output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    compiler = shlex.split(args.compiler)
    if not compiler or shutil.which(compiler[0]) is None:
        parser.error("C++ compiler not found; install g++ or select --compiler clang++")
    include = output / "support"
    for source in (fixture / "support").rglob("*.h.txt"):
        destination = include / source.relative_to(fixture / "support")
        destination = destination.with_suffix("")
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_bytes(source.read_bytes())
    flags = [
        "-std=gnu++20",
        "-DSSD1306_STANDALONE_TEST",
        "-Wall",
        "-Wextra",
        "-Werror",
        "-Wno-unused-variable",
        f"-I{include}",
        f"-I{root}",
    ]
    sanitizer_flags = (
        []
        if args.no_sanitizers
        else ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
    )
    component = root / "esphome" / "components"
    planner = component / "ssd1306_i2c" / "ssd1306_partial_update.cpp"
    driver = component / "ssd1306_i2c" / "ssd1306_i2c.cpp"
    base = component / "ssd1306_base" / "ssd1306_base.cpp"
    results: dict[str, object] = {
        "platform": platform.platform(),
        "sanitizers": not args.no_sanitizers,
        "compiler": execute(
            [*compiler, "--version"], output / "compiler.txt", quiet=True
        ),
        "runtime": "host substitutes; not target hardware or the full ESPHome runtime",
    }

    # The unchanged legacy driver uses a GNU C++ variable-length array.
    # Clang reports this GNU extension even with -std=gnu++20; keep it visible.
    if "clang" in str(results["compiler"]).lower():
        flags.append("-Wno-error=vla-cxx-extension")

    object_cache: dict[tuple[str, ...], Path] = {}

    def build(
        name: str,
        sources: list[Path],
        *,
        feature: bool,
        benchmark: bool = False,
    ) -> Path:
        binary = output / name
        extra = ["-O2"] if benchmark else ["-O1", "-g", *sanitizer_flags]
        define = ["-DUSE_SSD1306_I2C_PARTIAL_UPDATES"] if feature else []
        objects = []
        for source in sources:
            key = (str(source), *extra, *define)
            object_file = object_cache.get(key)
            if object_file is None:
                stamp = hashlib.sha256("\0".join(key).encode()).hexdigest()[:16]
                object_file = output / f"{source.stem}-{stamp}.o"
                execute(
                    [
                        *compiler,
                        *flags,
                        *extra,
                        *define,
                        "-c",
                        str(source),
                        "-o",
                        str(object_file),
                    ],
                    object_file.with_suffix(".log"),
                )
                object_cache[key] = object_file
            objects.append(str(object_file))
        execute(
            [*compiler, *extra, *objects, "-o", str(binary)],
            output / f"{name}-build.txt",
        )
        return binary

    try:
        binary = build(
            "planner-tests",
            [
                planner,
                root / "tests/components/ssd1306_i2c/test_partial_update_planner.cpp",
                fixture / "planner_main.cpp",
            ],
            feature=True,
        )
        results["planner"] = execute([str(binary)], output / "planner-tests.txt")
        binary = build(
            "driver-tests",
            [planner, base, driver, fixture / "driver_test.cpp"],
            feature=True,
        )
        results["driver"] = execute([str(binary)], output / "driver-tests.txt")
        expected = json.loads(
            (fixture / "legacy_baseline.txt").read_text(encoding="utf-8")
        )
        for feature in (False, True):
            name = "legacy-with-feature" if feature else "legacy-without-feature"
            binary = build(
                name,
                [planner, base, driver, fixture / "legacy_trace.cpp"],
                feature=feature,
            )
            trace = execute([str(binary)], output / f"{name}.txt", quiet=True)
            observed = {}
            for row in trace.splitlines():
                key, data = row.split(" ", 1)
                observed[key] = hashlib.sha256(data.encode()).hexdigest()
            if observed != expected["trace_sha256"]:
                raise RuntimeError(f"Legacy transfer mismatch in {name}")
            results[name] = (
                f"PASS: {len(observed)} traces match {expected['base_commit']}"
            )
            print(results[name])
        binary = build(
            "benchmark",
            [planner, fixture / "benchmark.cpp"],
            feature=True,
            benchmark=True,
        )
        execute([str(binary)], output / "benchmark.csv")
        results["result"] = "PASS"
        (output / "results.json").write_text(
            json.dumps(results, indent=2) + "\n", encoding="utf-8"
        )
        print(f"\nSSD1306_HOST_VALIDATION=PASS\nLogs and benchmark: {output}")
        return 0
    except (OSError, RuntimeError, subprocess.TimeoutExpired) as error:
        print(f"SSD1306_HOST_VALIDATION=FAIL: {error}\nLogs: {output}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
