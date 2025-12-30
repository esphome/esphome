#include "amber_api_descriptor_text_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace amber_api {

static const char *const TAG = "amber_api.text_sensor";

void AmberApiDescriptorTextSensor::on_amber_api_update(const AmberApiData &data) {
  if (!data.descriptor.empty()) {
    this->publish_state(data.descriptor);
  }
}

void AmberApiDescriptorTextSensor::dump_config() { LOG_TEXT_SENSOR("", "Amber API Price Descriptor", this); }

}  // namespace amber_api
}  // namespace esphome
