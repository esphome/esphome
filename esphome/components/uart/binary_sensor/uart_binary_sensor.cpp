#include "uart_binary_sensor.h"

namespace esphome::uart {

static const char *const TAG = "uart.binary_sensor";

size_t UARTBinarySensor::max_data_size_{};
std::vector<uint8_t> UARTBinarySensor::buffer_;

void UARTBinarySensor::setup() {
  static bool first = true;
  this->first_entity_ = first;
  first = false;
  if (this->max_data_size_ < this->data_.size()) {
    this->max_data_size_ = this->data_.size();
  }
}

void UARTBinarySensor::loop() {
  this->publish_state(false);
  // only first entry mange the buffer. The rest just check if matched.
  if (this->first_entity_) {
    if (this->buffer_.size() == this->max_data_size_) {
      this->buffer_.clear();
    }
    this->read_data_();
  }
  if (this->buffer_ == this->data_) {
    this->publish_state(true);
    this->buffer_.clear();
  }
}

void UARTBinarySensor::dump_config() { LOG_BINARY_SENSOR("", "UART Binary sensor", this); }

void UARTBinarySensor::read_data_() {
  if (!this->available()) {
    return;
  }
  uint8_t data;
  this->read_byte(&data);
  this->buffer_.push_back(data);
}

}  // namespace esphome::uart
