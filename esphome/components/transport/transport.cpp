#include "transport.h"

namespace esphome {
namespace transport {
bool Transport::transmit(const std::vector<uint8_t> &data) {
  auto result = this->send_data_(data);
  ESP_LOGV(TAG, "send_data returns %s for data  %s", TRUEFALSE(result), format_hex_pretty(data).c_str());
  return result;
}

void Transport::on_receive_data_(const std::vector<uint8_t> &data) {
  ESP_LOGV(TAG, "Received data %s", format_hex_pretty(data.data(), data.size()).c_str());
  this->callback_.call(data);
}
}  // namespace transport
}  // namespace esphome
