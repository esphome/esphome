
#include "modbus_switch.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <array>

namespace esphome::modbus_controller {

static const char *const TAG = "modbus_controller.switch";

// Maximum bytes to log in verbose hex output
static constexpr size_t MODBUS_SWITCH_MAX_LOG_BYTES = 64;

void ModbusSwitch::setup() {
  optional<bool> initial_state = Switch::get_initial_state_with_restore_mode();
  if (initial_state.has_value()) {
    // if it has a value, restore_mode is not "DISABLED", therefore act on the switch:
    if (initial_state.value()) {
      this->turn_on();
    } else {
      this->turn_off();
    }
  }
}
void ModbusSwitch::dump_config() { LOG_SWITCH(TAG, "Modbus Controller Switch", this); }

void ModbusSwitch::set_assumed_state(bool assumed_state) { this->assumed_state_ = assumed_state; }

bool ModbusSwitch::assumed_state() { return this->assumed_state_; }

void ModbusSwitch::parse_and_publish(std::span<const uint8_t> data) {
  bool value = false;
  // For coils/discrete inputs this is the bit index; for registers it is the byte offset.
  const size_t offset = this->offset;
  switch (this->register_type) {
    case modbus::EntityType::DISCRETE_INPUT:
    case modbus::EntityType::COIL:
      value = modbus::helpers::bit_from_packed(offset, data);
      break;
    default:
      value = modbus::helpers::get_data<uint16_t>(data.data(), offset) & this->bitmask;
      break;
  }

  // Is there a lambda registered
  // call it with the pre converted value and the raw data array
  if (this->publish_transform_func_) {
    // the lambda can parse the response itself
    auto val = (*this->publish_transform_func_)(this, value, data);
    if (val.has_value()) {
      ESP_LOGV(TAG, "Value overwritten by lambda");
      value = val.value();
    }
  }

  ESP_LOGV(TAG, "Publish '%s': new value = %s type = %d address = %X offset = %zx", this->get_name().c_str(),
           ONOFF(value), (int) this->register_type, this->start_address, offset);
  this->publish_state(value);
}

void ModbusSwitch::write_state(bool state) {
  // This will be called every time the user requests a state change. The switch is its own hub device, so it
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
      // Lambda handled (or declined) communication itself without using the entity or the buffer.
      ESP_LOGV(TAG, "Communication handled by lambda - exiting control");
      return;
    }
  }

  if (this->dispatched()) {
    // The lambda already sent a frame via item->write_*/send_pdu(); nothing more to do.
  } else if (!data.empty()) {
    // Deprecated buffer path (frozen): the lambda filled a custom PDU (function code + data); the hub adds
    // the device address and CRC.
    this->warn_write_buffer_deprecated_(this->get_name().c_str());
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
    char hex_buf[format_hex_pretty_size(MODBUS_SWITCH_MAX_LOG_BYTES)];
#endif
    ESP_LOGV(TAG, "Modbus Switch write raw: %s",
             format_hex_pretty_to(hex_buf, sizeof(hex_buf), data.data(), data.size()));
    // The lambda filled a legacy raw frame (device address + function code + data); the hub adds the CRC.
    this->send_raw_frame_deprecated(data);
  } else {
    ESP_LOGV(TAG, "write_state '%s': new value = %s type = %d address = %X offset = %x", this->get_name().c_str(),
             ONOFF(state), (int) this->register_type, this->start_address, this->offset);
    if (this->register_type == EntityType::COIL) {
      // offset for coil and discrete inputs is the coil/register number not bytes
      if (this->use_write_multiple_) {
        std::array<bool, 1> states{state};
        this->write_multiple_coils(this->write_address(), states);
      } else {
        this->write_single_coil(this->write_address(), state);
      }
    } else {
      if (this->use_write_multiple_) {
        std::array<uint16_t, 1> states{static_cast<uint16_t>(state ? (0xFFFF & this->bitmask) : 0)};
        this->write_multiple_registers(this->write_address(), states);
      } else {
        this->write_single_register(this->write_address(), state ? 0xFFFF & this->bitmask : 0u);
      }
    }
  }
  this->publish_state(state);
}
// ModbusSwitch end
}  // namespace esphome::modbus_controller
