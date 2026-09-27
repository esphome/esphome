"""Compile the real driver against deterministic I2C/scheduler/preferences fakes."""

from pathlib import Path
import subprocess

import pytest

ROOT = Path(__file__).resolve().parents[3] / "esphome/components"
STUB = r"""
#pragma once
#include <cmath>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>
#include <cassert>
#define ESP_LOGCONFIG(...) ((void)0)
#define ESP_LOGE(...) ((void)0)
#define ESP_LOGW(...) ((void)0)
#define ESP_LOGI(...) ((void)0)
#define ESP_LOGD(...) ((void)0)
#define LOG_I2C_DEVICE(...) ((void)0)
#define LOG_UPDATE_INTERVAL(...) ((void)0)
#define LOG_SENSOR(...) ((void)0)
#define LOG_NUMBER(...) ((void)0)
#define LOG_BINARY_SENSOR(...) ((void)0)
#define LOG_TEXT_SENSOR(...) ((void)0)
#define LOG_STR(x) (x)
#define LOG_STR_ARG(x) (x)
#define LOG_STR_LITERAL(x) (x)
#define YESNO(x) ((x) ? "YES" : "NO")
namespace esphome {
using LogString = char;
template<class T> using optional = std::optional<T>;
namespace setup_priority { constexpr float HARDWARE = 800; }
class Component {
 public:
  virtual void setup() {}
  virtual void dump_config() {}
  virtual float get_setup_priority() const { return 600; }
  bool failed = false, warning = false;
  std::map<std::string, std::function<void()>> timers;
  void mark_failed() { failed = true; }
  bool is_failed() const { return failed; }
  void status_set_warning() { warning = true; }
  void status_clear_warning() { warning = false; }
  void set_timeout(const char *name, uint32_t, std::function<void()> f) { timers[name] = f; }
  void cancel_timeout(const char *name) { timers.erase(name); }
  void run_timer() { auto f = timers.begin()->second; timers.erase(timers.begin()); f(); }
};
class PollingComponent : public Component { public: virtual void update() {} };
struct ESPPreferenceObject {
 float *value = nullptr;
 bool load(float *out) { if (!value) return false; *out = *value; return true; }
 bool save(float *in) { if (value) *value = *in; return true; }
};
namespace sensor { class Sensor { public: float state = NAN; void publish_state(float v) { state = v; } }; }
namespace binary_sensor { class BinarySensor { public: bool state{}; void publish_state(bool v) { state = v; } }; }
namespace text_sensor { class TextSensor { public: std::string state; void publish_state(const char *v) { state = v; } }; }
namespace number {
class Number {
 public:
  struct Traits { float max = 150; float get_min_value() { return -55; } float get_max_value() { return max; } } traits;
  float state = NAN;
  float *restored = nullptr;
  template<class T> ESPPreferenceObject make_entity_preference() { return {restored}; }
  void publish_state(float v) { state = v; }
  void call(float v) { control(v); }
 protected: virtual void control(float) = 0;
};
inline void log_number(const char *, const char *, const char *, Number *) {}
}
namespace i2c {
constexpr int ERROR_OK = 0;
inline uint16_t i2ctohs(uint16_t v) { return (v >> 8) | (v << 8); }
class I2CDevice {
 public:
  uint16_t regs[4] = {0, 0, 0x4b00, 0x5000};
  uint8_t pointer = 0;
  bool fail_write = false, fail_read = false;
  int fail_register = -1;
  unsigned writes = 0;
  int write(const uint8_t *data, size_t len) {
    ++writes;
    if (fail_write || data[0] == fail_register) return 1;
    pointer = data[0];
    if (len == 3) regs[pointer] = (data[1] << 8) | data[2];
    return 0;
  }
  int read(uint8_t *data, size_t) {
    if (fail_read) return 1;
    data[0] = regs[pointer] >> 8; data[1] = regs[pointer]; return 0;
  }
};
}
}
"""
PROGRAM = r"""
#include "tmp102/tmp102.h"
using namespace esphome;
using namespace esphome::tmp102;
int main() {
  // Legacy upstream configuration performs no I2C during setup and never writes config/limits.
  TMP102Component legacy;
  legacy.regs[1] = 0x61a0;
  legacy.regs[2] = 0x1230;
  legacy.regs[3] = 0x4560;
  legacy.regs[0] = 0x1900;
  legacy.setup();
  assert(legacy.writes == 0);
  legacy.update(); legacy.run_timer();
  assert(legacy.state == 25);
  assert(legacy.regs[1] == 0x61a0 && legacy.regs[2] == 0x1230 && legacy.regs[3] == 0x4560);
  TMP102Component absent;
  absent.fail_write = true;
  absent.setup(); assert(!absent.is_failed());
  absent.update(); absent.run_timer(); assert(absent.warning);
  absent.fail_write = false;
  absent.update(); absent.run_timer(); assert(!absent.warning);

  for (bool extended : {false, true}) {
    TMP102Component c; c.set_configure(true);
    c.set_extended_mode(extended);
    c.setup();
    assert(c.regs[3] == (extended ? 0x2800 : 0x5000));
    assert(c.regs[2] == (extended ? 0x2580 : 0x4b00));
    assert(c.set_limit_temperature(TMP102_LIMIT_LOW, -55));
    assert(c.regs[2] == (extended ? 0xe480 : 0xc900));
    assert(!c.set_limit_temperature(TMP102_LIMIT_HIGH, NAN));
    assert(!c.set_limit_temperature(TMP102_LIMIT_HIGH, INFINITY));
    assert(!c.set_limit_temperature(TMP102_LIMIT_LOW, -56));
    assert(!c.set_limit_temperature(TMP102_LIMIT_HIGH, extended ? 151 : 128));
    assert(!c.set_limit_temperature(TMP102_LIMIT_LOW, 81));
    c.regs[0] = extended ? 0xff81 : 0xff00; // -1 C, including EM flag
    c.update();
    c.update(); // Must not replace pending callback or re-trigger conversion.
    assert(c.timers.size() == 1);
    assert(c.set_limit_temperature(TMP102_LIMIT_HIGH, 90));
    c.run_timer();
    assert(c.state == -1);
    c.fail_write = true;
    assert(!c.set_limit_temperature(TMP102_LIMIT_HIGH, 100));
    assert(c.get_limit_temperature(TMP102_LIMIT_HIGH) == 90);
    c.fail_write = false;
    c.fail_read = true;
    c.update(); c.run_timer(); assert(c.warning);
    c.fail_read = false;
    c.update(); c.run_timer(); assert(!c.warning);
  }
  // Check every writable configuration field across supported setting values.
  for (auto rate : {TMP102_CONVERSION_RATE_0_25HZ, TMP102_CONVERSION_RATE_1HZ,
                    TMP102_CONVERSION_RATE_4HZ, TMP102_CONVERSION_RATE_8HZ}) {
    for (uint8_t faults : {1, 2, 4, 6}) {
      for (bool enabled : {false, true}) {
        TMP102Component c;
        c.set_configure(true);
        c.set_conversion_rate(rate);
        c.set_fault_queue(faults);
        c.set_extended_mode(enabled);
        c.set_one_shot_mode(enabled);
        c.set_alert_polarity(enabled ? TMP102_ALERT_POLARITY_ACTIVE_HIGH : TMP102_ALERT_POLARITY_ACTIVE_LOW);
        c.set_thermostat_mode(enabled ? TMP102_THERMOSTAT_MODE_INTERRUPT : TMP102_THERMOSTAT_MODE_COMPARATOR);
        c.setup();
        unsigned fault_bits = faults == 1 ? 0 : faults == 2 ? 1 : faults == 4 ? 2 : 3;
        assert(c.regs[1] == ((fault_bits << 11) | (static_cast<unsigned>(rate) << 6) |
                             (enabled ? 0x710 : 0)));
      }
    }
  }
  for (int reg : {1, 2, 3}) {
    TMP102Component c;
    c.set_configure(true);
    c.fail_register = reg;
    c.setup();
    assert(c.is_failed());
    c.update();
    assert(c.timers.empty());
  }
  TMP102Component one; one.set_configure(true);
  one.set_one_shot_mode(true); one.setup(); one.update();
  auto writes = one.writes;
  one.update(); assert(one.writes == writes);
  one.run_timer();
  TMP102Component failed; failed.set_configure(true);
  failed.fail_write = true; failed.setup(); assert(failed.is_failed());
  assert(!failed.set_limit_temperature(TMP102_LIMIT_HIGH, 90));
#ifdef USE_TMP102_BINARY_SENSOR
  for (bool polarity : {false, true}) {
    TMP102Component c; c.set_configure(true); binary_sensor::BinarySensor alert;
    c.set_alert_binary_sensor(&alert);
    c.set_alert_polarity(polarity ? TMP102_ALERT_POLARITY_ACTIVE_HIGH : TMP102_ALERT_POLARITY_ACTIVE_LOW);
    c.setup();
    c.regs[1] = polarity ? 0x20 : 0;
    c.update(); c.run_timer(); assert(alert.state);
    c.regs[1] ^= 0x20;
    c.update(); c.run_timer(); assert(!alert.state);
    c.fail_register = 1;
    c.update(); c.run_timer(); assert(c.warning);
  }
#endif
#ifdef USE_TMP102_NUMBER
  {
    TMP102Component c;
    c.set_configure(true);
    c.set_temperature_high(30);
    c.set_temperature_low(25);
    TMP102LimitNumber high(&c, TMP102_LIMIT_HIGH);
    float saved = 90;
    high.restored = &saved;
    high.set_initial_value(30);
    high.set_restore_value(false);
    c.set_high_limit_control(&high);
    c.setup(); high.setup();
    assert(high.state == 30);
    high.call(40);
    assert(high.state == 40 && saved == 90);
    c.fail_write = true;
    high.call(50);
    assert(high.state == 40 && saved == 90);
    high.run_timer();
    assert(high.state == 40);
  }
  for (auto pair : {std::pair<float,float>{40, 35}, {20, 15}, {20, 35}, {NAN, INFINITY}}) {
    TMP102Component c; c.set_configure(true); TMP102LimitNumber high(&c, TMP102_LIMIT_HIGH), low(&c, TMP102_LIMIT_LOW);
    c.set_temperature_high(30); c.set_temperature_low(25);
    high.set_initial_value(30); low.set_initial_value(25);
    high.restored = &pair.first; low.restored = &pair.second;
    c.set_high_limit_control(&high); c.set_low_limit_control(&low);
    c.setup(); low.setup(); high.setup();
    bool valid = std::isfinite(pair.first) && pair.second <= pair.first;
    assert(high.state == (valid ? pair.first : 30));
    assert(low.state == (valid ? pair.second : 25));
    high.call(-50); assert(high.timers.size() == 1);
    high.call(60); assert(high.timers.empty()); assert(high.state == 60);
  }
#endif
}
"""


