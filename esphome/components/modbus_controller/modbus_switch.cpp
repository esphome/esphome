
#include "modbus_switch.h"

namespace esphome {
namespace modbus_controller {

static const char *const TAG = "modbus_switch";

// ModbusSwitch
void ModbusSwitch::log() { LOG_SWITCH(TAG, get_name().c_str(), this); }

void ModbusSwitch::add_to_controller(ModbusController *master, ModbusFunctionCode register_type, uint16_t start_address,
                                     uint8_t offset, uint32_t bitmask) {
  /*
    Create a binary-sensor with a flag auto_switch . if true automatically create an assoociated switch object for
    this address and makes the sensor internal
    ... or maybe vice versa ?

  */
  this->register_type = register_type;
  if (register_type == ModbusFunctionCode::WRITE_SINGLE_REGISTER ||
      register_type == ModbusFunctionCode::WRITE_SINGLE_COIL) {
    start_address += offset;
    offset = 0;
  }
  this->start_address = start_address;
  this->offset = offset;
  this->bitmask = bitmask;
  this->sensor_value_type = SensorValueType::BIT;
  this->register_count = 1;
  this->skip_updates = 0;
  this->parent_ = master;
}

float ModbusSwitch::parse_and_publish(const std::vector<uint8_t> &data) {
  int64_t value = 0;
  float result = NAN;
  switch (this->register_type) {
    case ModbusFunctionCode::READ_DISCRETE_INPUTS:
    case ModbusFunctionCode::READ_COILS:
      // offset for coil is the actual number of the coil not the byte offset
      value = coil_from_vector(this->offset, data);
      break;
    default:
      value = get_data<uint16_t>(data, this->offset) & this->bitmask;
      break;
  }

  result = static_cast<float>(value);
  this->publish_state(value != 0.0);
  return result;
}

void ModbusSwitch::write_state(bool state) {
  // This will be called every time the user requests a state change.
  if (parent_ == nullptr) {
    // switch not configued correctly
    ESP_LOGE(TAG, "ModbusSwitch: %s : missing parent", this->get_name().c_str());
    return;
  }
  ModbusCommandItem cmd;
  ESP_LOGD(TAG, "write_state for ModbusSwitch '%s': new value = %d  type = %d address = %X offset = %x",
           this->get_name().c_str(), state, (int) this->register_type, this->start_address, this->offset);
  switch (this->register_type) {
    case ModbusFunctionCode::READ_COILS:
      // offset for coil and discrete inputs is the coil/register number not bytes
      cmd = ModbusCommandItem::create_write_single_coil(parent_, this->start_address + this->offset, state);
      break;
    case ModbusFunctionCode::READ_DISCRETE_INPUTS:
      cmd = ModbusCommandItem::create_write_single_command(parent_, this->start_address + this->offset, state);
      break;

    default:
      // since offset is in bytes and a register is 16 bits we get the start by adding offset/2
      cmd = ModbusCommandItem::create_write_single_command(parent_, this->start_address + this->offset / 2,
                                                           state ? 0xFFFF & this->bitmask : 0);
      break;
  }
  parent_->queue_command(cmd);
  publish_state(state);
}
// ModbusSwitch end
}  // namespace modbus_controller
}  // namespace esphome
