#ifdef USE_ARDUINO

#include "optolink_switch.h"
#include "optolink.h"
#include "VitoWiFi.h"

namespace esphome {
namespace optolink {

void OptolinkSwitch::write_state(bool value) {
  if (value != 0 && value != 1) {
    optolink_->set_error("datapoint value of switch %s not 0 or 1", get_sensor_name().c_str());
    ESP_LOGE("OptolinkSwitch", "datapoint value of switch %s not 0 or 1", get_sensor_name().c_str());
  } else {
    ESP_LOGI("OptolinkSwitch", "control of switch %s to value %d", get_sensor_name().c_str(), value);
    update_datapoint_(value);
    publish_state(value);
  }
};

}  // namespace optolink
}  // namespace esphome

#endif
