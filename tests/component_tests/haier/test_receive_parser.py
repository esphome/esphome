"""Compile and run captured-frame tests with a locally installed Zig C++ compiler."""

import importlib.util
from pathlib import Path
import subprocess
import sys

import pytest


def test_native_receive_parser(tmp_path: Path) -> None:
    if importlib.util.find_spec("ziglang") is None:
        pytest.skip("Install ziglang to run native parser tests")
    root = Path(__file__).resolve().parents[3]
    binary = tmp_path / "haier_parser.exe"
    subprocess.run(
        [
            sys.executable,
            "-m",
            "ziglang",
            "c++",
            "-std=c++20",
            "-I",
            str(root),
            str(Path(__file__).with_name("receive_parser.cpp")),
            "-o",
            str(binary),
        ],
        check=True,
        capture_output=True,
        text=True,
    )
    subprocess.run([str(binary)], check=True, capture_output=True, text=True)
