#include "packet_interface.h"

namespace esphome::packet_interface {
void PacketInterface::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Packet Interface:\n"
                "  Max Packet Size: %zu",
                this->get_max_packet_size());
}

void PacketInterface::on_receive_from_interface_(const PacketBuffer &data, PacketMetaData meta_data) {
  // Convert to vector for logging
  std::vector<uint8_t> log_data = data.to_vector();
  char logbuf[80];
  ESP_LOGV(TAG, "Received data %s", format_hex_pretty_to(logbuf, log_data));
  this->callback_.call(data, meta_data);
}
}  // namespace esphome::packet_interface
