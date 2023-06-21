#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"
#include <deque>

namespace esphome {
namespace ezo {

static const char *const TAG = "ezo.sensor";

enum EzoCommandType : uint8_t {
  EZO_READ = 0,
  EZO_LED,
  EZO_DEVICE_INFORMATION,
  EZO_SLOPE,
  EZO_CALIBRATION,
  EZO_SLEEP,
  EZO_I2C,
  EZO_T,
  EZO_CUSTOM
};

enum EzoCalibrationType : uint8_t { EZO_CAL_LOW = 0, EZO_CAL_MID = 1, EZO_CAL_HIGH = 2 };

class EzoCommand {
 public:
  std::string command;
  uint16_t delay_ms = 0;
  bool command_sent = false;
  EzoCommandType command_type;
};

/// This class implements support for the EZO circuits in i2c mode
class EZOSensor : public sensor::Sensor, public PollingComponent, public i2c::I2CDevice {
 public:
  void loop() override;
  void dump_config() override;
  void update() override;
  float get_setup_priority() const override { return setup_priority::DATA; };

  // I2C
  void set_address(uint8_t address);

  // Device Information
  void get_device_information();
  void add_device_infomation_callback(std::function<void(std::string)> &&callback) {
    this->device_infomation_callback_.add(std::move(callback));
  }

  // Sleep
  void set_sleep();

  // R
  void get_state();

  // Slope
  void get_slope();
  void add_slope_callback(std::function<void(std::string)> &&callback) {
    this->slope_callback_.add(std::move(callback));
  }

  // T
  void get_t();
  void set_t(float value);
  void set_tempcomp_value(float temp);  // For backwards compatibility
  void add_t_callback(std::function<void(std::string)> &&callback) { this->t_callback_.add(std::move(callback)); }

  // Calibration
  void get_calibration();
  void set_calibration_point_low(float value);
  void set_calibration_point_mid(float value);
  void set_calibration_point_high(float value);
  void set_calibration_generic(float value);
  void clear_calibration();
  void add_calibration_callback(std::function<void(std::string)> &&callback) {
    this->calibration_callback_.add(std::move(callback));
  }

  // LED
  void get_led_state();
  void set_led_state(bool on);
  void add_led_state_callback(std::function<void(bool)> &&callback) { this->led_callback_.add(std::move(callback)); }

  // Custom
  void send_custom(const std::string &to_send);
  void add_custom_callback(std::function<void(std::string)> &&callback) {
    this->custom_callback_.add(std::move(callback));
  }

 protected:
  std::deque<std::unique_ptr<EzoCommand>> commands_;
  int new_address_;

  void add_command_(const std::string &command, EzoCommandType command_type, uint16_t delay_ms = 300);

  void set_calibration_point_(EzoCalibrationType type, float value);

  CallbackManager<void(std::string)> device_infomation_callback_{};
  CallbackManager<void(std::string)> calibration_callback_{};
  CallbackManager<void(std::string)> slope_callback_{};
  CallbackManager<void(std::string)> t_callback_{};
  CallbackManager<void(std::string)> custom_callback_{};
  CallbackManager<void(bool)> led_callback_{};

  uint32_t start_time_ = 0;
};

}  // namespace ezo
}  // namespace esphome
