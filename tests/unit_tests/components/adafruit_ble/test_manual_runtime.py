"""Compile the production DFU entry path without Bluetooth headers or a radio.

Only hardware and framework lifecycle boundaries are stubbed; the production
request/deferred callback and OTA notification ordering execute unchanged.
"""

from pathlib import Path
import shutil
import subprocess
from sys import platform
from unittest.mock import patch

import pytest


@pytest.mark.parametrize("host_platform", ["win32", "linux", "darwin"])
def test_manual_runtime_host_command(tmp_path: Path, host_platform: str) -> None:
    """Keep native execution on Windows and sanitizers on Linux/macOS."""
    with (
        patch(f"{__name__}.platform", host_platform),
        patch("shutil.which", return_value="g++"),
        patch("subprocess.run") as run,
    ):
        test_manual_runtime(tmp_path, ble_trigger=False, paired=False)

    assert run.call_count == 2
    compile_call, execute_call = run.call_args_list
    command = compile_call.args[0]
    assert ("-fsanitize=address,undefined" in command) == (host_platform != "win32")
    binary = tmp_path / ("runtime.exe" if host_platform == "win32" else "runtime")
    assert command[-2:] == ["-o", str(binary)]
    assert execute_call.args[0] == [str(binary)]
    assert compile_call.kwargs == execute_call.kwargs == {"check": True}


@pytest.mark.parametrize(
    "ble_trigger, paired", [(False, False), (True, False), (True, True)]
)
def test_manual_runtime(tmp_path: Path, ble_trigger: bool, paired: bool) -> None:
    compiler = shutil.which("g++") or shutil.which("clang++")
    if compiler is None:
        pytest.skip("A host C++ compiler is required")
    root = Path(__file__).resolve().parents[4]
    stubs = {
        "esphome/core/defines.h": """
#pragma once
#define USE_ZEPHYR
#define USE_NRF52
#define USE_OTA_STATE_LISTENER
#define ESPHOME_LOG_LEVEL 0
""",
        "esphome/components/ota/ota_backend.h": """
#pragma once
#include <functional>
#include <vector>
#include <cstdint>
#include <cassert>
extern std::vector<int> events;
namespace esphome::ota {
enum OTAState { OTA_STARTED };
class OTAComponent {
 public:
  virtual ~OTAComponent() = default;
  virtual void dump_config() {}
  std::vector<std::function<void()>> pending;
  std::function<void()> listener;
  void defer(std::function<void()> f) { pending.push_back(f); }
  void drain() {
    auto callbacks = std::move(pending);
    for (auto &f : callbacks) f();
  }
 protected:
  void notify_state_(OTAState state, float progress, uint8_t error) {
    assert(state == OTA_STARTED && progress == 0 && error == 0);
    events.push_back(1);
    if (listener) listener();
  }
};
}
""",
        "hal/nrf_power.h": """
#pragma once
#include <cstdint>
struct Power { uint32_t GPREGRET{0}; };
extern Power power;
#define NRF_POWER (&power)
""",
        "esphome/core/application.h": """
#pragma once
#include <cassert>
#include <hal/nrf_power.h>
namespace esphome {
struct Application {
  void reboot() { assert(power.GPREGRET == 0xA8); events.push_back(3); }
};
extern Application App;
}
""",
    }
    if ble_trigger:
        stubs["esphome/core/defines.h"] += "#define USE_ADAFRUIT_BLE_TRIGGER\n"
        if paired:
            stubs["esphome/core/defines.h"] += "#define CONFIG_BT_SMP\n"
        stubs["zephyr/bluetooth/gatt.h"] = """
#pragma once
#include <sys/types.h>
struct bt_conn {};
struct bt_gatt_attr {};
#define BT_GATT_PERM_WRITE_AUTHEN 2
#define BT_GATT_PERM_WRITE 1
#define BT_ATT_ERR_INVALID_OFFSET 7
#define BT_ATT_ERR_VALUE_NOT_ALLOWED 19
#define BT_GATT_ERR(x) (-(x))
#define BT_GATT_SERVICE_DEFINE(name, ...) static_assert(true)
"""
    for name, contents in stubs.items():
        path = tmp_path / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(contents)
    source = tmp_path / "main.cpp"
    source.write_text("""
#include "esphome/components/adafruit_ble/ota/ota_adafruit_ble.cpp"
std::vector<int> events;
Power power;
namespace esphome { Application App; }
int main() {
  esphome::adafruit_ble::OTAComponent component;
  component.listener = []() {
    assert(power.GPREGRET == 0);
    events.push_back(2);
  };
#ifdef USE_ADAFRUIT_BLE_TRIGGER
  using esphome::adafruit_ble::dfu_trigger_write;
#ifdef CONFIG_BT_SMP
  static_assert(esphome::adafruit_ble::DFU_TRIGGER_PERM == BT_GATT_PERM_WRITE_AUTHEN);
#else
  static_assert(esphome::adafruit_ble::DFU_TRIGGER_PERM == BT_GATT_PERM_WRITE);
#endif
  assert(dfu_trigger_write(nullptr, nullptr, "DFU", 3, 1, 0) == -7);
  assert(dfu_trigger_write(nullptr, nullptr, "bad", 3, 0, 0) == -19);
  assert(dfu_trigger_write(nullptr, nullptr, "DFU", 2, 0, 0) == -19);
  assert(component.pending.empty());
  assert(dfu_trigger_write(nullptr, nullptr, "DFU", 3, 0, 0) == 3);
#else
  component.request_dfu_mode();
#endif
  component.request_dfu_mode();
  assert(events.empty() && power.GPREGRET == 0);
  assert(component.pending.size() == 2);
  component.drain();
  component.request_dfu_mode();
  component.drain();
  assert((events == std::vector<int>{1, 2, 3}));
}
""")
    binary = tmp_path / ("runtime.exe" if platform == "win32" else "runtime")
    # The Windows MinGW toolchain does not ship the ASan/UBSan runtimes.
    sanitizer_flags = [] if platform == "win32" else ["-fsanitize=address,undefined"]
    subprocess.run(
        [
            compiler,
            "-std=c++20",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-Wno-unused-parameter",
            *sanitizer_flags,
            "-fno-omit-frame-pointer",
            "-I",
            str(tmp_path),
            "-I",
            str(root),
            str(source),
            "-o",
            str(binary),
        ],
        check=True,
    )
    subprocess.run([str(binary)], check=True)
