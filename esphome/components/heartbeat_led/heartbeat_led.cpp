#include "heartbeat_led.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

namespace esphome {
namespace heartbeat_led {

static const char *TAG = "heartbeat_led";

int ledState = false;                 // ledState used to set the LED
unsigned long previousMillis = 0;   // will store last time LED was updated
const long interval = 1000;         // interval at which to blink (milliseconds)

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
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= interval) {
    // save the last time you blinked the LED
    previousMillis = currentMillis;

    // if the LED is off turn it on and vice-versa:
    if (ledState == false) {
      ledState = true;
    } else {
      ledState = false;
    }

    // set the LED with the ledState of the variable:
    this->pin_->digitalWrite(ledState);
  }
}
float HeartbeatLED::get_setup_priority() const { return setup_priority::HARDWARE; }
float HeartbeatLED::get_loop_priority() const { return 50.0f; }

}  // namespace heartbeat_led
}  // namespace esphome
