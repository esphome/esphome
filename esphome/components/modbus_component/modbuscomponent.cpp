#include "modbuscomponent.h"
#include "esphome/core/log.h"
#include <sstream>
#include <iomanip>

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
  uint32_t command_delay = millis() - this->last_command_timestamp_;

  // Left check of delay since last command in case theres ever a command sent by calling send_raw_command_ directly
  if ((command_delay > this->command_throttle_) && (!command_queue_.empty())) {
    auto &command = command_queue_.front();
    ESP_LOGD(TAG, "Sending next modbus command %u %u", command_delay, this->command_throttle_);
    this->sending_ = true;
    command->send();
    this->last_command_timestamp_ = millis();
    if (!command->on_data_func)  // No handler remove from queue directly after sending
      command_queue_.pop();
  } else {
    yield();
  }
  this->sending_ = false;
  return (!command_queue_.empty());
}

// Dispatch the response to the registered handler
void ModbusComponent::on_modbus_data(const std::vector<uint8_t> &data) {
  ESP_LOGD(TAG, "Modbus data %zu", data.size());
  this->sending_ = false;
  auto &current_command = this->command_queue_.front();
  if (current_command != nullptr) {
#ifdef CHECK_RESPONSE_SIZE
    // Because modbus has no header to indicate the response the expected response size is used to check if the correct
    // callback is place
    if (data.size() != current_command->expected_response_size) {
      ESP_LOGE(TAG, "Unexpected modbus response size. Expected=%d actual=%zu", current_command->expected_response_size,
               data.size());
    } else {
      ESP_LOGVV(TAG, "Dispatch to handler");
      current_command->on_data_func(current_command->register_address, data);
    }
#else
    ESP_LOGVV(TAG, "Dispatch to handler");
    current_command->on_data_func(current_command->register_address, data);
#endif
    command_queue_.pop();
  }


  // if (!command_queue_.empty()) {
  //   send_next_command_();
  // }
}
void ModbusComponent::on_modbus_error(uint8_t function_code, uint8_t exception_code) {
  ESP_LOGE(TAG, "Modbus error function code: 0x%X exception: %d ", function_code, exception_code);
  this->sending_ = false;
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
  // if (!command_queue_.empty()) {
  //  send_next_command_();
  //}
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
  for (auto &r : this->register_ranges) {
    ESP_LOGV(TAG, " Range : %X Size: %x (%d) skip: %d", r.start_address, r.register_count, (int) r.register_type,
             r.skip_updates_counter);
    if (r.skip_updates_counter == 0) {
      ModbusCommandItem command_item =
          ModbusCommandItem::create_read_command(this, r.register_type, r.start_address, r.register_count);
      queue_command_(command_item);
      r.skip_updates_counter = r.skip_updates;  // reset counter to config value
    } else {
      r.skip_updates_counter--;
    }
  }
  // send_next_command_();
  ESP_LOGI(TAG, "Modbus  update complete Free Heap  %u bytes",ESP.getFreeHeap());
}

