#include "serial_channel_call.h"
#include "serial_channel.h"
#include "esphome/core/log.h"

namespace esphome::serial_channel {

static const char *const TAG = "serial_channel.call";

SerialChannelCall &SerialChannelCall::set_data(const uint8_t *data, size_t len) {
  this->data_ = std::vector<uint8_t>(data, data + len);
  return *this;
}

SerialChannelCall &SerialChannelCall::set_data(const std::vector<uint8_t> &data) {
  this->data_ = data;
  return *this;
}

SerialChannelCall &SerialChannelCall::set_data(const std::string &base64_data) {
  // Decode base64
  std::vector<uint8_t> decoded = base64_decode(base64_data);
  this->data_ = decoded;
  return *this;
}

void SerialChannelCall::perform() {
  if (!this->data_.has_value()) {
    ESP_LOGW(TAG, "No data provided to SerialChannelCall");
    return;
  }

  const auto &data = this->data_.value();
  ESP_LOGD(TAG, "Sending %d bytes to '%s'", data.size(), this->parent_->get_name().c_str());
  this->parent_->control(data.data(), data.size());
}

}  // namespace esphome::serial_channel
