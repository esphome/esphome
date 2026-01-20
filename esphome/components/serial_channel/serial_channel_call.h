#pragma once

#include "esphome/core/helpers.h"
#include <vector>
#include <string>

namespace esphome::serial_channel {

class SerialChannel;

class SerialChannelCall {
 public:
  explicit SerialChannelCall(SerialChannel *parent) : parent_(parent) {}

  SerialChannelCall &set_data(const uint8_t *data, size_t len);
  SerialChannelCall &set_data(const std::vector<uint8_t> &data);
  SerialChannelCall &set_data(const std::string &base64_data);

  void perform();

 protected:
  SerialChannel *parent_;
  optional<std::vector<uint8_t>> data_;
};

}  // namespace esphome::serial_channel
