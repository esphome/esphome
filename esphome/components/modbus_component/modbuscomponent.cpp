#include "modbuscomponent.h"
#include "esphome/core/log.h"

namespace esphome {
namespace modbus_component {

static const char *TAG = "ModbusComponent";

void ModbusComponent::setup() { this->create_register_ranges(); }

/*
  To work with the existing modbus class and avoid polling for responses a command queue is used.
  send_next_command will submit the command at the top of the queue and set the corresponding callback
  to handle the response from the device.
  Once the response has been processed it is removed from the queue and the next command is sent

*/

bool ModbusComponent::send_next_command_() {
  if (!command_queue_.empty()) {
    auto &command = command_queue_.front();
    delay(500);  //
    delay(500);  // will fail lint
    delay(500);  //
    command->send();
    if (!command->on_data_func)  // No handler remove from queue directly after sending
      command_queue_.pop();
  }
  return (!command_queue_.empty());
}

// Dispatch the response to the registered handler
void ModbusComponent::on_modbus_data(const std::vector<uint8_t> &data) {
  ESP_LOGD(TAG, "Modbus data %zu", data.size());
  auto &current_command = this->command_queue_.front();
  if (current_command != nullptr) {
    // Because modbus has no header to indicate the response the expected response size is used to check if the correct
    // callback is place
    if (data.size() != current_command->expected_response_size) {
      ESP_LOGE(TAG, "Unexpected modbus response size. Expected=%d actual=%zu", current_command->expected_response_size,
               data.size());
    } else {
      ESP_LOGVV(TAG, "Dispatch to handler");
      current_command->on_data_func(current_command->register_address, data);
    }
    command_queue_.pop();
  }

  if (!command_queue_.empty()) {
    send_next_command_();
  }
}
void ModbusComponent::on_modbus_error(uint8_t function_code, uint8_t exception_code) {
  ESP_LOGE(TAG, "Modbus error function code: 0x%X exception: %d ", function_code, exception_code);
  // Remove pending command waiting for a response
  auto &current_command = this->command_queue_.front();
  if (current_command != nullptr) {
    ESP_LOGW(TAG,
             "last command: expected response size=%d function code=0x%X  register adddress = 0x%X  registers count=%d "
             "payload size=%zu",
             current_command->expected_response_size, function_code, current_command->register_address,
             current_command->register_count, current_command->payload.size());
    command_queue_.pop();
  }
  // pump the next command in the queue
  if (!command_queue_.empty()) {
    send_next_command_();
  }
}

void ModbusComponent::on_register_data(uint16_t start_address, const std::vector<uint8_t> &data) {
  ESP_LOGD(TAG, " data for register address : 0x%X : ", start_address);

  auto vec_it = find_if(begin(register_ranges), end(register_ranges),
                        [=](RegisterRange const &r) { return (r.start_address == start_address); });

  if (vec_it == register_ranges.end()) {
    ESP_LOGE(TAG, "Handle incoming data : No matching range for sensor found - start_address :  0x%X", start_address);
    return;
  }

  auto map_it = sensormap.find(vec_it->first_sensorkey);
  if (map_it == sensormap.end()) {
    ESP_LOGE(TAG, "Handle incoming data : No sensor found in at start_address :  0x%X", start_address);
    return;
  }
  // loop through all sensors with the same start address
  while (map_it != sensormap.end() && map_it->second->start_address == start_address) {
    float val = map_it->second->parse_and_publish(data);
    ESP_LOGD(TAG, " Sensor : %s = %.02f ", map_it->second->get_name().c_str(), val);
    map_it++;
  }

#if TESTREMOVE
  // Because the values are now cached the only benefit of removing is avoiding modbus traffic

  // The register range 3000 and 300E only contain device config settings - remove them once they have been published
  // TODO Should be a config setting rather than hardcoded here
  if (start_address == 0x3000) {
    remove_register_range(0x3000);
  }
  if (start_address == 0x300E) {
    remove_register_range(0x300E);
  }
#endif
}

//
// Queue the modbus requests to be send.
// Once we get a response to the command it is removed from the queue and the next command is send
//
void ModbusComponent::update() {
  static int UPDATE_CNT;

  if (UPDATE_CNT++ == 0) {  // Not sure what happens here. Can't connect via Wifi without skipping the first update.
                            // Maybe interfering with dump_config ?
    ESP_LOGD(TAG, "Skipping update");
    return;
  }

  for (auto r : this->register_ranges) {
    ESP_LOGV(TAG, " Range : %X Size: %x (%d)", r.start_address, r.register_count, (int) r.register_type);
    ModbusCommandItem command_item =
        ModbusCommandItem::create_read_command(this, r.register_type, r.start_address, r.register_count);
    queue_command_(command_item);
  }
  send_next_command_();
  ESP_LOGD(TAG, "Modbus  update complete");
}

// walk through the sensors and determine the registerranges to read
size_t ModbusComponent::create_register_ranges() {
  register_ranges.clear();
  uint8_t n = 0;
  // map is already sorted by keys so we start with the lowest address ;
  auto ix = sensormap.begin();
  auto prev = ix;
  uint16_t current_start_address = ix->second->start_address;
  uint32_t first_sensorkey = ix->second->getkey();

  while (ix != sensormap.end()) {
    size_t diff = ix->second->start_address - prev->second->start_address;

    if (diff > ix->second->get_register_size() || ix->second->register_type != prev->second->register_type) {
      // Difference doesn't match so we have a gap
      if (n > 0) {
        RegisterRange r;
        r.start_address = current_start_address;
        r.register_count = prev->second->offset + prev->second->get_register_size();
        r.register_type = prev->second->register_type;
        r.first_sensorkey = first_sensorkey;
        register_ranges.push_back(r);
      }

      current_start_address = ix->second->start_address;
      first_sensorkey = ix->second->getkey();
      n = 1;
    } else {
      n++;
    }
    prev = ix++;
  }
  // Add the last range
  if (n > 0) {
    RegisterRange r;
    r.start_address = current_start_address;
    r.register_count = prev->second->offset + prev->second->get_register_size();
    r.register_type = prev->second->register_type;
    r.first_sensorkey = first_sensorkey;
    register_ranges.push_back(r);
  }
  return register_ranges.size();
}

void ModbusComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "EPSOLAR:");
  ESP_LOGCONFIG(TAG, "  Address: 0x%02X", this->address_);

