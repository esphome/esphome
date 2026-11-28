"""Exercise the real driver with only its ESP-IDF and entity boundaries stubbed."""

from pathlib import Path
import subprocess
import sys

from esphome.host.toolchain import find_compilers


def test_driver_lifecycle(tmp_path: Path) -> None:
    root = Path(__file__).parents[3]
    fixtures = Path(__file__).parent / "fixtures"
    headers = (
        "core/component.h",
        "core/hal.h",
        "core/log.h",
        "components/ble_client/ble_client.h",
        "components/esp32_ble/ble_uuid.h",
        "components/sensor/sensor.h",
        "components/binary_sensor/binary_sensor.h",
    )
    for header in headers:
        target = tmp_path / "esphome" / header
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text('#include "lifecycle_stubs.h"\n', encoding="utf-8")
    executable = tmp_path / (
        "lifecycle.exe" if sys.platform == "win32" else "lifecycle"
    )
    subprocess.run(
        [
            *find_compilers().cxx,
            "-std=c++17",
            "-DUSE_ESP32",
            "-DUSE_SENSOR",
            "-UNDEBUG",
            f"-I{tmp_path}",
            f"-I{fixtures}",
            f"-I{root}",
            str(root / "esphome/components/thermopro_tp970/thermopro_tp970.cpp"),
            str(fixtures / "lifecycle.cpp"),
            "-o",
            str(executable),
        ],
        check=True,
        timeout=60,
    )
    subprocess.run([str(executable)], check=True, timeout=10)
