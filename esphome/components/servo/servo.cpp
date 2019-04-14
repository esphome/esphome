#include "servo.h"
#include "esphome/core/log.h"

namespace esphome {
namespace servo {

static const char *TAG = "servo";

uint32_t global_servo_id = 1911044085ULL;

void Servo::dump_config() {
  ESP_LOGCONFIG(TAG, "Servo:");
  ESP_LOGCONFIG(TAG, "  Idle Level: %.1f%%", this->idle_level_ * 100.0f);
  ESP_LOGCONFIG(TAG, "  Min Level: %.1f%%", this->min_level_ * 100.0f);
  ESP_LOGCONFIG(TAG, "  Max Level: %.1f%%", this->max_level_ * 100.0f);
}

}  // namespace servo
}  // namespace esphome
