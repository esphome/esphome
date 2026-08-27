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
  this->clear_dispatched_();
  // A new write supersedes this entity's own not-yet-sent writes: drop them (and detach any in-flight one)
  // so a rapidly-changing value writes the latest, not every intermediate.
  this->clear_tx_queue_for_device();
  modbus::RegisterValues data;
  auto original_value = value;
  if (this->write_transform_func_.has_value()) {
    // The lambda may drive the write itself via item->write_*(), override the value (return a value), or
    // (deprecated) fill `data` with the register words to write.
    auto val = (*this->write_transform_func_)(this, value, data);
    if (this->dispatched()) {
      return;
    }
    if (!data.empty()) {
      // Deprecated buffer path (frozen): the lambda supplied the register words for the shared write below.
      this->warn_write_buffer_deprecated_(LOG_STR("float output"), this->start_address);
    } else if (!val.has_value()) {
      ESP_LOGV(TAG, "Communication handled by lambda - exiting control");
      return;
    } else {
      ESP_LOGV(TAG, "Value overwritten by lambda");
      value = val.value();
    }
  } else {
    value = this->multiply_by_ * value;
  }

  if (data.empty()) {
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

  bool queued;
  if (this->register_count == 1 && !this->use_write_multiple_) {
    queued = this->write_single_register(this->write_address(), data[0]);
  } else {
    queued = this->write_multiple_registers(this->write_address(), data);
  }
  if (!queued) {
    ESP_LOGW(TAG, "Modbus output write (address 0x%X) was refused by the hub", this->write_address());
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
  this->clear_dispatched_();
  // A new write supersedes this entity's own not-yet-sent writes: drop them (and detach any in-flight one)
  // so a rapidly-changing value writes the latest, not every intermediate.
  this->clear_tx_queue_for_device();
  modbus::helpers::PduBuffer data;

  if (this->write_transform_func_.has_value()) {
    // The lambda may drive the write itself via item->write_*/queue_pdu(), override the value (return a value),
    // or (deprecated) fill `data` with a custom PDU.
    auto val = (*this->write_transform_func_)(this, state, data);
    if (this->dispatched()) {
      return;
    }
    if (!data.empty()) {
      this->warn_write_buffer_deprecated_(LOG_STR("binary output"), this->start_address);
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
      char hex_buf[format_hex_pretty_size(MODBUS_OUTPUT_MAX_LOG_BYTES)];
#endif
      ESP_LOGV(TAG, "Modbus binary output write raw: %s",
               format_hex_pretty_to(hex_buf, sizeof(hex_buf), data.data(), data.size()));
      // The lambda filled a legacy raw frame (device address + function code + data).
      if (!this->send_raw_frame_deprecated_(data)) {
        ESP_LOGW(TAG, "Modbus output write (address 0x%X) was refused by the hub", this->write_address());
      }
      return;
    }
    if (!val.has_value()) {
      ESP_LOGV(TAG, "Communication handled by lambda - exiting control");
      return;
    }
    ESP_LOGV(TAG, "Value overwritten by lambda");
    state = val.value();
  }
  ESP_LOGV(TAG, "Write new state: value is %s, type is %d address = %X, offset = %x", ONOFF(state),
           (int) this->register_type, this->start_address, this->offset);
  // offset for coil and discrete inputs is the coil/register number not bytes
  bool queued;
  if (this->use_write_multiple_) {
    std::array<bool, 1> states{state};
    queued = this->write_multiple_coils(this->write_address(), states);
  } else {
    queued = this->write_single_coil(this->write_address(), state);
  }
  if (!queued) {
    ESP_LOGW(TAG, "Modbus output write (address 0x%X) was refused by the hub", this->write_address());
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
