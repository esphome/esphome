#include "one_wire.h"

namespace esphome {
namespace one_wire {

static const char *const TAG = "one_wire";

const std::string &OneWireDevice::get_address_name() {
  if (this->address_name_.empty())
    this->address_name_ = std::string("0x") + format_hex(this->address_);
  return this->address_name_;
}

std::string OneWireDevice::unique_id() { return "dallas-" + str_lower_case(format_hex(this->address_)); }

bool OneWireDevice::send_command_(uint8_t cmd) {
  if (!this->bus_->select(this->address_))
    return false;
  this->bus_->write8(cmd);
  return true;
}

bool OneWireDevice::check_address_() {
  if (this->address_ != 0)
    return true;
  auto devices = this->bus_->get_devices();
  if (devices.empty()) {
    ESP_LOGE(TAG, "No devices, can't auto-select address");
    return false;
  }
  if (devices.size() > 1) {
    ESP_LOGE(TAG, "More than one device, can't auto-select address");
    return false;
  }
  this->address_ = devices[0];
  return true;
}

}  // namespace one_wire
}  // namespace esphome
