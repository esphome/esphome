#ifdef USE_ARDUINO

#include "optolink_number.h"
#include "../optolink.h"

namespace esphome {
namespace optolink {

static const char *const TAG = "optolink.number";

void OptolinkNumber::control(float value) {
  if (value > traits.get_max_value() || value < traits.get_min_value()) {
    set_optolink_state_("datapoint value of number %s not in allowed range", get_component_name().c_str());
    ESP_LOGE(TAG, "datapoint value of number %s not in allowed range", get_component_name().c_str());
  } else {
    ESP_LOGI(TAG, "control of number %s to value %f", get_component_name().c_str(), value);
    write_datapoint_value_(value);
    publish_state(value);
  }
};

void OptolinkNumber::datapoint_value_changed(uint8_t value) {
  if (traits.get_min_value() >= 0) {
    publish_state(value);
  } else {
    publish_state((int8_t) value);
  }
};

void OptolinkNumber::datapoint_value_changed(uint16_t value) {
  if (traits.get_min_value() >= 0) {
    publish_state(value);
  } else {
    publish_state((int16_t) value);
  }
};

void OptolinkNumber::datapoint_value_changed(uint32_t value) {
  if (traits.get_min_value() >= 0) {
    publish_state(value);
  } else {
    publish_state((int32_t) value);
  }
};

}  // namespace optolink
}  // namespace esphome

#endif
