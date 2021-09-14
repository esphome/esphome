#include "esphome/core/log.h"
#include "tuya_select.h"

namespace esphome {
namespace tuya {

static const char *const TAG = "tuya.select";

void TuyaSelect::setup() {
  this->parent_->register_listener(this->select_id_, [this](const TuyaDatapoint &datapoint) {
    const std::string& value = this->enum_to_value(datapoint.value_enum);
    ESP_LOGV(TAG, "MCU reported select %u is: %d (%s)", this->select_id_, datapoint.value_enum, &value);
    this->publish_state(value);
  });
}

void TuyaSelect::control(const std::string &value)  {
  std::uint8_t value_enum = this->value_to_enum(value);
  ESP_LOGV(TAG, "Setting select %u: %s (%d)", this->select_id_, &value, value_enum);
  this->parent_->set_enum_datapoint_value(this->select_id_, value_enum);
  this->publish_state(value);
}

void TuyaSelect::dump_config() {
  LOG_SELECT("", "Tuya Select", this);
  ESP_LOGCONFIG(TAG, "  Select has datapoint ID %u", this->select_id_);
}

const std::string& TuyaSelect::enum_to_value(uint8_t enum_value) {
  auto it = std::find(this->enums_.begin(), this->enums_.end(), enum_value);

  // If element was found
  if (it != this->enums_.end())
  {
      auto index = it - this->enums_.begin();
      auto options = this->traits.get_options();
      return options.at(index);
  }
  else {
    return error_not_found;
  }
}

std::uint8_t TuyaSelect::value_to_enum(const std::string& value){
  auto options = this->traits.get_options();
  auto it = std::find(options.begin(), options.end(), value);

  // If element was found
  if (it != options.end())
  {
      auto index = it - options.begin();
      return this->enums_.at(index);
  }
  else {
    return 0;
  }
}


}  // namespace tuya
}  // namespace esphome
