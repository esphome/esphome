
#include "modbus_switch.h"
#include "esphome/core/log.h"
namespace esphome {
namespace modbus_controller {

static const char *const TAG = "modbus_controller.switch";

void ModbusSwitch::setup() {
  // value isn't required
  // without it we crash on save
  this->get_initial_state();
}
void ModbusSwitch::dump_config() { LOG_SWITCH(TAG, "Modbus Controller Switch", this); }

void ModbusSwitch::parse_and_publish(const std::vector<uint8_t> &data) {
  bool value = false;
  switch (this->register_type) {
    case ModbusRegisterType::DISCRETE_INPUT:
    case ModbusRegisterType::COIL:
      // offset for coil is the actual number of the coil not the byte offset
      value = coil_from_vector(this->offset, data);
      break;
    default:
      value = get_data<uint16_t>(data, this->offset) & this->bitmask;
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

  ESP_LOGV(TAG, "Publish '%s': new value = %s type = %d address = %X offset = %x", this->get_name().c_str(),
           ONOFF(value), (int) this->register_type, this->start_address, this->offset);
  this->publish_state(value);
}

void ModbusSwitch::write_state(bool state) {
  // This will be called every time the user requests a state change.
  ModbusCommandItem cmd;
  ESP_LOGV(TAG, "write_state '%s': new value = %s type = %d address = %X offset = %x", this->get_name().c_str(),
           ONOFF(state), (int) this->register_type, this->start_address, this->offset);
  switch (this->register_type) {
    case ModbusRegisterType::COIL:
      // offset for coil and discrete inputs is the coil/register number not bytes
      cmd = ModbusCommandItem::create_write_single_coil(parent_, this->start_address + this->offset, state);
      break;
    case ModbusRegisterType::DISCRETE_INPUT:
      cmd = ModbusCommandItem::create_write_single_command(parent_, this->start_address + this->offset, state);
      break;

    default:
      // since offset is in bytes and a register is 16 bits we get the start by adding offset/2
      cmd = ModbusCommandItem::create_write_single_command(parent_, this->start_address + this->offset / 2,
                                                           state ? 0xFFFF & this->bitmask : 0);
      break;
  }
  this->parent_->queue_command(cmd);
  publish_state(state);
}
// ModbusSwitch end
}  // namespace modbus_controller
}  // namespace esphome
