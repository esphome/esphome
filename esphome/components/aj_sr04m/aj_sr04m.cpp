#include "aj_sr04m.h"
#include "esphome/core/log.h"

namespace esphome {
namespace aj_sr04m {

static const char *const TAG = "aj_sr04m.sensor";

void Ajsr04mComponent::setup() { ESP_LOGCONFIG(TAG, "Setting up AJ_SR04M Sensor..."); }
void Ajsr04mComponent::update() {
  this->write(0x55);
  ESP_LOGV(TAG, "Request read out from sensor");

  while (this->available() == 4) {
    auto frame = *this->read_array<4>();
    auto checksum = (frame[0] + frame[1] + frame[2]) & 0x00FF;
    if ((frame[0] == 0xFF) && (checksum == frame[3])) {
      float value = ((frame[1] << 8) + frame[2]) / 1000.0;
      this->publish_state(value);
    } else {
      ESP_LOGW(TAG, "checksum failed: %02x != %02x", checksum, frame[3]);
    }
  }
}
void Ajsr04mComponent::dump_config() {
  LOG_SENSOR("", "AJ_SR04M Sensor", this);
  LOG_UPDATE_INTERVAL(this);
}
float Ajsr04mComponent::get_setup_priority() const { return setup_priority::DATA; }
}  // namespace aj_sr04m
}  // namespace esphome
