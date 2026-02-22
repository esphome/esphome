#include "uart_binary_sensor.h"

namespace esphome::uart {

static const char *const TAG = "uart.binary_sensor";

size_t UARTBinarySensor::max_data_size{};
std::vector<uint8_t> UARTBinarySensor::buffer;

void UARTBinarySensor::setup() {
  static bool first = true;
  this->first_entity_ = first;
  first = false;
  if (max_data_size < this->data_.size()) {
    max_data_size = this->data_.size();
  }
}

void UARTBinarySensor::loop() {
  this->publish_state(false);
  // only first entry mange the buffer. The rest just check if data match.
  if (this->first_entity_) {
    if (buffer.size() == max_data_size) {
      buffer.erase(buffer.begin());
    }
    this->read_data_();
  }
  if (buffer.size() >= this->data_.size() &&
      // for different size need to compare end of buffer with data
      std::equal(this->data_.begin(), this->data_.end(), buffer.end() - this->data_.size())) {
    this->publish_state(true);
    buffer.clear();
  }
}

void UARTBinarySensor::dump_config() { LOG_BINARY_SENSOR("", "UART Binary sensor", this); }

void UARTBinarySensor::read_data_() {
  if (!this->available()) {
    return;
  }
  uint8_t data;
  this->read_byte(&data);
  buffer.push_back(data);
}

}  // namespace esphome::uart