// walk through the sensors and determine the registerranges to read
size_t ModbusComponent::create_register_ranges() {
  register_ranges.clear();
  uint8_t n = 0;
  // map is already sorted by keys so we start with the lowest address ;
  auto ix = sensormap.begin();
  auto prev = ix;
  uint16_t current_start_address = ix->second->start_address;
  uint8_t buffer_offset = ix->second->offset;
  uint8_t buffer_gap = 0;
  uint8_t skip_updates = 0;
  auto first_sensorkey = ix->second->getkey();
  int total_register_count = 0;
  while (ix != sensormap.end()) {
    size_t diff = ix->second->start_address - prev->second->start_address;
    skip_updates = std::max(skip_updates, prev->second->skip_updates);
    ESP_LOGV(TAG, "Register:: 0x%X %d %d  0x%llx (%d) buffer_offset = %d (0x%X)", ix->second->start_address,
             ix->second->register_count, ix->second->offset, ix->second->getkey(), total_register_count, buffer_offset,
             buffer_offset);
    if (current_start_address != ix->second->start_address ||
        //  ( prev->second->start_address + prev->second->offset != ix->second->start_address) ||
        ix->second->register_type != prev->second->register_type) {
      // Difference doesn't match so we have a gap
      if (n > 0) {
        RegisterRange r;
        r.start_address = current_start_address;
        r.register_count = total_register_count;
        r.register_type = prev->second->register_type;
        r.first_sensorkey = first_sensorkey;
        r.skip_updates = skip_updates;
        r.skip_updates_counter = 0;
        skip_updates = 0;
        ESP_LOGD(TAG, "Add range 0x%X %d skip:%d", r.start_address, r.register_count, r.skip_updates);
        register_ranges.push_back(r);
      }
      current_start_address = ix->second->start_address;
      first_sensorkey = ix->second->getkey();
      total_register_count = ix->second->register_count;
      buffer_offset = ix->second->offset;
      buffer_gap = ix->second->offset;
      n = 1;
    } else {
      n++;
      if (ix->second->offset != prev->second->offset || n == 1) {
        total_register_count += ix->second->register_count;
        buffer_offset += ix->second->get_register_size();
      }
    }
    prev = ix++;
  }
  // Add the last range
  if (n > 0) {
    RegisterRange r;
    r.start_address = current_start_address;
    //    r.register_count = prev->second->offset>>1 + prev->second->get_register_size();
    r.register_count = total_register_count;
    r.register_type = prev->second->register_type;
    r.first_sensorkey = first_sensorkey;
    r.skip_updates = skip_updates;
    r.skip_updates_counter = 0;
    register_ranges.push_back(r);
  }
  return register_ranges.size();
}

#ifdef OLD
// walk through the sensors and determine the registerranges to read
size_t ModbusComponent::create_register_ranges() {
  register_ranges.clear();
  uint8_t n = 0;
  // map is already sorted by keys so we start with the lowest address ;
  auto ix = sensormap.begin();
  auto prev = ix;
  uint16_t current_start_address = ix->second->start_address;
  uint8_t buffer_offset = ix->second->offset;
  uint8_t buffer_gap = 0;
  auto first_sensorkey = ix->second->getkey();
  int count = ix->second->register_count;
  while (ix != sensormap.end()) {
    size_t diff = ix->second->start_address - prev->second->start_address;
    if (ix->second->offset != buffer_offset) {
      ESP_LOGW(TAG, "GAP:  0x%X %d %d  0x%llx (%d) buffer_offset = %d (0x%X)", ix->second->start_address,
               ix->second->register_count, ix->second->offset, ix->second->getkey(), count, buffer_offset,
               buffer_offset);
    }
    ESP_LOGD(TAG, "Register:: 0x%X %d %d  0x%llx (%d) buffer_offset = %d (0x%X)", ix->second->start_address,
             ix->second->register_count, ix->second->offset, ix->second->getkey(), count, buffer_offset, buffer_offset);
    // if (diff > ix->second->get_register_size() || ix->second->register_type != prev->second->register_type) {
    // f (diff > ix->second->get_register_size() || ix->second->register_type != prev->second->register_type) {
    if (current_start_address != ix->second->start_address ||
        //  ( prev->second->start_address + prev->second->offset != ix->second->start_address) ||
        ix->second->register_type != prev->second->register_type) {
      // Difference doesn't match so we have a gap
      if (n > 0) {
        RegisterRange r;
        r.start_address = current_start_address;

        //        r.register_count = (prev->second->offset >> 1 )  + prev->second->get_register_size();
        r.register_count = count;
        //        r.register_count = (prev->second->offset >> 1 )  + prev->second->get_register_size();
        r.register_type = prev->second->register_type;
        r.first_sensorkey = first_sensorkey;
        ESP_LOGD(TAG, "Add range 0x%X %d %d %d", r.start_address, r.register_count, count, diff);
        register_ranges.push_back(r);
      }
      current_start_address = ix->second->start_address;
      first_sensorkey = ix->second->getkey();
      count = ix->second->register_count;
      buffer_offset = ix->second->offset;
      buffer_gap = ix->second->offset;
      n = 1;
    } else {
      n++;
      if (ix->second->offset != prev->second->offset || n == 1) {
        count += ix->second->register_count;
        buffer_offset += ix->second->get_register_size();
      }
    }
    prev = ix++;
  }
  // Add the last range
  if (n > 0) {
    RegisterRange r;
    r.start_address = current_start_address;
    //    r.register_count = prev->second->offset>>1 + prev->second->get_register_size();
    r.register_count = count;
    r.register_type = prev->second->register_type;
    r.first_sensorkey = first_sensorkey;
    register_ranges.push_back(r);
  }
  return register_ranges.size();
}
#endif

void ModbusComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "EPSOLAR:");
  ESP_LOGCONFIG(TAG, "  Address: 0x%02X", this->address_);
  for (auto &item : this->sensormap) {
    item.second->log();
  }
  create_register_ranges();
}

void ModbusComponent::loop() { send_next_command_(); }

#ifndef USE_OLD
// Extract data from modbus response buffer
template<typename T> T get_data(const std::vector<uint8_t> &data, size_t offset) {
  if (sizeof(T) == sizeof(uint8_t)) {
    return T(data[offset]);
  }
  if (sizeof(T) == sizeof(uint16_t)) {
    return T((uint16_t(data[offset + 0]) << 8) | (uint16_t(data[offset + 1]) << 0));
  }

  if (sizeof(T) == sizeof(uint32_t)) {
    return get_data<uint16_t>(data, offset) << 16 | get_data<uint16_t>(data, (offset + 2));
  }

  if (sizeof(T) == sizeof(uint64_t)) {
    return static_cast<uint64_t>(get_data<uint32_t>(data, offset)) << 32 |
           (static_cast<uint64_t>(get_data<uint32_t>(data, offset + 4)));
  }
}
#else
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
    return T((uint16_t(data[offset + 0]) << 8) | (uint16_t(data[offset + 1]) << 16) |
             ((uint16_t(data[offset + 2]) << 8) | (uint16_t(data[offset + 3]) << 0)));
  }
  if (sizeof(T) == sizeof(uint64_t)) {
    uint64_t result = 0;
    result = (((uint16_t(data[offset + 0]) << 8) | uint16_t(data[offset + 1]) << 16) |
              ((uint16_t(data[offset + 2]) << 8) | uint16_t(data[offset + 3])));

    result <<= 32;
    result |= (((uint16_t(data[offset + 4]) << 8) | uint16_t(data[offset + 5]) << 16) |
               ((uint16_t(data[offset + 6]) << 8) | uint16_t(data[offset + 7])));

    return result;
  }
}
#endif

void ModbusComponent::on_write_register_response(uint16_t start_address, const std::vector<uint8_t> &data) {
  ESP_LOGD(TAG, "Command ACK 0x%X %d ", get_data<uint16_t>(data, 0), get_data<int16_t>(data, 1));
}

void FloatSensorItem::log() { LOG_SENSOR("", sensor_->get_name().c_str(), this->sensor_); }

void BinarySensorItem::log() { LOG_BINARY_SENSOR("", sensor_->get_name().c_str(), this->sensor_); }

void TextSensorItem::log() { LOG_TEXT_SENSOR("", sensor_->get_name().c_str(), this->sensor_); }

