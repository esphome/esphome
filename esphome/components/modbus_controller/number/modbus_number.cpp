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
  this->clear_dispatched_();
  // A new write supersedes this entity's own not-yet-sent writes: drop them (and detach any in-flight one)
  // so a rapidly-changing value writes the latest, not every intermediate.
  this->clear_tx_queue_for_device();
  modbus::RegisterValues data;
  float write_value = value;
  if (this->write_transform_func_.has_value()) {
    // The lambda may drive the write itself via item->write_*(), override the value (return a value), or
    // (deprecated) fill `data` with the register words to write.
    auto val = (*this->write_transform_func_)(this, value, data);
    if (this->dispatched()) {
      this->publish_state(value);
      return;
    }
    if (!data.empty()) {
      // Deprecated buffer path (frozen): the lambda filled a legacy raw frame as words; pack it big-endian.
      this->warn_write_buffer_deprecated_(LOG_STR("number"), this->start_address);
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
      char hex_buf[format_hex_pretty_uint16_size(MODBUS_NUMBER_MAX_LOG_REGISTERS)];
#endif
      ESP_LOGV(TAG, "Modbus Number write raw: %s",
               format_hex_pretty_to(hex_buf, sizeof(hex_buf), data.data(), data.size()));
      // Sized to hold RegisterValues at capacity, so a full buffer can never truncate into a valid frame.
      StaticVector<uint8_t, modbus::MAX_NUM_OF_REGISTERS_TO_READ * 2> bytes;
      for (uint16_t word : data) {
        const auto word_bytes = decode_value(word);
        bytes.push_back(word_bytes[0]);
        bytes.push_back(word_bytes[1]);
      }
      if (!this->send_raw_frame_deprecated_(std::span<const uint8_t>(bytes.data(), bytes.size()))) {
        ESP_LOGW(TAG, "Modbus write for '%s' was refused by the hub; state not published", this->get_name().c_str());
        return;
      }
      this->publish_state(value);
      return;
    }
    if (!val.has_value()) {
      ESP_LOGV(TAG, "Communication handled by lambda - exiting control");
      return;
    }
    ESP_LOGV(TAG, "Value overwritten by lambda");
    write_value = val.value();
  } else {
    write_value = this->multiply_by_ * write_value;
  }

  modbus::helpers::float_to_payload(data, write_value, this->sensor_value_type);
  // float_to_payload() appends nothing for RAW, so an empty payload must be caught before data[0] below.
  if (data.empty()) {
    ESP_LOGW(TAG, "No payload was created for updating number");
    return;
  }

  ESP_LOGD(TAG,
           "Updating register: connected Sensor=%s start address=0x%X register count=%d new value=%.02f (val=%.02f)",
           this->get_name().c_str(), this->start_address, this->register_count, value, write_value);

  bool queued;
  if (this->register_count == 1 && !this->use_write_multiple_) {
    queued = this->write_single_register(this->write_address(), data[0]);
  } else {
    queued = this->write_multiple_registers(this->write_address(), data);
  }
  if (!queued) {
    ESP_LOGW(TAG, "Modbus write for '%s' was refused by the hub; state not published", this->get_name().c_str());
    return;
  }
  this->publish_state(value);
}
void ModbusNumber::dump_config() { LOG_NUMBER(TAG, "Modbus Number", this); }

}  // namespace esphome::modbus_controller
