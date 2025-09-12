#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include <string>
//#include "esphome/core/automation.h"
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#ifdef USE_SELECT
#include "esphome/components/select/select.h"
#endif
#ifdef USE_NUMBER
#include "esphome/components/number/number.h"
#endif
#ifdef USE_SWITCH
#include "esphome/components/switch/switch.h"
#endif
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif

/** @brief Range result structure (min/max detection range). */
struct RangeResult {
  float min;
  float max;
};
using SRange = RangeResult;
/** @brief Sensitivity result structure (keep and trigger sensitivity). */
struct SensitivityResult {
  int keep;
  int trig;
};
using SenResult = SensitivityResult;

/** @brief Delay result structure (confirm and disappear delays in seconds). */
struct DelayResult {
  float confirm;
  float disappear;
};
using DelResult = DelayResult;

/**
 * @brief Motion data parsed from $DFDMD lines.
 * @param exist    presence flag (0/1)
 * @param distance measured distance (meters)
 * @param speed    measured speed (m/s)
 * @param valid    true if parsed successfully
 */
struct MotionData {
  int exist;       ///< presence flag (0/1)
  float distance;  ///< distance in meters
  float speed;     ///< speed in m/s
  bool valid;      ///< parsing result flag
};
using MotData = MotionData;

/** @brief Running mode of the device. */
enum MotionMode {
  MODE_UNKNOWN = -1,
  MODE_MOTION = 0,
  MODE_SPEED = 1
};

namespace esphome {
namespace dfrobot_c4001 {

class C4001Listener {
 public:
  virtual void on_presence(bool presence){};
  virtual void on_distance(float distance){};
  virtual void on_speed(float speed){};
};


/**
 * @brief Main component for the DFRobot C4001 device.
 *
 * This class handles UART communication, parsing, and publishing to
 * Home Assistant via child components (sensors, binary sensors, numbers, switches).
 */
class c4001Component : public Component, public uart::UARTDevice {
 public:
  explicit c4001Component(uart::UARTComponent *parent = nullptr) : uart::UARTDevice(parent) {}

  /** Lifecycle hooks */
  void setup() override;
  void loop() override;

  /** UART helpers */
  void uart_clear_buffer();
  size_t uart_read_raw(char *buf, size_t bufsize, uint32_t timeout_ms = 200);

  /** Debug / configuration helpers */
  void print_config();
  int get_run_mode();

  /** Data accessors for child components */
  float get_speed() { return speed_; }
  float get_distance() { return distance_; }
  bool get_exist() { return exist_; }

  /** Methods used by child components to publish state */
  void publish_exist();
  void publish_speed();
  void publish_distance();

  /** Range configuration interface */
  void set_min_range(float value);
  void set_max_range(float value);
  void set_trig_range(float value);

  /** Command helper (sends a command string with parameters) */
  void send_cmd_with_param(const char *cmd);

  /** Sensor control helpers */
  bool sensor_stop();
  void save_config();
  void sensor_start();

  /** Query helpers for device settings */
  float get_trig_uart();
  SRange get_range_uart();

  float get_min_range() const { return min_range_; }
  float get_max_range() const { return max_range_; }
  float get_trig_range() const { return trig_range_; }

  /** Sensitivity interface */
  void set_keep_sensitivity(int value);
  void set_trig_sensitivity(int value);
  SenResult getSensitivity();

  /** Delay interface */
  void set_confirm_delay(float value);
  void set_disappear_delay(float value);
  DelResult get_delay_uart();

  /** Threshold factor interface */
  void set_threshold_factor(int value);
  int get_threshold_uart();

  /** Micro-switch control */
  void set_micro_switch_state(bool state);
  int get_micro_uart();

  /** Operating mode setter (expects "motion" or "speed") */
  void set_operating_mode(const std::string &state);

  /** Refresh numbers and device-derived values */
  void update_config_param();

  /** Read and parse incoming UART data according to current mode */
  void get_data();

