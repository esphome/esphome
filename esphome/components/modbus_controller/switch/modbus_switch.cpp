
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
  this->clear_dispatched_();
  // A new write supersedes this entity's own not-yet-sent writes: drop them (and detach any in-flight one)
  // so a rapidly-changing value writes the latest, not every intermediate.
  this->clear_tx_queue_for_device();
  modbus::helpers::PduBuffer data;
  bool write_value = state;
  if (this->write_transform_func_.has_value()) {
    // The lambda may drive the write itself via item->write_*/queue_pdu(), override the written value (return a
    // value), or (deprecated) fill `data` with a custom PDU.
    auto val = (*this->write_transform_func_)(this, state, data);
    if (this->dispatched()) {
      this->publish_state(state);
      return;
    }
    if (!data.empty()) {
      this->warn_write_buffer_deprecated_(LOG_STR("switch"), this->start_address);
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
      char hex_buf[format_hex_pretty_size(MODBUS_SWITCH_MAX_LOG_BYTES)];
#endif
      ESP_LOGV(TAG, "Modbus Switch write raw: %s",
               format_hex_pretty_to(hex_buf, sizeof(hex_buf), data.data(), data.size()));
      // The lambda filled a legacy raw frame (device address + function code + data).
      if (!this->send_raw_frame_deprecated_(data)) {
        ESP_LOGW(TAG, "Modbus write for '%s' was refused by the hub; state not published", this->get_name().c_str());
        return;
      }
      this->publish_state(state);
      return;
    }
    if (!val.has_value()) {
      ESP_LOGV(TAG, "Communication handled by lambda - exiting control");
      return;
    }
    // The returned bool is the wire value only; the entity still reports the requested state. A polled
    // entity needs the read lambda inverted to match, or the next poll flips the display back.
    ESP_LOGV(TAG, "Value overwritten by lambda");
    write_value = val.value();
  }
  ESP_LOGV(TAG, "write_state '%s': new value = %s (wire = %s) type = %d address = %X offset = %x",
           this->get_name().c_str(), ONOFF(state), ONOFF(write_value), (int) this->register_type, this->start_address,
           this->offset);
  bool queued;
  if (this->register_type == EntityType::COIL) {
    // offset for coil and discrete inputs is the coil/register number not bytes
    if (this->use_write_multiple_) {
      std::array<bool, 1> states{write_value};
      queued = this->write_multiple_coils(this->write_address(), states);
    } else {
      queued = this->write_single_coil(this->write_address(), write_value);
    }
  } else {
    if (this->use_write_multiple_) {
      std::array<uint16_t, 1> states{static_cast<uint16_t>(write_value ? (0xFFFF & this->bitmask) : 0)};
      queued = this->write_multiple_registers(this->write_address(), states);
    } else {
      queued = this->write_single_register(this->write_address(), write_value ? 0xFFFF & this->bitmask : 0u);
    }
  }
  if (!queued) {
    ESP_LOGW(TAG, "Modbus write for '%s' was refused by the hub; state not published", this->get_name().c_str());
    return;
  }
  this->publish_state(state);
}
// ModbusSwitch end
}  // namespace esphome::modbus_controller
