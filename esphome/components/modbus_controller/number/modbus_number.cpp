#include <vector>
#include "modbus_number.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::modbus_controller {

static const char *const TAG = "modbus.number";

// Maximum uint16_t registers to log in verbose hex output
static constexpr size_t MODBUS_NUMBER_MAX_LOG_REGISTERS = 32;

void ModbusNumber::parse_and_publish(std::span<const uint8_t> data) {
  float result = payload_to_float(data, *this, this->offset) / this->multiply_by_;

  // Is there a lambda registered
  // call it with the pre converted value and the raw data array
  if (this->transform_func_.has_value()) {
    // the lambda can parse the response itself
    auto val = (*this->transform_func_)(this, result, data);
    if (val.has_value()) {
      ESP_LOGV(TAG, "Value overwritten by lambda");
      result = val.value();
    }
  }
  ESP_LOGD(TAG, "Number new state : %.02f", result);
  // this->sensor_->raw_state = result;
  this->publish_state(result);
}

void ModbusNumber::control(float value) {
  std::vector<uint16_t> data;
  float write_value = value;
  // Is there are lambda configured?
  if (this->write_transform_func_.has_value()) {
    // data is passed by reference
    // the lambda can fill the empty vector directly
    // in that case the return value is ignored
    auto val = (*this->write_transform_func_)(this, value, data);
    if (val.has_value()) {
      ESP_LOGV(TAG, "Value overwritten by lambda");
      write_value = val.value();
    } else {
      ESP_LOGV(TAG, "Communication handled by lambda - exiting control");
      return;
    }
  } else {
    write_value = this->multiply_by_ * write_value;
  }

  bool queued = false;
  if (!data.empty()) {
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
    char hex_buf[format_hex_pretty_uint16_size(MODBUS_NUMBER_MAX_LOG_REGISTERS)];
#endif
    ESP_LOGV(TAG, "Modbus Number write raw: %s",
             format_hex_pretty_to(hex_buf, sizeof(hex_buf), data.data(), data.size()));
    // The lambda filled a legacy raw frame as words (address + function code + data); pack big-endian and
    // send; the hub adds the CRC.
    if (data.size() * 2 > modbus::MAX_RAW_SIZE) {
      // Past the buffer's capacity push_back would silently drop bytes and the truncated frame would
      // get a valid CRC; refuse instead.
      ESP_LOGE(TAG, "Raw frame of %zu words exceeds the frame limit, not sent", data.size());
      return;
    }
    StaticVector<uint8_t, modbus::MAX_RAW_SIZE> bytes;
    for (auto v : data) {
      bytes.push_back((v >> 8) & 0xFF);
      bytes.push_back(v & 0xFF);
    }
    this->write_command_.emplace(this->parent_->create_command());
    queued = this->write_command_->send_raw_frame_deprecated(std::span<const uint8_t>(bytes.data(), bytes.size()));
  } else {
    std::vector<uint16_t> payload;
    modbus::helpers::float_to_payload(payload, write_value, this->sensor_value_type);
    // float_to_payload() appends nothing for RAW, so an empty payload must be caught before payload[0]
    // below (the same guard select and output already carry).
    if (payload.empty()) {
      ESP_LOGW(TAG, "No payload was created for updating number");
      return;
    }

    ESP_LOGD(TAG,
             "Updating register: connected Sensor=%s start address=0x%X register count=%d new value=%.02f (val=%.02f)",
             this->get_name().c_str(), this->start_address, this->register_count, value, write_value);

    // Create and send the write command
    this->write_command_.emplace(this->parent_->create_command());
    if (this->register_count == 1 && !this->use_write_multiple_) {
      queued = this->write_command_->write_single_register(this->write_address(), payload[0]);
    } else {
      queued = this->write_command_->write_multiple_registers(this->write_address(), payload);
    }
  }
  // Only report the new value if the hub accepted the frame; a refusal leaves the entity unchanged.
  if (!queued) {
    ESP_LOGW(TAG, "Modbus write for '%s' was refused by the hub; state not published", this->get_name().c_str());
    return;
  }
  this->publish_state(value);
}
void ModbusNumber::dump_config() { LOG_NUMBER(TAG, "Modbus Number", this); }

}  // namespace esphome::modbus_controller
