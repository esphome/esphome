"""Exercise the real controller with deterministic clock, UART and entity stubs."""

from pathlib import Path
import subprocess
import sys


def test_controller(tmp_path: Path):
    source = Path(__file__).parent
    root = source.resolve().parents[2]
    for header in (
        "core/component.h",
        "core/application.h",
        "core/log.h",
        "components/uart/uart.h",
        "components/water_heater/water_heater.h",
        "components/sensor/sensor.h",
        "components/binary_sensor/binary_sensor.h",
        "components/text_sensor/text_sensor.h",
        "components/network/util.h",
    ):
        target = tmp_path / "esphome" / header
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text('#include "controller_stubs.h"\n')
    binary = tmp_path / "controller.exe"
    subprocess.run(
        [
            sys.executable,
            "-m",
            "ziglang",
            "c++",
            "-std=c++20",
            "-DESPHOME_LOG_LEVEL=0",
            "-DESPHOME_LOG_LEVEL_VERBOSE=6",
            "-I",
            str(tmp_path),
            "-I",
            str(source.resolve()),
            "-I",
            str(root),
            str(source / "controller.cpp"),
            "-o",
            str(binary),
        ],
        check=True,
        text=True,
    )
    subprocess.run([str(binary)], check=True, text=True)
