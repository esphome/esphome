#include "modbus_output.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::modbus_controller {

static const char *const TAG = "modbus_controller.output";

// Maximum bytes to log in verbose hex output
static constexpr size_t MODBUS_OUTPUT_MAX_LOG_BYTES = 64;

/** Write a value to the device
 *
 */
void ModbusFloatOutput::write_state(float value) {
  std::vector<uint16_t> data;
  auto original_value = value;
  // Is there are lambda configured?
  if (this->write_transform_func_.has_value()) {
    // data is passed by reference
    // the lambda can fill the empty vector directly
    // in that case the return value is ignored
    auto val = (*this->write_transform_func_)(this, value, data);
    if (val.has_value()) {
      ESP_LOGV(TAG, "Value overwritten by lambda");
      value = val.value();
    } else {
      ESP_LOGV(TAG, "Communication handled by lambda - exiting control");
      return;
    }
  } else {
    value = this->multiply_by_ * value;
  }
  // lambda didn't set payload
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

  // Create and send the write command
  optional<ModbusCommandItem> write_cmd;
  if (this->register_count == 1 && !this->use_write_multiple_) {
    write_cmd.emplace(
        ModbusCommandItem::create_write_single_command(this->parent_, this->start_address + this->offset, data[0]));
  } else {
    write_cmd.emplace(ModbusCommandItem::create_write_multiple_command(
        this->parent_, this->start_address + this->offset, data.size(), data));
  }
  this->parent_->queue_command(std::move(*write_cmd));
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
  // This will be called every time the user requests a state change.
  optional<ModbusCommandItem> cmd;
  std::vector<uint8_t> data;

  // Is there are lambda configured?
  if (this->write_transform_func_.has_value()) {
    // data is passed by reference
    // the lambda can fill the empty vector directly
    // in that case the return value is ignored
    auto val = (*this->write_transform_func_)(this, state, data);
    if (val.has_value()) {
      ESP_LOGV(TAG, "Value overwritten by lambda");
      state = val.value();
    } else {
      ESP_LOGV(TAG, "Communication handled by lambda - exiting control");
      return;
    }
  }
  if (!data.empty()) {
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
    char hex_buf[format_hex_pretty_size(MODBUS_OUTPUT_MAX_LOG_BYTES)];
#endif
    ESP_LOGV(TAG, "Modbus binary output write raw: %s",
             format_hex_pretty_to(hex_buf, sizeof(hex_buf), data.data(), data.size()));
    cmd.emplace(ModbusCommandItem::create_custom_command(
        this->parent_, data,
        [this](modbus::EntityType register_type, uint16_t start_address, std::span<const uint8_t> data) {
          this->parent_->on_write_register_response(register_type, this->start_address, data);
        }));
  } else {
    ESP_LOGV(TAG, "Write new state: value is %s, type is %d address = %X, offset = %x", ONOFF(state),
             (int) this->register_type, this->start_address, this->offset);

    // offset for coil and discrete inputs is the coil/register number not bytes
    if (this->use_write_multiple_) {
      std::vector<bool> states{state};
      cmd.emplace(
          ModbusCommandItem::create_write_multiple_coils(this->parent_, this->start_address + this->offset, states));
    } else {
      cmd.emplace(
          ModbusCommandItem::create_write_single_coil(this->parent_, this->start_address + this->offset, state));
    }
  }
  this->parent_->queue_command(std::move(*cmd));
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