@pytest.mark.parametrize(
    "features",
    [
        [],
        ["NUMBER"],
        ["BINARY_SENSOR"],
        ["TEXT_SENSOR"],
        ["NUMBER", "BINARY_SENSOR", "TEXT_SENSOR"],
    ],
)
def test_driver(tmp_path: Path, features: list[str]) -> None:
    (tmp_path / "stub.h").write_text(STUB)
    headers = [
        "core/component.h",
        "core/defines.h",
        "core/optional.h",
        "core/preferences.h",
        "core/log.h",
    ]
    headers += [
        f"components/{x}/{x}.h"
        for x in ["i2c", "sensor", "number", "binary_sensor", "text_sensor"]
    ]
    for name in headers:
        path = tmp_path / "esphome" / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text('#include "stub.h"\n')
    source = tmp_path / "test.cpp"
    source.write_text(PROGRAM)
    executable = tmp_path / "test"
    subprocess.run(
        [
            "g++",
            "-std=c++20",
            *[f"-DUSE_TMP102_{x}" for x in features],
            "-I",
            str(tmp_path),
            "-I",
            str(ROOT),
            str(source),
            str(ROOT / "tmp102/tmp102.cpp"),
            "-o",
            str(executable),
        ],
        check=True,
    )
    subprocess.run([str(executable)], check=True)
