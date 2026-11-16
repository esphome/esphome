"""Run the production sparse callback table without an SDK."""

from pathlib import Path
import shutil
import subprocess
import sys

import pytest


def test_endpoint_callbacks_host(tmp_path: Path) -> None:
    compiler = shutil.which("g++") or shutil.which("clang++")
    if compiler is None:
        pytest.skip("A host C++ compiler is required")
    root = Path(__file__).resolve().parents[4]
    source = Path(__file__).with_name("endpoint_callbacks_test.cpp")
    binary = tmp_path / ("runtime.exe" if sys.platform == "win32" else "runtime")
    sanitizer_flags = (
        ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
        if sys.platform != "win32"
        else []
    )
    subprocess.run(
        [
            compiler,
            "-std=c++20",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-g",
            *sanitizer_flags,
            "-I",
            str(root),
            str(source),
            "-o",
            str(binary),
        ],
        check=True,
    )
    subprocess.run([str(binary)], check=True)
