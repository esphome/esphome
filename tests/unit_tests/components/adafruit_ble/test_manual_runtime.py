"""Compile the production DFU entry path without Bluetooth headers or a radio.

The production component executes unchanged against minimal scheduler, OTA and
hardware stubs. This is not an integration test of the real framework classes.
"""

from pathlib import Path
import shutil
import subprocess
from sys import platform

import pytest


@pytest.mark.parametrize("ble_trigger", [False, True])
def test_manual_runtime(tmp_path: Path, ble_trigger: bool) -> None:
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
#include <hal/nrf_power.h>
extern std::vector<int> events;
namespace esphome::ota {
enum OTAState { OTA_STARTED };
// Minimal boundary double for ota_backend.h and Component::defer().
// It models queued execution and notification order, not the full framework ABI.
class OTAComponent {
 public:
  virtual ~OTAComponent() = default;
  virtual void dump_config() {}
  std::function<void()> pending;
  void defer(std::function<void()> f) { pending = std::move(f); }
 protected:
  void notify_state_(OTAState state, float progress, uint8_t error) {
    assert(state == OTA_STARTED && progress == 0 && error == 0);
    assert(power.GPREGRET == 0);
    events.push_back(1);
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
struct RebootRequested {};
struct Application {
  [[noreturn]] void reboot() {
    assert(power.GPREGRET == 0xA8);
    events.push_back(2);
    throw RebootRequested{};
  }
};
extern Application App;
}
""",
    }
    if ble_trigger:
        stubs["esphome/core/defines.h"] += "#define USE_ADAFRUIT_BLE_TRIGGER\n"
        stubs["zephyr/bluetooth/gatt.h"] = """
#pragma once
#include <sys/types.h>
struct bt_conn {};
struct bt_gatt_attr {};
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
#ifdef USE_ADAFRUIT_BLE_TRIGGER
  using esphome::adafruit_ble::dfu_trigger_write;
  assert(dfu_trigger_write(nullptr, nullptr, "DFU", 3, 0, 0) == 3);
#else
  component.request_dfu_mode();
#endif
  assert(events.empty() && power.GPREGRET == 0);
  assert(component.pending);
  bool rebooted = false;
  try {
    component.pending();
  } catch (const esphome::RebootRequested &) {
    rebooted = true;
  }
  assert(rebooted);
  assert((events == std::vector<int>{1, 2}));
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
