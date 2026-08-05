#include "modbus_output.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <array>

namespace esphome::modbus_controller {

static const char *const TAG = "modbus_controller.output";

// Maximum bytes to log in verbose hex output
static constexpr size_t MODBUS_OUTPUT_MAX_LOG_BYTES = 64;

/** Write a value to the device
 *
 */
void ModbusFloatOutput::write_state(float value) {
  // The output is its own hub device, so it writes with this->write_*() directly and the write_lambda's
  // `item` pointer IS this command.
  this->clear_dispatched_();
  // A new write supersedes this entity's own not-yet-sent writes: drop them (and detach any in-flight one)
  // so a rapidly-changing value writes the latest, not every intermediate.
  this->clear_tx_queue_for_device();
  ModbusWriteRegisters data;
  auto original_value = value;
  // Is there are lambda configured?
  if (this->write_transform_func_.has_value()) {
    // The lambda may drive the write itself via item->write_*(), override the value (return a value), or
    // (deprecated) fill `data` with the register words to write. `data` is passed by reference.
    auto val = (*this->write_transform_func_)(this, value, data);
    if (val.has_value()) {
      ESP_LOGV(TAG, "Value overwritten by lambda");
      value = val.value();
    } else if (!this->dispatched() && data.empty()) {
      ESP_LOGV(TAG, "Communication handled by lambda - exiting control");
      return;
    }
  } else {
    value = this->multiply_by_ * value;
  }

  if (this->dispatched()) {
    // The lambda already sent a frame via item->write_*(); nothing more to do.
    return;
  }

  if (!data.empty()) {
    // Deprecated buffer path (frozen): the lambda supplied the register words to write.
    this->warn_write_buffer_deprecated_("Modbus float output");
  } else {
    modbus::helpers::float_to_payload(data, value, this->sensor_value_type);
  }

  ESP_LOGD(TAG, "Updating register: start address=0x%X register count=%d new value=%.02f (val=%.02f)",
           this->start_address, this->register_count, value, original_value);

  // The command declares register_count registers, so the payload must be exactly that many words;
  // anything else would put a byte count on the wire that disagrees with the quantity field.
  // number_to_payload() appends nothing for RAW, so an empty payload must be caught before data[0].
  if (data.empty()) {
    ESP_LOGW(TAG, "No payload was created for updating output");
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

  if (this->register_count == 1 && !this->use_write_multiple_) {
    this->write_single_register(this->write_address(), data[0]);
  } else {
    this->write_multiple_registers(this->write_address(), data);
  }
}

void ModbusFloatOutput::dump_config() {
  ESP_LOGCONFIG(TAG, "Modbus Float Output:");
  LOG_FLOAT_OUTPUT(this);
  ESP_LOGCONFIG(TAG,
                "  Device start address: 0x%X\n"
                "  Register count: %d\n"
                "  Value type: %d",
                this->start_address, this->register_count, static_cast<int>(this->sensor_value_type));
}

// ModbusBinaryOutput
void ModbusBinaryOutput::write_state(bool state) {
  // This will be called every time the user requests a state change. The output is its own hub device, so it
  // writes with this->write_*/send_pdu() directly and the write_lambda's `item` pointer IS this command.
  this->clear_dispatched_();
  // A new write supersedes this entity's own not-yet-sent writes: drop them (and detach any in-flight one)
  // so a rapidly-changing value writes the latest, not every intermediate.
  this->clear_tx_queue_for_device();
  ModbusWriteBytes data;

  // Is there are lambda configured?
  if (this->write_transform_func_.has_value()) {
    // The lambda may drive the write itself via item->write_*/send_pdu(), override the value (return a value),
    // or (deprecated) fill `data` with a custom PDU. `data` is passed by reference.
    auto val = (*this->write_transform_func_)(this, state, data);
    if (val.has_value()) {
      ESP_LOGV(TAG, "Value overwritten by lambda");
      state = val.value();
    } else if (!this->dispatched() && data.empty()) {
      ESP_LOGV(TAG, "Communication handled by lambda - exiting control");
      return;
    }
  }

  if (this->dispatched()) {
    // The lambda already sent a frame via item->write_*/send_pdu(); nothing more to do.
  } else if (!data.empty()) {
    // Deprecated buffer path (frozen): the lambda filled a custom PDU (function code + data); the hub adds
    // the device address and CRC.
    this->warn_write_buffer_deprecated_("Modbus binary output");
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
    char hex_buf[format_hex_pretty_size(MODBUS_OUTPUT_MAX_LOG_BYTES)];
#endif
    ESP_LOGV(TAG, "Modbus binary output write raw: %s",
             format_hex_pretty_to(hex_buf, sizeof(hex_buf), data.data(), data.size()));
    // The lambda filled a legacy raw frame (device address + function code + data); the hub adds the CRC.
    this->send_raw_frame_deprecated(data);
  } else {
    ESP_LOGV(TAG, "Write new state: value is %s, type is %d address = %X, offset = %x", ONOFF(state),
             (int) this->register_type, this->start_address, this->offset);
    // offset for coil and discrete inputs is the coil/register number not bytes
    if (this->use_write_multiple_) {
      std::array<bool, 1> states{state};
      this->write_multiple_coils(this->write_address(), states);
    } else {
      this->write_single_coil(this->write_address(), state);
    }
  }
}

void ModbusBinaryOutput::dump_config() {
  ESP_LOGCONFIG(TAG, "Modbus Binary Output:");
  LOG_BINARY_OUTPUT(this);
  ESP_LOGCONFIG(TAG,
                "  Device start address: 0x%X\n"
                "  Register count: %d\n"
                "  Value type: %d",
                this->start_address, this->register_count, static_cast<int>(this->sensor_value_type));
}

}  // namespace esphome::modbus_controller
