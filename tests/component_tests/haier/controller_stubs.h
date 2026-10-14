#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <optional>
#include <vector>
namespace esphome {
struct TestApp {
  uint32_t now{0};
  uint32_t get_loop_component_start_time() { return now; }
};
inline TestApp App;
constexpr size_t format_hex_pretty_size(size_t n) { return n * 3 + 1; }
class Component {
 public:
  virtual void setup() {}
  virtual void loop() {}
  virtual void dump_config() {}
  bool warning{false};
  void status_set_warning(const char *) { warning = true; }
  void status_clear_warning() { warning = false; }
  template<typename F> void set_interval(const char *, uint32_t, F) {}
};
namespace sensor {
class Sensor {
 public:
  float state{NAN};
  void publish_state(float x) { state = x; }
};
}  // namespace sensor
namespace text_sensor {
class TextSensor {
 public:
  void publish_state(const char *) {}
};
}  // namespace text_sensor
namespace binary_sensor {
class BinarySensor {
 public:
  std::vector<bool> states;
  void publish_state(bool x) { states.push_back(x); }
};
}  // namespace binary_sensor
namespace uart {
class UARTDevice {
 public:
  std::vector<std::vector<uint8_t>> sent;
  int available() { return 0; }
  bool read_byte(uint8_t *) { return false; }
  void write_array(const uint8_t *p, size_t n) { sent.emplace_back(p, p + n); }
};
}  // namespace uart
namespace water_heater {
enum WaterHeaterMode {
  WATER_HEATER_MODE_OFF,
  WATER_HEATER_MODE_ELECTRIC,
  WATER_HEATER_MODE_ECO,
  WATER_HEATER_MODE_PERFORMANCE
};
constexpr unsigned WATER_HEATER_STATE_ON = 1, WATER_HEATER_SUPPORTS_TARGET_TEMPERATURE = 2,
                   WATER_HEATER_SUPPORTS_OPERATION_MODE = 4, WATER_HEATER_SUPPORTS_ON_OFF = 8;
class WaterHeaterCall {
 public:
  std::optional<WaterHeaterMode> mode;
  std::optional<bool> on;
  float temperature{NAN};
  auto get_mode() const { return mode; }
  auto get_on() const { return on; }
  float get_target_temperature() const { return temperature; }
};
struct WaterHeaterCallInternal : WaterHeaterCall {
  explicit WaterHeaterCallInternal(void *) {}
};
class WaterHeaterTraits {
 public:
  void set_supports_current_temperature(bool) {}
  void add_feature_flags(unsigned) {}
  void set_supported_modes(std::initializer_list<WaterHeaterMode>) {}
  void set_min_temperature(float) {}
  void set_max_temperature(float) {}
  void set_target_temperature_step(float) {}
};
class WaterHeater {
 public:
  virtual WaterHeaterCallInternal make_call() = 0;
  virtual void control(const WaterHeaterCall &) = 0;
  virtual WaterHeaterTraits traits() = 0;
  bool is_on() const { return state_ & WATER_HEATER_STATE_ON; }
  void publish_state() {}
  void set_state_flag_(unsigned flag, bool x) {
    if (x)
      state_ |= flag;
    else
      state_ &= ~flag;
  }

 protected:
  float current_temperature_{NAN}, target_temperature_{NAN};
  unsigned state_{0};
  WaterHeaterMode mode_{WATER_HEATER_MODE_OFF};
};
}  // namespace water_heater
}  // namespace esphome
