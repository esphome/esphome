#include "esphome/core/log.h"
#include "tuya_select.h"

namespace esphome {
namespace tuya {

static const char *const TAG = "tuya.select";

void TuyaSelect::setup() {
  this->parent_->register_listener(this->select_id_, [this](const TuyaDatapoint &datapoint) {
    if (this->is_string_) {
      // STRING type: match value_string against option names
      ESP_LOGV(TAG, "MCU reported select %u string value %s", this->select_id_, datapoint.value_string.c_str());
      const auto &options = this->traits.get_options();
      for (size_t i = 0; i < options.size(); i++) {
        if (options[i] == datapoint.value_string) {
          this->publish_state(i);
          return;
        }
      }
      ESP_LOGW(TAG, "Invalid string value %s", datapoint.value_string.c_str());
    } else {
      // ENUM/INTEGER type: match value_enum against mappings
      uint8_t enum_value = datapoint.value_enum;
      ESP_LOGV(TAG, "MCU reported select %u value %u", this->select_id_, enum_value);
      auto mappings = this->mappings_;
      auto it = std::find(mappings.cbegin(), mappings.cend(), enum_value);
      if (it == mappings.end()) {
        ESP_LOGW(TAG, "Invalid value %u", enum_value);
        return;
      }
      size_t mapping_idx = std::distance(mappings.cbegin(), it);
      this->publish_state(mapping_idx);
    }
  });
}

void TuyaSelect::control(size_t index) {
  if (this->optimistic_)
    this->publish_state(index);

  if (this->is_string_) {
    // STRING type: send the option name as string
    const auto &option = this->option_at(index);
    ESP_LOGV(TAG, "Setting %u datapoint string value to %s", this->select_id_, option);
    this->parent_->set_string_datapoint_value(this->select_id_, option);
  } else {
    // ENUM/INTEGER type: send the mapped value
    uint8_t mapping = this->mappings_.at(index);
    ESP_LOGV(TAG, "Setting %u datapoint value to %u:%s", this->select_id_, mapping, this->option_at(index));
    if (this->is_int_) {
      this->parent_->set_integer_datapoint_value(this->select_id_, mapping);
    } else {
      this->parent_->set_enum_datapoint_value(this->select_id_, mapping);
    }
  }
}

void TuyaSelect::dump_config() {
  LOG_SELECT("", "Tuya Select", this);
  ESP_LOGCONFIG(TAG,
                "  Select has datapoint ID %u\n"
                "  Data type: %s\n"
                "  Options are:",
                this->select_id_, this->is_int_ ? "int" : "enum");
  const auto &options = this->traits.get_options();
  for (size_t i = 0; i < this->mappings_.size(); i++) {
    ESP_LOGCONFIG(TAG, "    %i: %s", this->mappings_.at(i), options.at(i));
  }
}

}  // namespace tuya
}  // namespace esphome
