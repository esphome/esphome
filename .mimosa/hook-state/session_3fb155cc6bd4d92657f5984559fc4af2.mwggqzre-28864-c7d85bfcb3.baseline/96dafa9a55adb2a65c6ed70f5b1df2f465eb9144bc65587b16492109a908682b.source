#include "esphome/core/log.h"
#include "tuya_select.h"

namespace esphome::tuya {

ESPHOME_LOG_TAG(TAG, "tuya.select");

void TuyaSelect::setup() {
  this->parent_->register_listener(this->select_id_, [this](const TuyaDatapoint &datapoint) {
    // The parent dispatches by datapoint ID alone; a report of the wrong
    // type would otherwise be read from the wrong union field
    auto is_numeric = [](TuyaDatapointType t) {
      return t == TuyaDatapointType::INTEGER || t == TuyaDatapointType::ENUM;
    };
    if (datapoint.type != this->datapoint_type_ && !(is_numeric(datapoint.type) && is_numeric(this->datapoint_type_))) {
      ESP_LOGW(TAG, "Select %u got a report of unexpected type %u", this->select_id_, (unsigned) datapoint.type);
      return;
    }
    if (this->datapoint_type_ == TuyaDatapointType::STRING) {
      // STRING type: match value_string against option names
      ESP_LOGV(TAG, "MCU reported select %u string value %s", this->select_id_, datapoint.value_string.c_str());
      // An option name can never contain a NUL byte, and index_of() takes a
      // NUL-terminated option table, so reject such payloads before lookup
      if (datapoint.value_string.find('\0') != std::string::npos) {
        ESP_LOGW(TAG, "Invalid string value containing NUL byte");
        return;
      }
      auto index = this->index_of(datapoint.value_string);
      if (index.has_value()) {
        this->publish_state(*index);
        return;
      }
      ESP_LOGW(TAG, "Invalid string value %s", datapoint.value_string.c_str());
    } else {
      // ENUM/INTEGER type: match value_enum against mappings
      uint8_t enum_value = datapoint.value_enum;
      ESP_LOGV(TAG, "MCU reported select %u value %u", this->select_id_, enum_value);
      const auto &mappings = this->mappings_;
      const auto *it = std::find(mappings.begin(), mappings.end(), enum_value);
      if (it == mappings.end()) {
        ESP_LOGW(TAG, "Invalid value %u", enum_value);
        return;
      }
      size_t mapping_idx = std::distance(mappings.begin(), it);
      this->publish_state(mapping_idx);
    }
  });
}

void TuyaSelect::control(size_t index) {
  if (this->optimistic_)
    this->publish_state(index);

  if (this->datapoint_type_ == TuyaDatapointType::STRING) {
    // STRING type: send the option name as string
    const char *option = this->option_at(index);
    ESP_LOGV(TAG, "Setting %u datapoint string value to %s", this->select_id_, option);
    if (this->optimistic_) {
      // The parent skips a value equal to the last MCU report, which an MCU that does
      // not report back leaves stale, so optimistic mode always sends
      this->parent_->force_set_string_datapoint_value(this->select_id_, option);
    } else {
      this->parent_->set_string_datapoint_value(this->select_id_, option);
    }
  } else {
    // ENUM/INTEGER type: send the mapped value
    uint8_t mapping = this->mappings_[index];
    ESP_LOGV(TAG, "Setting %u datapoint value to %u:%s", this->select_id_, mapping, this->option_at(index));
    if (this->datapoint_type_ == TuyaDatapointType::INTEGER) {
      this->parent_->set_integer_datapoint_value(this->select_id_, mapping);
    } else {
      this->parent_->set_enum_datapoint_value(this->select_id_, mapping);
    }
  }
}

static const LogString *datapoint_type_name(TuyaDatapointType type) {
  switch (type) {
    case TuyaDatapointType::STRING:
      return LOG_STR("string");
    case TuyaDatapointType::INTEGER:
      return LOG_STR("int");
    default:
      return LOG_STR("enum");
  }
}

void TuyaSelect::dump_config() {
  LOG_SELECT("", "Tuya Select", this);
  ESP_LOGCONFIG(TAG,
                "  Select has datapoint ID %u\n"
                "  Data type: %s\n"
                "  Options are:",
                this->select_id_, LOG_STR_ARG(datapoint_type_name(this->datapoint_type_)));
  const auto &options = this->traits.get_options();
  for (size_t i = 0; i < options.size(); i++) {
    if (this->datapoint_type_ == TuyaDatapointType::STRING) {
      ESP_LOGCONFIG(TAG, "    %s", options.at(i));
    } else {
      ESP_LOGCONFIG(TAG, "    %i: %s", this->mappings_[i], options.at(i));
    }
  }
}

}  // namespace esphome::tuya
