#include "packet_interface.h"

namespace esphome {
namespace packet_interface {
bool PacketInterface::transmit(const std::vector<uint8_t> &data, const PacketMetaData) {
  auto result = this->send_data_(data);
  char logbuf[80];
  ESP_LOGV(TAG, "send_data returns %s for data  %s", TRUEFALSE(result), format_hex_pretty_to(logbuf, data));
  return result;
}

void PacketInterface::on_receive_data_(const std::vector<uint8_t> &data, PacketMetaData meta_data) {
  char logbuf[80];
  ESP_LOGV(TAG, "Received data %s", format_hex_pretty_to(logbuf, data));
  this->callback_.call(data);
}
}  // namespace packet_interface
}  // namespace esphome
