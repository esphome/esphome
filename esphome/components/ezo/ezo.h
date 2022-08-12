#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"
#include <deque>

namespace esphome {
namespace ezo {

enum EzoCommandType : uint8_t {
  EZO_READ = 0,
  EZO_LED = 1,
  EZO_DEVICE_INFORMATION = 2,
  EZO_SLOPE = 3,
  EZO_CALIBRATION,
  EZO_SLEEP = 4,
  EZO_I2C = 5,
  EZO_T = 6,
  EZO_CUSTOM = 7
};
static const char *const EZO_COMMAND_TYPE_STRINGS[] = {"EZO_READ",  "EZO_LED",         "EZO_DEVICE_INFORMATION",
                                                       "EZO_SLOPE", "EZO_CALIBRATION", "EZO_SLEEP",
                                                       "EZO_I2C",   "EZO_T",           "EZO_CUSTOM"};

enum EzoCalibrationType : uint8_t { EZO_CAL_LOW = 0, EZO_CAL_MID = 1, EZO_CAL_HIGH = 2 };
static const char *const EZO_CALIBRATION_TYPE_STRINGS[] = {"LOW", "MID", "HIGH"};

class EzoCommand {
 public:
  std::string command;
  uint16_t delay_ms = 0;
  bool command_sent = false;
  EzoCommandType command_type;
};

/// This class implements support for the EZO circuits in i2c mode
class EZOSensor : public sensor::Sensor, public PollingComponent, public i2c::I2CDevice {
 private:
  void set_calibration_point_(EzoCalibrationType type, float value) {
    std::string payload = str_sprintf("Cal,%s,%0.2f", EZO_CALIBRATION_TYPE_STRINGS[type], value);
    this->add_command(payload, EzoCommandType::EZO_CALIBRATION, 900);
  }

 public:
  void loop() override;
  void dump_config() override;
  void update() override;
  float get_setup_priority() const override { return setup_priority::DATA; };

  void add_command(const std::string &command, EzoCommandType command_type, uint16_t delay_ms = 300) {
    EzoCommand *ezo_command = new EzoCommand;
    ezo_command->command = command;
    ezo_command->command_type = command_type;
    ezo_command->delay_ms = delay_ms;
    this->commands_.push_back(ezo_command);
  };

  // R
  void get_state(int pos = 0) { this->add_command("R", EzoCommandType::EZO_READ, 900); }

  // I2C
  void set_i2c() { this->add_command("I2c,100", EzoCommandType::EZO_I2C); }  // NOLINT otherwise we get set_i2_c

  // Sleep
  void set_sleep() { this->add_command("Sleep", EzoCommandType::EZO_SLEEP); }

  // Calibration
  void set_calibration_point_low(float value) { this->set_calibration_point_(EzoCalibrationType::EZO_CAL_LOW, value); }
  void set_calibration_point_mid(float value) { this->set_calibration_point_(EzoCalibrationType::EZO_CAL_MID, value); }
  void set_calibration_point_high(float value) {
    this->set_calibration_point_(EzoCalibrationType::EZO_CAL_HIGH, value);
  }
  void get_calibration() { this->add_command("Cal,?", EzoCommandType::EZO_CALIBRATION); }
  void clear_calibration() { this->add_command("Cal,clear", EzoCommandType::EZO_CALIBRATION); }
  void add_calibration_callback(std::function<void(std::string)> &&callback) {
    this->calibration_callback_.add(std::move(callback));
  }

  // Device Information
  void get_device_information() { this->add_command("i", EzoCommandType::EZO_DEVICE_INFORMATION); }
  void add_device_infomation_callback(std::function<void(std::string)> &&callback) {
    this->device_infomation_callback_.add(std::move(callback));
  }

  // Slope
  void get_slope() { this->add_command("Slope,?", EzoCommandType::EZO_SLOPE); }
  void add_slope_callback(std::function<void(std::string)> &&callback) {
    this->slope_callback_.add(std::move(callback));
  }

  // LED
  void set_led_state(bool on) {
    std::string to_send = "L,";
    to_send += on ? "1" : "0";
    this->add_command(to_send, EzoCommandType::EZO_LED);
  }
  void get_led_state() { this->add_command("L,?", EzoCommandType::EZO_LED); }
  void add_led_state_callback(std::function<void(bool)> &&callback) { this->led_callback_.add(std::move(callback)); }

  // T
  void set_tempcomp_value(float temp) { this->set_t(temp); }  // For backwards compatibility
  void get_t() { this->add_command("T,?", EzoCommandType::EZO_T); }
  void set_t(float value) {
    std::string payload = str_sprintf("T,%0.2f", value);
    this->add_command(payload, EzoCommandType::EZO_T);
  }
  void add_t_callback(std::function<void(std::string)> &&callback) { this->t_callback_.add(std::move(callback)); }

  // Custom
  void send_custom(const std::string &to_send) { this->add_command(to_send, EzoCommandType::EZO_CUSTOM); }
  void add_custom_callback(std::function<void(std::string)> &&callback) {
    this->custom_callback_.add(std::move(callback));
  }

 protected:
  std::deque<EzoCommand *> commands_;

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