  void register_listener(C4001Listener *listener) { this->listeners_.push_back(listener); }
#ifdef USE_NUMBER
      /** Setters for Number child entities (Home Assistant number entities) */
      void
      set_min_range_number(number::Number *number) {
    this->min_range_number_ = number;
  }
  void set_max_range_number(number::Number *number) { this->max_range_number_ = number; }
  void set_trig_range_number(number::Number *number) { this->trig_range_number_ = number; }
  void set_keep_sensitivity_number(number::Number *number) { this->keep_sensitivity_number_ = number; }
  void set_trig_sensitivity_number(number::Number *number) { this->trig_sensitivity_number_ = number; }
  void set_confirm_delay_number(number::Number *number) { this->confirm_delay_number_ = number; }
  void set_disappear_delay_number(number::Number *number) { this->disappear_delay_number_ = number; }
  void set_threshold_factor_number(number::Number *number) { this->threshold_factor_number_ = number; }
#endif

#ifdef USE_SWITCH
  void set_motion_switch(switch_::Switch *sw) { this->motion_switch_ = sw; }
#endif

#ifdef USE_SELECT
  void set_operating_mode_select(select::Select *selector) { this->operating_selector_ = selector; };
#endif

#ifdef USE_SENSOR
  //void set_speed_sensor(sensor::Sensor *speed_sensor) { speed_sensor_ = speed_sensor; }
  //void set_distance_sensor(sensor::Sensor *distance_sensor) { distance_sensor_ = distance_sensor; }
#endif

#ifdef USE_BINARY_SENSOR
  //void set_exist_sensor(binary_sensor::BinarySensor *exist_sensor) { exist_sensor_ = exist_sensor; }
#endif

 protected:
  /** UART raw reader (implemented in .cpp) */
  // size_t uart_read_raw(char *buf, size_t bufsize, uint32_t timeout_ms = 200);

  /** Detection range defaults (meters) */
  float min_range_{0.6f};    ///< default minimum detection range
  float max_range_{6.0f};    ///< default maximum detection range
  float trig_range_{6.0f};   ///< default trigger range

  /** Sensitivity defaults (unitless, device specific scale) */
  int keep_sensitivity_{7};  ///< default keep sensitivity
  int trig_sensitivity_{7};  ///< default trigger sensitivity

  /** Delay defaults (seconds) */
  float confirm_delay_{0.5f};   ///< default confirmation delay (s)
  float disappear_delay_{15.0f};///< default disappearance delay (s)

  /** Threshold factor default */
  int threshold_factor_{5}; ///< default threshold factor

#ifdef USE_NUMBER
  number::Number *min_range_number_{nullptr};
  number::Number *max_range_number_{nullptr};
  number::Number *trig_range_number_{nullptr};
  number::Number *keep_sensitivity_number_{nullptr};
  number::Number *trig_sensitivity_number_{nullptr};
  number::Number *confirm_delay_number_{nullptr};
  number::Number *disappear_delay_number_{nullptr};
  number::Number *threshold_factor_number_{nullptr};
#endif

  /** Micro motion (hardware) state */
  int micro_motion_{0};
#ifdef USE_SWITCH
  switch_::Switch *motion_switch_{nullptr};
#endif

  /** Current run mode (0 = motion, 1 = speed) */
  int run_mode_{0};
#ifdef USE_SELECT
  select::Select *operating_selector_{nullptr};
#endif

  /** Measured values */
  float speed_{0.0f};
  float distance_{0.0f};
#ifdef USE_SENSOR
  //sensor::Sensor *speed_sensor_{nullptr};
  //sensor::Sensor *distance_sensor_{nullptr};
#endif

  /** Presence flag parsed from device output */
  bool exist_{false};
#ifdef USE_BINARY_SENSOR
  //binary_sensor::BinarySensor *exist_sensor_{nullptr};
#endif

  /** Timing and internal helpers */
  uint32_t update_interval_{1000}; ///< default update interval (ms)
  uint32_t last_update_{0};

  uint8_t test_value_{0};  ///< test value (0-255)
  std::vector<C4001Listener *> listeners_{};

};

}  // namespace dfrobot_c4001
}  // namespace esphome
