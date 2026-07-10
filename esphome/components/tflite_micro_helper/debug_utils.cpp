#include "debug_utils.h"
#include "esphome/core/log.h"

namespace esphome::tflite_micro_helper {

ScopedDuration::ScopedDuration(const char *tag) : tag_(tag), start_(esphome::millis()) {}

void ScopedDuration::log_duration(const char *func) {
  ESP_LOGD(this->tag_, "%s duration: %ums", func, this->elapsed());
}

void ScopedDuration::log(const char *msg, uint32_t val) { ESP_LOGD(this->tag_, "%s: %ums", msg, val); }

}  // namespace esphome::tflite_micro_helper
