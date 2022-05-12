#include "esphome/core/log.h"
#include "tuya_select.h"

namespace esphome {
namespace tuya {

static const char *const TAG = "tuya.select";

void TuyaSelect::setup() {
  this->parent_->register_listener(this->select_id_, [this](const TuyaDatapoint &datapoint) {
    uint8_t enum_value = datapoint.value_enum;
    ESP_LOGV(TAG, "MCU reported select %u value %u", this->select_id_, enum_value);
    auto value = this->at(enum_value);
    if (value.has_value()) {
      this->publish_state(value.value());
      return;
    }

    ESP_LOGW(TAG, "Invalid index %i", enum_value);
  });
}

void TuyaSelect::control(const std::string &value) {
  if (this->optimistic_)
    this->publish_state(value);

  auto idx = this->index_of(value);
  if (idx.has_value()) {
    ESP_LOGV(TAG, "Setting %i datapoint value to %i (%s)", this->select_id_, idx.value(), value.c_str());
    this->parent_->set_enum_datapoint_value(this->select_id_, idx.value());
    return;
  }

  ESP_LOGW(TAG, "Invalid value %s", value.c_str());
}

void TuyaSelect::dump_config() {
  LOG_SELECT("", "Tuya Select", this);
  ESP_LOGCONFIG(TAG, "  Select has datapoint ID %u", this->select_id_);
  ESP_LOGCONFIG(TAG, "  Options are:");
  auto options = this->traits.get_options();
  uint8_t i = 0;
  for (const std::string option : options) {
    ESP_LOGCONFIG(TAG, "    %i: %s", i++, option.c_str());
  }
}

}  // namespace tuya
}  // namespace esphome