  for (auto &item : this->sensormap) {
    item.second->log();
  }
}

// Extract data from modbus response buffer
template<typename T> T get_data(const std::vector<uint8_t> &data, size_t offset) {
  offset <<= 1;
  if (sizeof(T) == sizeof(uint8_t)) {
    return T(data[offset]);
  }
  if (sizeof(T) == sizeof(uint16_t)) {
    return T((uint16_t(data[offset + 0]) << 8) | (uint16_t(data[offset + 1]) << 0));
  }
  if (sizeof(T) == sizeof(uint32_t)) {
    return T((uint16_t(data[offset + 0]) << 8) | (uint16_t(data[offset + 1]) << 0) | (uint16_t(data[offset + 2]) << 8) |
             (uint16_t(data[offset + 3]) << 0) << 16);
  }
}

void ModbusComponent::on_write_register_response(uint16_t start_address, const std::vector<uint8_t> &data) {
  ESP_LOGD(TAG, "Command ACK 0x%X %d ", get_data<uint16_t>(data, 0), get_data<int16_t>(data, 1));
}

void FloatSensorItem::log() { LOG_SENSOR("", sensor_->get_name().c_str(), this->sensor_); }

void BinarySensorItem::log() { LOG_BINARY_SENSOR("", sensor_->get_name().c_str(), this->sensor_); }

// Extract bits from value and shift right according to the bitmask
// if the bitmask is 0x00F0  we want the values frrom bit 5 - 8.
// the result is then shifted right by the postion if the first right set bit in the mask
// Usefull for modbus data where more than one value is packed in a 16 bit register
// Example: on Epever the "Length of night" register 0x9065 encodes values of the whole night length of time as
// D15 - D8 =  hour, D7 - D0 = minute
// To get the hours use mask 0xFF00 and  0x00FF for the minute
template<typename N> N mask_and_shift_by_rightbit(N data, N mask) {
  auto result = (mask & data);
  if (result == 0) {
    return result;
  }
  for (int pos = 0; pos < sizeof(N) << 3; pos++) {
    if ((mask & (1 << pos)) != 0)
      return result >> pos;
  }
  return 0;
}

