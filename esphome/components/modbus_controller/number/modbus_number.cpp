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
  // The number is its own hub device, so it writes with this->write_*() directly and the write_lambda's
  // `item` pointer IS this command.
  this->clear_dispatched_();
  // A new write supersedes this entity's own not-yet-sent writes: drop them (and detach any in-flight one)
  // so a rapidly-changing value writes the latest, not every intermediate.
  this->clear_tx_queue_for_device();
  ModbusWriteRegisters data;
  float write_value = value;
  // Is there are lambda configured?
  if (this->write_transform_func_.has_value()) {
    // The lambda may drive the write itself via item->write_*(), override the value (return a value), or
    // (deprecated) fill `data` with the register words to write. `data` is passed by reference.
    auto val = (*this->write_transform_func_)(this, value, data);
    if (val.has_value()) {
      ESP_LOGV(TAG, "Value overwritten by lambda");
      write_value = val.value();
    } else if (!this->dispatched() && data.empty()) {
      ESP_LOGV(TAG, "Communication handled by lambda - exiting control");
      return;
    }
  } else {
    write_value = this->multiply_by_ * write_value;
  }

  if (this->dispatched()) {
    // The lambda already sent a frame via item->write_*(); nothing more to do but publish.
    this->publish_state(value);
    return;
  }

  if (!data.empty()) {
    // Deprecated buffer path (frozen): the lambda filled a legacy raw frame as words (device address +
    // function code + data); pack them big-endian and send. The hub adds the CRC.
    this->warn_write_buffer_deprecated_(this->get_name().c_str());
    StaticVector<uint8_t, modbus::MAX_RAW_SIZE> bytes;
    for (auto v : data) {
      bytes.push_back((v >> 8) & 0xFF);
      bytes.push_back(v & 0xFF);
    }
    this->send_raw_frame_deprecated(std::span<const uint8_t>(bytes.data(), bytes.size()));
    this->publish_state(value);
    return;
  }

  modbus::helpers::float_to_payload(data, write_value, this->sensor_value_type);

  ESP_LOGD(TAG,
           "Updating register: connected Sensor=%s start address=0x%X register count=%d new value=%.02f (val=%.02f)",
           this->get_name().c_str(), this->start_address, this->register_width(), value, write_value);

  if (this->register_width() == 1 && !this->use_write_multiple_) {
    // since offset is in bytes and a register is 16 bits we get the start by adding offset/2
    this->write_single_register(this->write_address(), data[0]);
  } else {
    this->write_multiple_registers(this->write_address(), data);
  }
  this->publish_state(value);
}
void ModbusNumber::dump_config() { LOG_NUMBER(TAG, "Modbus Number", this); }

}  // namespace esphome::modbus_controller
