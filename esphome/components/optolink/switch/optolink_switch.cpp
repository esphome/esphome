#ifdef USE_ARDUINO

#include "optolink_switch.h"
#include "../optolink.h"

namespace esphome {
namespace optolink {

static const char *const TAG = "optolink.switch";

void OptolinkSwitch::write_state(bool value) {
  if (value != 0 && value != 1) {
    ESP_LOGE(TAG, "datapoint value of switch %s not 0 or 1", get_component_name().c_str());
  } else {
    ESP_LOGI(TAG, "control of switch %s to value %d", get_component_name().c_str(), value);
    write_datapoint_value_((uint8_t) value);
    publish_state(value);
  }
};

}  // namespace optolink
}  // namespace esphome

#endif