float FloatSensorItem::parse_and_publish(const std::vector<uint8_t> &data) {
  int64_t value = 0;  // int64_t because it can hold signed and unsigned 32 bits
  float result = NAN;

  switch (sensor_value_type) {
    case SensorValueType::U_SINGLE:
      value = mask_and_shift_by_rightbit(get_data<uint16_t>(data, this->offset), this->bitmask);  // default is 0xFFFF ;
      break;
    case SensorValueType::U_DOUBLE:
      value = get_data<uint32_t>(data, this->offset);  // Ignore bitmask for double register values.
      break;                                           // define 2 Singlebit regs instead
    case SensorValueType::S_SINGLE:
      value = mask_and_shift_by_rightbit(get_data<int16_t>(data, this->offset),
                                         (int16_t) this->bitmask);  // default is 0xFFFF ;
      break;
    case SensorValueType::S_DOUBLE:
      value = get_data<int32_t>(data, this->offset);
      break;

    default:
      break;
  }
  result = float(value);
  if (transform_expression != nullptr)
    result = transform_expression(value);
  this->sensor_->state = result;
  // No need to publish if the value didn't change since the last publish
  // can reduce mqtt traffic considerably if many sensors are used
  ESP_LOGVV(TAG, " SENSOR : new: %lld  old: %lld ", value, this->last_value);
  if (value != this->last_value) {
    this->sensor_->publish_state(result);
    this->last_value = value;
  }
  return result;
}

float BinarySensorItem::parse_and_publish(const std::vector<uint8_t> &data) {
  int64_t value = 0;
  float result = NAN;
  if (this->register_type == ModbusFunctionCode::READ_DISCRETE_INPUTS) {
    value = data[this->offset] & 1;  // Discret Input is always just one bit
  } else {
    value = get_data<uint16_t>(data, this->offset) & this->bitmask;
  }
  // MUST be a binarySensor
  // return from here since the remaining code deals with float results

  result = float(value);
  // No need tp publish if the value didn't change since the last publish
  if (value != this->last_value) {
    this->sensor_->publish_state(value != 0.0);
    this->last_value = value;
  }
  return result;
}

ModbusCommandItem ModbusCommandItem::create_write_multiple_command(ModbusComponent *modbusdevice,
                                                                   uint16_t start_address, uint16_t register_count,
                                                                   const std::vector<uint16_t> &values) {
  ModbusCommandItem cmd;
  cmd.modbusdevice = modbusdevice;
  cmd.function_code = ModbusFunctionCode::WRITE_MULTIPLE_REGISTERS;
  cmd.register_address = start_address;
  cmd.expected_response_size = 4;       // Response to write commands is always 4 bytes
  cmd.register_count = register_count;  // not used here anyways
  cmd.on_data_func = [modbusdevice](uint16_t start_address, const std::vector<uint8_t> data) {
    modbusdevice->on_write_register_response(start_address, data);
  };
  cmd.payload = values;
  return cmd;
}

ModbusCommandItem ModbusCommandItem::create_write_single_command(ModbusComponent *modbusdevice, uint16_t start_address,
                                                                 int16_t value) {
  ModbusCommandItem cmd;
  cmd.modbusdevice = modbusdevice;
  cmd.function_code = ModbusFunctionCode::WRITE_SINGLE_REGISTER;
  cmd.register_address = start_address;
  cmd.expected_response_size = 4;  // Response to write commands is always 4 bytes
  cmd.register_count = 1;          // not used here anyways
  cmd.on_data_func = [modbusdevice](uint16_t start_address, const std::vector<uint8_t> data) {
    modbusdevice->on_write_register_response(start_address, data);
  };
  cmd.payload.push_back(value);
  return cmd;
}

bool ModbusCommandItem::send() {
  modbusdevice->send(uint8_t(this->function_code), this->register_address, this->register_count,
                     this->payload.empty() ? nullptr : &this->payload[0]);
  return true;
}

}  // namespace modbus_component
}  // namespace esphome
