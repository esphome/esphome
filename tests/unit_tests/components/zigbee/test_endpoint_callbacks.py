"""Run the production sparse callback table with host sanitizers, without an SDK."""

from pathlib import Path
import shutil
import subprocess

import pytest


def test_endpoint_callbacks_host(tmp_path: Path) -> None:
    compiler = shutil.which("g++") or shutil.which("clang++")
    if compiler is None:
        pytest.skip("A host C++ compiler is required")
    root = Path(__file__).resolve().parents[4]
    source = Path(__file__).with_name("endpoint_callbacks_test.cpp")
    binary = tmp_path / "endpoint_callbacks_test"
    subprocess.run(
        [
            compiler,
            "-std=c++20",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-g",
            "-fsanitize=address,undefined",
            "-fno-omit-frame-pointer",
            "-I",
            str(root),
            str(source),
            "-o",
            str(binary),
        ],
        check=True,
    )
    subprocess.run([str(binary)], check=True)
