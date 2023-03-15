#ifdef USE_ARDUINO

#include "optolink_number.h"
#include "optolink.h"
#include "VitoWiFi.h"

namespace esphome {
namespace optolink {

void OptolinkNumber::control(float value) {
  if (value > traits.get_max_value() || value < traits.get_min_value()) {
    optolink_->set_error("datapoint value of number %s not in allowed range", get_sensor_name().c_str());
    ESP_LOGE("OptolinkNumber", "datapoint value of number %s not in allowed range", get_sensor_name().c_str());
  } else {
    ESP_LOGI("OptolinkNumber", "control of number %s to value %f", get_sensor_name().c_str(), value);
    update_datapoint_(value);
    publish_state(value);
  }
};

}  // namespace optolink
}  // namespace esphome

#endif
