#include "packet_interface.h"
#include "esphome/core/log.h"

namespace esphome::packet_interface {

ESPHOME_LOG_TAG(TAG, "packet_interface");

// Packets in a row that cannot be understood before the mismatch is logged
static constexpr uint8_t MISMATCH_COUNT = 10;

void PacketInterface::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Packet Interface '%s':\n"
                "  Max Packet Size: %zu",
                this->name_, this->get_max_packet_size());
}

void PacketInterface::on_receive_from_interface_(const PacketBuffer &data, const PacketMetaData &meta_data) {
#ifdef ESPHOME_LOG_HAS_VERBOSE
  static constexpr size_t LOG_BYTES = 24;
  uint8_t head[LOG_BYTES];
  size_t len = data.copy_to(head, LOG_BYTES);
  ESP_LOGV_HEX(TAG, head, len, LOG_BYTES, "%s: received %zu bytes %s", this->name_, data.size());
#endif
  this->callback_.call(data, meta_data);
}

void PacketInterface::note_received_(bool understood) {
  if (understood) {
    this->not_understood_count_ = 0;
  } else if (this->not_understood_count_ < MISMATCH_COUNT) {
    this->not_understood_count_++;
    if (this->not_understood_count_ == MISMATCH_COUNT) {
      ESP_LOGW(TAG, "%s: %u packets in a row could not be understood; check that both ends have the same layers",
               this->name_, MISMATCH_COUNT);
    }
  }
}
}  // namespace esphome::packet_interface
