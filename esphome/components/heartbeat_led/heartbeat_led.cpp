#include "heartbeat_led.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

namespace esphome {
namespace heartbeat_led {

static const char *TAG = "heartbeat_led";

HeartbeatLED *global_heartbeat_led = nullptr;

HeartbeatLED::HeartbeatLED(GPIOPin *pin) : pin_(pin) { global_heartbeat_led = this; }
void HeartbeatLED::pre_setup() {
  ESP_LOGCONFIG(TAG, "Setting up Heartbeat LED...");
  this->pin_->setup();
  this->pin_->digital_write(false);
}
void HeartbeatLED::dump_config() {
  ESP_LOGCONFIG(TAG, "Heartbeat LED:");
  LOG_PIN("  Pin: ", this->pin_);
}
void HeartbeatLED::loop() {
  this->pin_->digital_write(millis() % 1500u < 250u);
}
float HeartbeatLED::get_setup_priority() const { return setup_priority::HARDWARE; }
float HeartbeatLED::get_loop_priority() const { return 50.0f; }

}  // namespace heartbeat_led
}  // namespace esphome
