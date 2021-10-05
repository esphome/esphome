#pragma once
#ifdef USE_UART_DEBUGGER

#include <vector>
#include "esphome/core/defines.h"
#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "uart_component.h"

namespace esphome {
namespace uart {

class UARTDebugger : public Component, public Trigger<UARTDirection, std::vector<uint8_t>> {
 public:
  explicit UARTDebugger(UARTComponent *parent) {
    parent->add_debug_callback([this](UARTDirection direction, uint8_t byte) {
      if (!this->is_my_direction_(direction)) { return; }
      if (this->is_recursive_()) { return; }
      if (has_buffered_bytes_() && this->direction_changed_(direction)) {
        this->fire_trigger_();
      }
      this->store_byte_(direction, byte);
      this->trigger_after_delmiter(byte) ||
      this->trigger_after_bytes_();
    });
  }

  void loop() {
    if (this->has_buffered_bytes_()) {
      this->trigger_after_timeout_();
    }
  }

  void set_direction(UARTDirection direction) { this->for_direction_ = direction; }
  void set_after_bytes(size_t size) { this->after_bytes_ = size; }
  void set_after_timeout(uint32_t timeout) { this->after_timeout_ = timeout; }
  void add_delimiter_byte(uint8_t byte) { this->after_delimiter_.push_back(byte); }

 protected:
  UARTDirection for_direction_;
  UARTDirection last_direction_{};
  std::vector<uint8_t> bytes_{};
  size_t after_bytes_;
  uint32_t after_timeout_;
  unsigned long last_time_{};
  std::vector<uint8_t> after_delimiter_{};
  size_t after_delimiter_pos_{};
  bool is_triggering_{false};

  bool is_my_direction_(UARTDirection direction) {
    return this->for_direction_ == UART_DIRECTION_BOTH ||
           this->for_direction_ == direction;
  }

  bool is_recursive_() {
    return this->is_triggering_;
  }

  bool direction_changed_(UARTDirection direction) {
    return this->for_direction_ == UART_DIRECTION_BOTH &&
           this->last_direction_ != direction;
  }

  void store_byte_(UARTDirection direction, uint8_t byte) {
    this->bytes_.push_back(byte);
    this->last_direction_ = direction;
    this->last_time_ = millis();
  }

  bool trigger_after_delmiter(uint8_t byte) {
    if (this->after_delimiter_.size() > 0) {
      if (this->after_delimiter_[this->after_delimiter_pos_] == byte) {
        this->after_delimiter_pos_++;
        if (this->after_delimiter_pos_ == this->after_delimiter_.size()) {
          this->fire_trigger_();
          this->after_delimiter_pos_ = 0;
          return true;
        }
      } else {
        this->after_delimiter_pos_ = 0;
      }
    }
    return false;
  }

  bool trigger_after_bytes_() {
    if (this->after_bytes_ > 0 && this->bytes_.size() >= this->after_bytes_) {
      this->fire_trigger_();
      return true;
    }
    return false;
  }

  bool trigger_after_timeout_() {
    if (this->after_timeout_ > 0 && millis() - this->last_time_ >= this->after_timeout_) {
      this->fire_trigger_();
      return true;
    }
    return false;
  }

  bool has_buffered_bytes_() {
    return this->bytes_.size() > 0;
  }

  void fire_trigger_() {
    this->is_triggering_ = true;
    trigger(this->last_direction_, this->bytes_);
    this->bytes_.clear();
    this->is_triggering_ = false;
  }
};

/// This UARTDevice is used by the serial debugger to read data from a
/// serial interface when the 'dummy_receiver' option is enabled.
/// The data are not stored, nor processed. This is most useful when the
/// debugger is used to reverse engineer a serial protocol, for which no
/// specific UARTDevice implementation exists (yet), but for which the
/// incoming bytes must be read to drive the debugger.
class UARTDummyReceiver : public Component, public UARTDevice {
 public:
  UARTDummyReceiver(UARTComponent *parent) : UARTDevice(parent) {}
  void loop() override {
    // Reading up to a limited number of bytes, to make sure that this loop()
    // won't lock up the system on a continuous incoming stream of bytes.
    uint8_t data;
    int count = 50;
    while (this->available() && count--) {
      this->read_byte(&data);
    }
  }
};

}  // namespace uart
}  // namespace esphome
#endif
