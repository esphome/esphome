#include "uart_transport.h"

namespace esphome {
namespace uart {

void UartTransport::loop() {
  auto cnt = this->available();
  if (cnt > this->rx_buffer_size_) {
    cnt = this->rx_buffer_size_;
  }
  if (cnt > 0) {
    // resize() will not reduce capacity, so rx_data_ buffer will grow to max used size and stay there
    this->rx_data_.resize(cnt);
    this->read_array(this->rx_data_.data(), cnt);
    this->on_receive_data_(this->rx_data_);
    this->rx_data_.clear();
  }
}
bool UartTransport::send_data_(const std::vector<uint8_t> &data) {
  this->write_array(data.data(), data.size());
  return true;
}

}  // namespace uart
}  // namespace esphome
