#include "tuya.h"
#include "esphome/core/log.h"

namespace esphome {
namespace tuya {

static const char *const TAG = "tuya";

void Tuya::dump_config() { ESP_LOGCONFIG(TAG, "Tuya:"); }

void Tuya::handle_datapoint_(const TuyaDatapoint &datapoint) {
  // Log datapoint update with value
  switch (datapoint.type) {
    case TuyaDatapointType::BOOLEAN:
      ESP_LOGD(TAG, "Datapoint %u update to %s", datapoint.id, ONOFF(datapoint.value_bool));
      break;
    case TuyaDatapointType::INTEGER:
      ESP_LOGD(TAG, "Datapoint %u update to %d", datapoint.id, datapoint.value_int);
      break;
    case TuyaDatapointType::STRING:
      ESP_LOGD(TAG, "Datapoint %u update to %s", datapoint.id, datapoint.value_string.c_str());
      break;
    case TuyaDatapointType::ENUM:
      ESP_LOGD(TAG, "Datapoint %u update to %u", datapoint.id, datapoint.value_enum);
      break;
    case TuyaDatapointType::BITMASK:
      ESP_LOGD(TAG, "Datapoint %u update to 0x%X", datapoint.id, datapoint.value_bitmask);
      break;
    case TuyaDatapointType::RAW: {
      char hex_buf[format_hex_pretty_size(MAX_DATAPOINT_LOG_BYTES)];
      ESP_LOGD(TAG, "Datapoint %u update to %s", datapoint.id,
               format_hex_pretty_to(hex_buf, datapoint.value_raw.data(), datapoint.value_raw.size()));
      break;
    }
    default:
      ESP_LOGD(TAG, "Datapoint %u update", datapoint.id);
      break;
  }

  // Update internal datapoints
  bool found = false;
  for (auto &dp : this->datapoints_) {
    if (dp.id == datapoint.id) {
      dp = datapoint;
      found = true;
      break;
    }
  }
  if (!found) {
    this->datapoints_.push_back(datapoint);
  }

  // Notify listeners
  for (auto &listener : this->listeners_) {
    if (listener.datapoint_id == datapoint.id) {
      listener.on_datapoint(datapoint);
    }
  }
}

optional<TuyaDatapoint> Tuya::get_datapoint_(uint8_t datapoint_id) {
  for (auto &datapoint : this->datapoints_) {
    if (datapoint.id == datapoint_id)
      return datapoint;
  }
  return {};
}

void Tuya::set_numeric_datapoint_value_(uint8_t datapoint_id, TuyaDatapointType datapoint_type, const uint32_t value,
                                        uint8_t length, bool forced) {
  ESP_LOGD(TAG, "Setting datapoint %u to %u", datapoint_id, value);
  optional<TuyaDatapoint> datapoint = this->get_datapoint_(datapoint_id);
  if (!datapoint.has_value() || datapoint->type != datapoint_type) {
    ESP_LOGW(TAG, "Datapoint %u not found or has incorrect type", datapoint_id);
  } else if (!forced &&
             std::find(this->ignore_mcu_update_on_datapoints_.begin(), this->ignore_mcu_update_on_datapoints_.end(),
                       datapoint_id) != this->ignore_mcu_update_on_datapoints_.end()) {
    ESP_LOGV(TAG, "Ignoring MCU update for datapoint %u", datapoint_id);
    return;
  } else {
    bool should_send = forced;
    switch (datapoint_type) {
      case TuyaDatapointType::BOOLEAN:
        should_send = should_send || datapoint->value_bool != value;
        break;
      case TuyaDatapointType::INTEGER:
        should_send = should_send || datapoint->value_int != static_cast<int>(value);
        break;
      case TuyaDatapointType::ENUM:
        should_send = should_send || datapoint->value_enum != value;
        break;
      case TuyaDatapointType::BITMASK:
        should_send = should_send || datapoint->value_bitmask != value;
        break;
      default:
        break;
    }
    if (!should_send) {
      ESP_LOGV(TAG, "Not sending unchanged value");
      return;
    }
  }

  std::vector<uint8_t> data;
  for (int i = length - 1; i >= 0; i--) {
    data.push_back((value >> (i * 8)) & 0xFF);
  }

  this->send_datapoint_command(datapoint_id, datapoint_type, data);
}

void Tuya::set_raw_datapoint_value_(uint8_t datapoint_id, const std::vector<uint8_t> &value, bool forced) {
  char hex_buf[format_hex_pretty_size(MAX_DATAPOINT_LOG_BYTES)];
  ESP_LOGD(TAG, "Setting datapoint %u to %s", datapoint_id, format_hex_pretty_to(hex_buf, value.data(), value.size()));
  optional<TuyaDatapoint> datapoint = this->get_datapoint_(datapoint_id);
  if (!datapoint.has_value() || datapoint->type != TuyaDatapointType::RAW) {
    ESP_LOGW(TAG, "Datapoint %u not found or has incorrect type", datapoint_id);
  } else if (!forced && datapoint->value_raw == value) {
    ESP_LOGV(TAG, "Not sending unchanged value");
    return;
  }

  this->send_datapoint_command(datapoint_id, TuyaDatapointType::RAW, value);
}

void Tuya::set_string_datapoint_value_(uint8_t datapoint_id, const std::string &value, bool forced) {
  ESP_LOGD(TAG, "Setting datapoint %u to %s", datapoint_id, value.c_str());
  optional<TuyaDatapoint> datapoint = this->get_datapoint_(datapoint_id);
  if (!datapoint.has_value() || datapoint->type != TuyaDatapointType::STRING) {
    ESP_LOGW(TAG, "Datapoint %u not found or has incorrect type", datapoint_id);
  } else if (!forced &&
             std::find(this->ignore_mcu_update_on_datapoints_.begin(), this->ignore_mcu_update_on_datapoints_.end(),
                       datapoint_id) != this->ignore_mcu_update_on_datapoints_.end()) {
    ESP_LOGV(TAG, "Ignoring MCU update for datapoint %u", datapoint_id);
    return;
  } else if (!forced && datapoint->value_string == value) {
    ESP_LOGV(TAG, "Not sending unchanged value");
    return;
  }

  std::vector<uint8_t> data;
  for (char const &c : value) {
    data.push_back(c);
  }

  this->send_datapoint_command(datapoint_id, TuyaDatapointType::STRING, data);
}

void Tuya::set_raw_datapoint_value(uint8_t datapoint_id, const std::vector<uint8_t> &value) {
  this->set_raw_datapoint_value_(datapoint_id, value, false);
}

void Tuya::set_boolean_datapoint_value(uint8_t datapoint_id, bool value) {
  this->set_numeric_datapoint_value_(datapoint_id, TuyaDatapointType::BOOLEAN, value, 1, false);
}

void Tuya::set_integer_datapoint_value(uint8_t datapoint_id, uint32_t value) {
  this->set_numeric_datapoint_value_(datapoint_id, TuyaDatapointType::INTEGER, value, 4, false);
}

void Tuya::set_string_datapoint_value(uint8_t datapoint_id, const std::string &value) {
  this->set_string_datapoint_value_(datapoint_id, value, false);
}

void Tuya::set_enum_datapoint_value(uint8_t datapoint_id, uint8_t value) {
  this->set_numeric_datapoint_value_(datapoint_id, TuyaDatapointType::ENUM, value, 1, false);
}

void Tuya::set_bitmask_datapoint_value(uint8_t datapoint_id, uint32_t value, uint8_t length) {
  this->set_numeric_datapoint_value_(datapoint_id, TuyaDatapointType::BITMASK, value, length, false);
}

void Tuya::force_set_raw_datapoint_value(uint8_t datapoint_id, const std::vector<uint8_t> &value) {
  this->set_raw_datapoint_value_(datapoint_id, value, true);
}

void Tuya::force_set_boolean_datapoint_value(uint8_t datapoint_id, bool value) {
  this->set_numeric_datapoint_value_(datapoint_id, TuyaDatapointType::BOOLEAN, value, 1, true);
}

void Tuya::force_set_integer_datapoint_value(uint8_t datapoint_id, uint32_t value) {
  this->set_numeric_datapoint_value_(datapoint_id, TuyaDatapointType::INTEGER, value, 4, true);
}

void Tuya::force_set_string_datapoint_value(uint8_t datapoint_id, const std::string &value) {
  this->set_string_datapoint_value_(datapoint_id, value, true);
}

void Tuya::force_set_enum_datapoint_value(uint8_t datapoint_id, uint8_t value) {
  this->set_numeric_datapoint_value_(datapoint_id, TuyaDatapointType::ENUM, value, 1, true);
}

void Tuya::force_set_bitmask_datapoint_value(uint8_t datapoint_id, uint32_t value, uint8_t length) {
  this->set_numeric_datapoint_value_(datapoint_id, TuyaDatapointType::BITMASK, value, length, true);
}

void Tuya::register_listener(uint8_t datapoint_id, const std::function<void(TuyaDatapoint)> &func) {
  auto listener = TuyaDatapointListener{
      .datapoint_id = datapoint_id,
      .on_datapoint = func,
  };
  this->listeners_.push_back(listener);

  // Run through existing datapoints
  for (auto &datapoint : this->datapoints_) {
    if (datapoint.id == datapoint_id)
      func(datapoint);
  }
}

}  // namespace tuya
}  // namespace esphome
