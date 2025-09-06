#include "modbus_select.h"
#include "esphome/core/log.h"

namespace esphome {
namespace modbus_controller {

static const char *const TAG = "modbus_controller.select";

void ModbusSelect::dump_config() { LOG_SELECT(TAG, "Modbus Controller Select", this); }

void ModbusSelect::parse_and_publish(const std::vector<uint8_t> &data) {
  int64_t value = payload_to_number(data, this->sensor_value_type, this->offset, this->bitmask);

  ESP_LOGD(TAG, "New select value %lld from payload", value);

  optional<std::string> new_state;

  if (this->transform_func_.has_value()) {
    auto val = (*this->transform_func_)(this, value, data);
    if (val.has_value()) {
      new_state = *val;
      ESP_LOGV(TAG, "lambda returned option %s", new_state->c_str());
    }
  }

  if (!new_state.has_value()) {
    auto map_it = std::find(this->mapping_.cbegin(), this->mapping_.cend(), value);

    if (map_it != this->mapping_.cend()) {
      size_t idx = std::distance(this->mapping_.cbegin(), map_it);
      new_state = this->traits.get_options()[idx];
      ESP_LOGV(TAG, "Found option %s for value %lld", new_state->c_str(), value);
    } else {
      ESP_LOGE(TAG, "No option found for mapping %lld", value);
    }
  }

  if (new_state.has_value()) {
    this->publish_state(new_state.value());
  }
}

void ModbusSelect::control(const std::string &value) {
  auto options = this->traits.get_options();
  auto opt_it = std::find(options.cbegin(), options.cend(), value);
  size_t idx = std::distance(options.cbegin(), opt_it);
  optional<int64_t> mapval = this->mapping_[idx];
  ESP_LOGD(TAG, "Found value %lld for option '%s'", *mapval, value.c_str());

  std::vector<uint16_t> data;

  if (this->write_transform_func_.has_value()) {
    auto val = (*this->write_transform_func_)(this, value, *mapval, data);
    if (val.has_value()) {
      mapval = *val;
      ESP_LOGV(TAG, "write_lambda returned mapping value %lld", *mapval);
    } else {
      ESP_LOGD(TAG, "Communication handled by write_lambda - exiting control");
      return;
    }
  }

  if (data.empty()) {
    number_to_payload(data, *mapval, this->sensor_value_type);
  } else {
    ESP_LOGV(TAG, "Using payload from write lambda");
  }

  if (data.empty()) {
    ESP_LOGW(TAG, "No payload was created for updating select");
    return;
  }

  const uint16_t write_address = this->start_address + this->offset / 2;
  ModbusCommandItem write_cmd;
  if ((this->register_count == 1) && (!this->use_write_multiple_)) {
    write_cmd = ModbusCommandItem::create_write_single_command(this->parent_, write_address, data[0]);
  } else {
    write_cmd =
        ModbusCommandItem::create_write_multiple_command(this->parent_, write_address, this->register_count, data);
  }

  this->parent_->queue_command(write_cmd);

  if (this->optimistic_)
    this->publish_state(value);
}

}  // namespace modbus_controller
}  // namespace esphome
