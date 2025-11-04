#pragma once

#include "one_wire_bus.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace one_wire {

#define LOG_ONE_WIRE_DEVICE(this) \
  ESP_LOGCONFIG(TAG, "  Address: %s (%s)", this->get_address_name().c_str(), \
                LOG_STR_ARG(this->bus_->get_model_str(this->address_ & 0xff)));

class OneWireDevice {
 public:
  /// @brief store the address of the device
  /// @param address of the device
  void set_address(uint64_t address) { this->address_ = address; }

  void set_index(uint8_t index) { this->index_ = index; }

  /// @brief store the pointer to the OneWireBus to use
  /// @param bus pointer to the OneWireBus object
  void set_one_wire_bus(OneWireBus *bus) { this->bus_ = bus; }

  /// Helper to create (and cache) the name for this sensor. For example "0xfe0000031f1eaf29".
  const std::string &get_address_name();

 protected:
  static constexpr uint8_t INDEX_NOT_SET = 255;

  uint64_t address_{0};
  uint8_t index_{INDEX_NOT_SET};
  OneWireBus *bus_{nullptr};  ///< pointer to OneWireBus instance
  std::string address_name_;

  /// @brief find an address if necessary
  /// should be called from setup
  bool check_address_or_index_();

  /// @brief send command on the bus
  /// @param cmd command to send
  bool send_command_(uint8_t cmd);
};

}  // namespace one_wire
}  // namespace esphome
