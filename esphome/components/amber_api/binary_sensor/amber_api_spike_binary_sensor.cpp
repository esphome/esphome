#include "amber_api_spike_binary_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace amber_api {

static const char *const TAG = "amber_api.binary_sensor";

void AmberApiSpikeBinarySensor::on_amber_api_update(const AmberApiData &data) {
  // Spike status is "none", "potential", or "spike"
  // Binary sensor is ON when status is "spike"
  bool is_spike = data.spike_status == "spike";
  this->publish_state(is_spike);
}

void AmberApiSpikeBinarySensor::dump_config() { LOG_BINARY_SENSOR("", "Amber API Spike Status", this); }

}  // namespace amber_api
}  // namespace esphome
