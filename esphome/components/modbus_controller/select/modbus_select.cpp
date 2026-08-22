#include "modbus_select.h"
#include "esphome/core/log.h"

namespace esphome::modbus_controller {

static const char *const TAG = "modbus_controller.select";

void ModbusSelect::dump_config() { LOG_SELECT(TAG, "Modbus Controller Select", this); }

void ModbusSelect::parse_and_publish(std::span<const uint8_t> data) {
  int64_t value =
      modbus::helpers::payload_to_number(data, this->sensor_value_type, this->offset, this->bitmask).value_or(0);

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
      ESP_LOGV(TAG, "Found option %s for value %lld", this->option_at(idx), value);
      this->publish_state(idx);
      return;
    } else {
      ESP_LOGE(TAG, "No option found for mapping %lld", value);
    }
  }

  if (new_state.has_value()) {
    this->publish_state(new_state.value());
  }
}

void ModbusSelect::control(size_t index) {
  optional<int64_t> mapval = this->mapping_[index];
  const char *option = this->option_at(index);
  ESP_LOGD(TAG, "Found value %lld for option '%s'", *mapval, option);

  std::vector<uint16_t> data;

  if (this->write_transform_func_.has_value()) {
    // Transform func requires string parameter for backward compatibility
    auto val = (*this->write_transform_func_)(this, std::string(option), *mapval, data);
    if (val.has_value()) {
      mapval = val;
      ESP_LOGV(TAG, "write_lambda returned mapping value %lld", *mapval);
    } else {
      ESP_LOGD(TAG, "Communication handled by write_lambda - exiting control");
      return;
    }
  }

  if (data.empty()) {
    modbus::helpers::number_to_payload(data, *mapval, this->sensor_value_type);
  } else {
    ESP_LOGV(TAG, "Using payload from write lambda");
  }

  if (data.empty()) {
    ESP_LOGW(TAG, "No payload was created for updating select");
    return;
  }

  // register_count declares the READ range width - it may pull neighboring registers into one poll -
  // so a write covers exactly the registers the value occupies: the quantity comes from the payload,
  // never from register_count (padding to it would zero registers the user only declared for reading).
  // A payload wider than the declared range means the config and the lambda disagree - drop it.
  if (data.size() > this->register_count) {
    ESP_LOGE(TAG, "Payload has %zu registers but register_count is %u; dropping write", data.size(),
             this->register_count);
    return;
  }

  const uint16_t write_address = this->write_address();
  this->write_command_.emplace(this->parent_->create_command());
  bool queued;
  if ((this->register_count == 1) && (!this->use_write_multiple_)) {
    queued = this->write_command_->write_single_register(write_address, data[0]);
  } else {
    queued = this->write_command_->write_multiple_registers(write_address, data);
  }

  // Only report the new option if the hub accepted the frame; a refusal leaves the entity unchanged.
  if (!queued) {
    ESP_LOGW(TAG, "Modbus write for '%s' was refused by the hub; state not published", this->get_name().c_str());
    return;
  }
  if (this->optimistic_)
    this->publish_state(index);
}

}  // namespace esphome::modbus_controller
