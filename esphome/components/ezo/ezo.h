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

/// Longest command text, without the terminator. The longest documented command is the datasheet's
/// "Import, 59 6F 75 20 61 72" (25). Longer custom commands are dropped.
static constexpr size_t EZO_MAX_COMMAND_LENGTH = 25;
/// Total command slots, including the one in flight. The periodic read does not take a slot.
static constexpr uint8_t EZO_COMMAND_QUEUE_SIZE = 8;
/// Bytes shared by the texts of all queued custom commands, stored back to back without terminators.
static constexpr uint8_t EZO_CUSTOM_TEXT_SIZE = 64;

/// Each built-in command; the text is rendered only when it is sent. The order matches the tables in ezo.cpp.
enum class EzoCommandId : uint8_t {
  EZO_COMMAND_ID_DEVICE_INFORMATION,
  EZO_COMMAND_ID_SLEEP,
  EZO_COMMAND_ID_SLOPE,
  EZO_COMMAND_ID_T_GET,
  EZO_COMMAND_ID_CAL_GET,
  EZO_COMMAND_ID_CAL_CLEAR,
  EZO_COMMAND_ID_LED_GET,
  EZO_COMMAND_ID_LED_OFF,
  EZO_COMMAND_ID_LED_ON,
  EZO_COMMAND_ID_READ,     // READ to CAL_GENERIC wait 900 ms for the reply
  EZO_COMMAND_ID_CAL_LOW,  // CAL_LOW to I2C send their value
  EZO_COMMAND_ID_CAL_MID,
  EZO_COMMAND_ID_CAL_HIGH,
  EZO_COMMAND_ID_CAL_GENERIC,
  EZO_COMMAND_ID_T_SET,
  EZO_COMMAND_ID_I2C,
  EZO_COMMAND_ID_CUSTOM,  // the text is in EZOSensor::custom_text_
};

struct __attribute__((packed)) EzoCommand {
  EzoCommandId id;
  union __attribute__((packed)) {
    float value;          // CAL_* and T_SET value, or the I2C address
    uint8_t text_length;  // CUSTOM: length of its text in EZOSensor::custom_text_
  };
};
static_assert(sizeof(EzoCommand) == 5, "a queue slot holds the command id and one float");

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
  void add_command_(EzoCommandId id, float value = 0.0f);
  // Queues a command and links the sender; false when the queue is full
  bool enqueue_(const EzoCommand &command) {
    if (!this->queue_.push(command))
      return false;
    send_queued_fn = send_queued;
    this->enable_loop();
    return true;
  }
  void send_next_();
  static void send_queued(EZOSensor *self, uint32_t now);
  void finish_command_();

  CallbackManager<void(const std::string &)> device_infomation_callback_{};
  CallbackManager<void(const std::string &)> calibration_callback_{};
  CallbackManager<void(const std::string &)> slope_callback_{};
  CallbackManager<void(const std::string &)> t_callback_{};
  CallbackManager<void(const std::string &)> custom_callback_{};
  CallbackManager<void(bool)> led_callback_{};

  /// Set to send_queued on first queue, so poll-only configs do not link it or the command text tables.
  inline static void (*send_queued_fn)(EZOSensor *self, uint32_t now){nullptr};
  /// Queued commands; the front one stays in place while it is in flight. The periodic read is not queued.
  StaticRingBuffer<EzoCommand, EZO_COMMAND_QUEUE_SIZE> queue_;
  EzoState state_{EzoState::EZO_STATE_IDLE};
  uint16_t reply_due_{0};     // low 16 bits of when the reply can be read; every reply delay is under 1 s
  bool read_pending_{false};  // a periodic read was requested and is not yet sent
  /// Ring of queued custom texts in queue order; the oldest starts here, the queued slots hold the lengths.
  uint8_t custom_text_start_{0};
  char custom_text_[EZO_CUSTOM_TEXT_SIZE];
};

}  // namespace esphome::ezo
