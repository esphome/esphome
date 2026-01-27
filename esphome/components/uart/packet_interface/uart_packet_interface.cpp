#include "uart_packet_interface.h"

namespace esphome {
namespace uart {

void UartPacketInterface::loop() {
  size_t cnt = this->available();
  if (cnt > this->rx_buffer_size_) {
    cnt = this->rx_buffer_size_;
  }
  if (cnt > 0) {
    // resize() will not reduce capacity, so rx_data_ buffer will grow to max used size and stay there
    this->rx_data_.resize(cnt);
    this->read_array(this->rx_data_.data(), cnt);
    this->on_receive_from_interface_(this->rx_data_, {});
    this->rx_data_.clear();
  }
}
bool UartPacketInterface::send_to_interface(const std::vector<uint8_t> &data, const PacketMetaData meta_data) {
  this->write_array(data.data(), data.size());
  return true;
}

}  // namespace uart
}  // namespace esphome
