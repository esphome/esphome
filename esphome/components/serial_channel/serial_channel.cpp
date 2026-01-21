#include "serial_channel.h"
#include "esphome/core/defines.h"
#include "esphome/core/controller_registry.h"
#include "esphome/core/log.h"

namespace esphome::serial_channel {

static const char *const TAG = "serial_channel";

void SerialChannel::setup() {}

void SerialChannel::publish_state(const uint8_t *data, size_t len) {
  // Encode to base64
  std::string base64 = base64_encode(data, len);
  this->publish_state(base64);
}

void SerialChannel::publish_state(const std::string &base64_data) {
  this->state_ = base64_data;
  ESP_LOGV(TAG, "'%s': Received %d bytes (base64: %s)", this->get_name().c_str(), base64_data.length(),
           base64_data.c_str());
  this->state_callback_.call(base64_data);
#if defined(USE_SERIAL_CHANNEL) && defined(USE_CONTROLLER_REGISTRY)
  ControllerRegistry::notify_serial_channel_update(this);
#endif
}

void SerialChannel::add_on_state_callback(std::function<void(const std::string &)> &&callback) {
  this->state_callback_.add(std::move(callback));
}

void SerialChannel::loop() {
  uint8_t byte;
  while (this->available()) {
    if (this->read_byte(&byte)) {
      this->rx_buffer_.push_back(byte);

      // Publish on buffer full, or no more data available
      if (this->rx_buffer_.full() || !this->available()) {
        break;
      }
    } else {
      // unexpected read failure, will be logged in read_byte
      break;
    }
    if (!this->rx_buffer_.empty()) {
      this->publish_state(this->rx_buffer_.begin(), this->rx_buffer_.size());
      this->rx_buffer_.clear();
    }
  }
}

void SerialChannel::dump_config() {
  ESP_LOGCONFIG(TAG, "Serial Channel '%s'", this->get_name().c_str());
  LOG_SERIAL_CHANNEL("  ", "Serial Channel", this);
  ESP_LOGCONFIG(TAG, "  Buffer Size: %d", this->rx_buffer_.capacity());
}

void SerialChannel::control(const uint8_t *data, size_t len) {
  // Write data to UART
  this->write_array(data, len);
  this->flush();
  ESP_LOGV(TAG, "'%s': Sent %d bytes to UART", this->get_name().c_str(), len);
}

}  // namespace esphome::serial_channel