// Extract bits from value and shift right according to the bitmask
// if the bitmask is 0x00F0  we want the values frrom bit 5 - 8.
// the result is then shifted right by the postion if the first right set bit in the mask
// Usefull for modbus data where more than one value is packed in a 16 bit register
// Example: on Epever the "Length of night" register 0x9065 encodes values of the whole night length of time as
// D15 - D8 =  hour, D7 - D0 = minute
// To get the hours use mask 0xFF00 and  0x00FF for the minute
template<typename N> N mask_and_shift_by_rightbit(N data, uint32_t mask) {
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

union dataresults {
  byte raw[16];
  uint8_t u_byte;
  uint16_t u_word;
  uint32_t u_dword;
  uint64_t u_qword;
  int8_t s_byte;
  int16_t s_word;
  int32_t s_dword;
  int64_t s_qword;
};

float FloatSensorItem::parse_and_publish(const std::vector<uint8_t> &data) {
  int64_t value = 0;  // int64_t because it can hold signed and unsigned 32 bits
  float result = NAN;

  switch (sensor_value_type) {
    case SensorValueType::U_WORD:
      value = mask_and_shift_by_rightbit(get_data<uint16_t>(data, this->offset), this->bitmask);  // default is 0xFFFF ;
      break;
    case SensorValueType::U_DWORD:
      value = get_data<uint32_t>(data, this->offset);
      value = mask_and_shift_by_rightbit((uint32_t) value, this->bitmask);
      break;
    case SensorValueType::U_DWORD_R:
      value = get_data<uint32_t>(data, this->offset);
      value = static_cast<uint32_t>(value & 0xFFFF) << 16 | (value & 0xFFFF0000) >> 16;
      value = mask_and_shift_by_rightbit((uint32_t) value, this->bitmask);
      break;
    case SensorValueType::S_WORD:
      value = mask_and_shift_by_rightbit(get_data<int16_t>(data, this->offset),
                                         this->bitmask);  // default is 0xFFFF ;
      break;
    case SensorValueType::S_DWORD:
      value = mask_and_shift_by_rightbit(get_data<int32_t>(data, this->offset), this->bitmask);
      break;
    case SensorValueType::S_DWORD_R: {
      value = get_data<uint32_t>(data, this->offset);
      // Currently the high word is at the low position
      // the sign bit is therefore at low before the switch
      uint32_t sign_bit = (value & 0x8000) << 16;
      value = mask_and_shift_by_rightbit(
          static_cast<int32_t>(((value & 0x7FFF) << 16 | (value & 0xFFFF0000) >> 16) | sign_bit), this->bitmask);
    } break;
    case SensorValueType::U_QWORD:
      // Ignore bitmask for U_QWORD
      value = get_data<uint64_t>(data, this->offset);
      break;

    case SensorValueType::S_QWORD:
      // Ignore bitmask for S_QWORD
      value = get_data<int64_t>(data, this->offset);
      break;
    case SensorValueType::U_QWORD_R:
      // Ignore bitmask for U_QWORD
      value = get_data<uint64_t>(data, this->offset);
      value = static_cast<uint64_t>(value & 0xFFFF) << 48 | (value & 0xFFFF000000000000) >> 48 |
              static_cast<uint64_t>(value & 0xFFFF0000) << 32 | (value & 0x0000FFFF00000000) >> 32 |
              static_cast<uint64_t>(value & 0xFFFF00000000) << 16 | (value & 0x00000000FFFF0000) >> 16;
      break;

    case SensorValueType::S_QWORD_R:
      // Ignore bitmask for S_QWORD
      value = get_data<int64_t>(data, this->offset);
      break;
    default:
      break;
  }

  if (transform_expression != nullptr) {
    result = transform_expression(value);
  }  else {
    result = float(value);
  }
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

float TextSensorItem::parse_and_publish(const std::vector<uint8_t> &data) {
  int64_t value = 0;
  float result = this->response_bytes_;
  std::ostringstream output;
  uint8_t max_items = this->response_bytes_;
  for (uint16_t b : data) {
    if (this->hex_encode) {
      output << std::setfill('0') << std::setw(2) << std::hex << b;
    } else {
      output << (char) b;
      if (--max_items == 0) {
        break;
      }
    }
    this->sensor_->publish_state(output.str());
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
  ESP_LOGV(TAG, "Command sent %d 0x%X %d", uint8_t(this->function_code), this->register_address, this->register_count);
  modbusdevice->send(uint8_t(this->function_code), this->register_address, this->register_count,
                     this->payload.empty() ? nullptr : &this->payload[0]);
  return true;
}

}  // namespace modbus_component
}  // namespace esphome
