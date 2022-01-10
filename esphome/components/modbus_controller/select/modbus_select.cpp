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

  auto map_it = std::find(this->mapping.cbegin(), this->mapping.cend(), value);

  if (map_it != this->mapping.cend()) {
    size_t idx = std::distance(this->mapping.cbegin(), map_it);
    std::string option = this->traits.get_options()[idx];
    this->publish_state(option);
  } else {
    ESP_LOGE(TAG, "No option found for mapping %llu", value);
  }
}

void ModbusSelect::control(const std::string &value) {
  auto options = this->traits.get_options();
  auto opt_it = std::find(options.cbegin(), options.cend(), value);
  if (opt_it != options.cend()) {
    size_t idx = std::distance(options.cbegin(), opt_it);
    uint64_t mapping = this->mapping[idx];

    ESP_LOGV(TAG, "Set selection to %llu", mapping);

    ModbusCommandItem write_cmd;
    if ((this->register_count == 1) && (!this->use_write_multiple_)) {
      write_cmd =
          ModbusCommandItem::create_write_single_command(parent_, this->write_address, static_cast<uint16_t>(mapping));
    } else {
      std::vector<uint16_t> data;
      auto it = data.begin();
      for (uint8_t i = 0; i < this->get_register_size(); i++) {
        it = data.insert(it, static_cast<uint16_t>(mapping & 0xFFFF));
        mapping >>= 16;
      }
      write_cmd =
          ModbusCommandItem::create_write_multiple_command(parent_, this->write_address, this->register_count, data);
    }
    parent_->queue_command(write_cmd);
  } else {
    ESP_LOGE(TAG, "No mapping found for option %s", value.c_str());
  }
}

}  // namespace modbus_controller
}  // namespace esphome
