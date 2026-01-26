#include "packet_interface.h"

namespace esphome::packet_interface {
void PacketInterface::on_receive_from_interface_(const std::vector<uint8_t> &data, PacketMetaData meta_data) {
  char logbuf[80];
  ESP_LOGV(TAG, "Received data %s", format_hex_pretty_to(logbuf, data));
  this->callback_.call(data, meta_data);
}
}  // namespace esphome::packet_interface
