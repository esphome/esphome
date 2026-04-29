#include "horizontal_airflow.h"

#include <algorithm>

#include <protocol/haier_protocol.h>

namespace esphome {
namespace haier {

void HorizontalAirflowSelect::control(const std::string &value) {
  hon_protocol::HorizontalSwingMode state;
  const auto &options = this->traits.get_options();
  auto item_it = std::find_if(options.begin(), options.end(), [&value](const char *opt) { return value == opt; });
  if (item_it == options.end()) {
    ESP_LOGE("haier", "Invalid horizontal airflow mode: %s", value.c_str());
    return;
  }
  state = HORIZONTAL_SWING_MODES_ORDER[item_it - options.begin()];
  const esphome::optional<hon_protocol::HorizontalSwingMode> current_state = this->parent_->get_horizontal_airflow();
  if (!current_state.has_value() || (current_state.value() != state)) {
    this->parent_->set_horizontal_airflow(state);
  }
  this->publish_state(value);
}

}  // namespace haier
}  // namespace esphome
