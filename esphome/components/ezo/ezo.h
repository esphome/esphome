#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include <string>

namespace esphome::ezo {

ESPHOME_LOG_TAG(TAG, "ezo.sensor");

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

/// Longest command text, without the terminator. The longest built-in command is "Cal,high,<float>" (20);
/// custom commands such as "Name,<16 chars>" fit too. Longer custom commands are dropped.
static constexpr size_t EZO_MAX_COMMAND_LENGTH = 23;
/// Total command slots, including the one in flight. The periodic read does not take a slot.
static constexpr uint8_t EZO_COMMAND_QUEUE_SIZE = 4;

struct EzoCommand {
  char command[EZO_MAX_COMMAND_LENGTH + 1];
  uint16_t delay_ms;
  EzoCommandType command_type;
};

enum class EzoState : uint8_t {
  EZO_STATE_IDLE,
  EZO_STATE_IDLE_AFTER_READ,  // a pending read waits for one queued command first
  EZO_STATE_SENT_QUEUED,      // the queue's front command is waiting for its reply
  EZO_STATE_SENT_READ,
};

/// This class implements support for the EZO circuits in i2c mode
class EZOSensor final : public sensor::Sensor, public PollingComponent, public i2c::I2CDevice {
 public:
  void loop() override;
  void dump_config() override;
  void update() override;

  // I2C
  void set_address(uint8_t address);

  // Device Information
  void get_device_information();
  template<typename F> void add_device_infomation_callback(F &&callback) {
    this->device_infomation_callback_.add(std::forward<F>(callback));
  }

  // Sleep
  void set_sleep();

  // R
  void get_state();

  // Slope
  void get_slope();
  template<typename F> void add_slope_callback(F &&callback) { this->slope_callback_.add(std::forward<F>(callback)); }

  // T
  void get_t();
  void set_t(float value);
  void set_tempcomp_value(float temp);  // For backwards compatibility
  template<typename F> void add_t_callback(F &&callback) { this->t_callback_.add(std::forward<F>(callback)); }

  // Calibration
  void get_calibration();
  void set_calibration_point_low(float value);
  void set_calibration_point_mid(float value);
  void set_calibration_point_high(float value);
  void set_calibration_generic(float value);
  void clear_calibration();
  template<typename F> void add_calibration_callback(F &&callback) {
    this->calibration_callback_.add(std::forward<F>(callback));
  }

  // LED
  void get_led_state();
  void set_led_state(bool on);
  template<typename F> void add_led_state_callback(F &&callback) { this->led_callback_.add(std::forward<F>(callback)); }

  // Custom
  void send_custom(const std::string &to_send);
  template<typename F> void add_custom_callback(F &&callback) { this->custom_callback_.add(std::forward<F>(callback)); }

 protected:
  void add_command_(const char *command, EzoCommandType command_type, uint16_t delay_ms = 300);
  void send_next_();
  void finish_command_();

  void set_calibration_point_(EzoCalibrationType type, float value);

  CallbackManager<void(std::string)> device_infomation_callback_{};
  CallbackManager<void(std::string)> calibration_callback_{};
  CallbackManager<void(std::string)> slope_callback_{};
  CallbackManager<void(std::string)> t_callback_{};
  CallbackManager<void(std::string)> custom_callback_{};
  CallbackManager<void(bool)> led_callback_{};

  /// Queued commands; the front one stays in place while it is in flight. Reads are not queued.
  StaticRingBuffer<EzoCommand, EZO_COMMAND_QUEUE_SIZE> queue_;
  uint32_t start_time_{0};
  EzoState state_{EzoState::EZO_STATE_IDLE};
  bool read_pending_{false};  // a read was requested and is not yet sent
};

}  // namespace esphome::ezo
