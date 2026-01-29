#include "key_collector_text_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace key_collector {

static const char *const TAG = "key_collector.text_sensor";

void KeyCollectorTextSensor::setup() {
  this->parent_->add_on_result_callback([this](std::string x, uint8_t start, uint8_t end) { this->publish_state(x); });
}

}  // namespace key_collector
}  // namespace esphome
