#include "serial_channel.h"
#include "esphome/core/defines.h"
#include "esphome/core/controller_registry.h"
#include "esphome/core/log.h"

namespace esphome::serial_channel {

static const char *const TAG = "serial_channel";

void SerialChannel::publish_state(const uint8_t *data, size_t len) {
  // Encode to base64
  std::string base64 = base64_encode(data, len);
  this->publish_state(base64);
}

void SerialChannel::publish_state(const std::string &base64_data) {
  this->state_ = base64_data;
  ESP_LOGD(TAG, "'%s': Received %d bytes (base64: %s)", this->get_name().c_str(), base64_data.length(),
           base64_data.c_str());
  this->state_callback_.call(base64_data);
#if defined(USE_SERIAL_CHANNEL) && defined(USE_CONTROLLER_REGISTRY)
  ControllerRegistry::notify_serial_channel_update(this);
#endif
}

void SerialChannel::add_on_state_callback(std::function<void(const std::string &)> &&callback) {
  this->state_callback_.add(std::move(callback));
}

}  // namespace esphome::serial_channel
