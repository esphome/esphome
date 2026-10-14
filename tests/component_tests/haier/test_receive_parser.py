"""Compile and run captured-frame tests with the required Zig C++ test compiler."""

from pathlib import Path
import subprocess
import sys


def test_native_receive_parser(tmp_path: Path) -> None:
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
        text=True,
    )
    subprocess.run([str(binary)], check=True, text=True)
