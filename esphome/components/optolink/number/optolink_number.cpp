#ifdef USE_ARDUINO

#include "optolink_number.h"
#include "../optolink.h"
#include "VitoWiFi.h"

namespace esphome {
namespace optolink {

static const char *const TAG = "optolink.number";

void OptolinkNumber::control(float value) {
  if (value > traits.get_max_value() || value < traits.get_min_value()) {
    set_optolink_state("datapoint value of number %s not in allowed range", get_component_name().c_str());
    ESP_LOGE(TAG, "datapoint value of number %s not in allowed range", get_component_name().c_str());
  } else {
    ESP_LOGI(TAG, "control of number %s to value %f", get_component_name().c_str(), value);
    write_datapoint_value(value);
    publish_state(value);
  }
};

}  // namespace optolink
}  // namespace esphome

#endif
