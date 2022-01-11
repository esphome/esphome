#include "modbus_select.h"
#include "esphome/core/log.h"

namespace esphome {
namespace modbus_controller {

static const char *const TAG = "modbus_controller.sensor";

void ModbusSelect::dump_config() { LOG_SELECT(TAG, "Modbus Controller Select", this); }

void ModbusSelect::parse_and_publish(const std::vector<uint8_t> &data) {
  uint64_t value = 0;
  auto it = data.cbegin() + this->offset;
  size_t count = this->get_register_size();
  while ((it != data.cend()) && (count > 0)) {
    value <<= 8;
    value |= *it;
    --count;
    ++it;
  }

  ESP_LOGV(TAG, "New select value %llu", value);

  optional<std::string> new_state;

  if (this->transform_func_.has_value()) {
    auto val = (*this->transform_func_)(this, value, data);
    if (val.has_value()) {
      new_state = val.value();
    }
  }

  if (!new_state.has_value()) {
    auto map_it = std::find(this->mapping_.cbegin(), this->mapping_.cend(), value);

    if (map_it != this->mapping_.cend()) {
      size_t idx = std::distance(this->mapping_.cbegin(), map_it);
      new_state = this->traits.get_options()[idx];
    } else {
      ESP_LOGE(TAG, "No option found for mapping %llu", value);
    }
  }

  if (new_state.has_value()) {
    this->publish_state(new_state.value());
  }
}

void ModbusSelect::control(const std::string &value) {
  optional<uint64_t> mapval;
  std::vector<uint16_t> data;

  if (this->write_transform_func_.has_value()) {
    auto val = (*this->write_transform_func_)(this, value, data);
    if (val.has_value()) {
      mapval = val.value();
    }
  }

  if (data.empty()) {
    if (!mapval.has_value()) {
      auto options = this->traits.get_options();
      auto opt_it = std::find(options.cbegin(), options.cend(), value);
      size_t idx = std::distance(options.cbegin(), opt_it);
      mapval = this->mapping_[idx];
    }

    auto it = data.begin();
    uint64_t remaining = mapval.value();
    ESP_LOGD(TAG, "Create payload for %llu", remaining);
    for (uint8_t i = 0; i < this->register_count; i++) {
      it = data.insert(it, static_cast<uint16_t>(remaining & 0xFFFF));
      remaining >>= 16;
    }
  }

  ModbusCommandItem write_cmd;
  if ((this->register_count == 1) && (!this->use_write_multiple_)) {
    write_cmd = ModbusCommandItem::create_write_single_command(parent_, this->write_address_, data[0]);
  } else {
    write_cmd =
        ModbusCommandItem::create_write_multiple_command(parent_, this->write_address_, this->register_count, data);
  }

  parent_->queue_command(write_cmd);
}

}  // namespace modbus_controller
}  // namespace esphome
