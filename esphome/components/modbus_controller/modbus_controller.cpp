#include "modbus_controller.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome {
namespace modbus_controller {

static const char *const TAG = "modbus_controller";

void ModbusController::setup() {
  // Modbus::setup();
  this->create_register_ranges_();
}

/*
 To work with the existing modbus class and avoid polling for responses a command queue is used.
 send_next_command will submit the command at the top of the queue and set the corresponding callback
 to handle the response from the device.
 Once the response has been processed it is removed from the queue and the next command is sent
*/
bool ModbusController::send_next_command_() {
  uint32_t last_send = millis() - this->last_command_timestamp_;

  if ((last_send > this->command_throttle_) && !waiting_for_response() && !command_queue_.empty()) {
    auto &command = command_queue_.front();

    ESP_LOGV(TAG, "Sending next modbus command to device %d register 0x%02X count %d", this->address_,
             command->register_address, command->register_count);
    command->send();
    this->last_command_timestamp_ = millis();
    // remove from queue if no handler is defined or command was sent too often
    if (!command->on_data_func || command->send_countdown < 1) {
      ESP_LOGD(TAG, "Modbus command to device=%d register=0x%02X countdown=%d removed from queue after send",
               this->address_, command->register_address, command->send_countdown);
      command_queue_.pop_front();
    }
  }
  return (!command_queue_.empty());
}

// Queue incoming response
void ModbusController::on_modbus_data(const std::vector<uint8_t> &data) {
  auto &current_command = this->command_queue_.front();
  if (current_command != nullptr) {
    // Move the commandItem to the response queue
    current_command->payload = data;
    this->incoming_queue_.push(std::move(current_command));
    ESP_LOGV(TAG, "Modbus response queued");
    command_queue_.pop_front();
  }
}

// Dispatch the response to the registered handler
void ModbusController::process_modbus_data_(const ModbusCommandItem *response) {
  ESP_LOGV(TAG, "Process modbus response for address 0x%X size: %zu", response->register_address,
           response->payload.size());
  response->on_data_func(response->register_type, response->register_address, response->payload);
}

void ModbusController::on_modbus_error(uint8_t function_code, uint8_t exception_code) {
  ESP_LOGE(TAG, "Modbus error function code: 0x%X exception: %d ", function_code, exception_code);
  // Remove pending command waiting for a response
  auto &current_command = this->command_queue_.front();
  if (current_command != nullptr) {
    ESP_LOGE(TAG,
             "Modbus error - last command: function code=0x%X  register adddress = 0x%X  "
             "registers count=%d "
             "payload size=%zu",
             function_code, current_command->register_address, current_command->register_count,
             current_command->payload.size());
    command_queue_.pop_front();
  }
}

std::map<uint64_t, SensorItem *>::iterator ModbusController::find_register_(ModbusRegisterType register_type,
                                                                            uint16_t start_address) {
  auto vec_it = find_if(begin(register_ranges_), end(register_ranges_), [=](RegisterRange const &r) {
    return (r.start_address == start_address && r.register_type == register_type);
  });

  if (vec_it == register_ranges_.end()) {
    ESP_LOGE(TAG, "No matching range for sensor found - start_address :  0x%X", start_address);
  } else {
    auto map_it = sensormap_.find(vec_it->first_sensorkey);
    if (map_it == sensormap_.end()) {
      ESP_LOGE(TAG, "No sensor found in at start_address :  0x%X (0x%llX)", start_address, vec_it->first_sensorkey);
    } else {
      return sensormap_.find(vec_it->first_sensorkey);
    }
  }
  // not found
  return std::end(sensormap_);
}
void ModbusController::on_register_data(ModbusRegisterType register_type, uint16_t start_address,
                                        const std::vector<uint8_t> &data) {
  ESP_LOGV(TAG, "data for register address : 0x%X : ", start_address);

  auto map_it = find_register_(register_type, start_address);
  // loop through all sensors with the same start address
  while (map_it != sensormap_.end() && map_it->second->start_address == start_address) {
    if (map_it->second->register_type == register_type) {
      map_it->second->parse_and_publish(data);
    }
    map_it++;
  }
}

void ModbusController::queue_command(const ModbusCommandItem &command) {
  // check if this commmand is already qeued.
  // not very effective but the queue is never really large
  for (auto &item : command_queue_) {
    if (item->register_address == command.register_address && item->register_count == command.register_count &&
        item->register_type == command.register_type) {
      ESP_LOGW(TAG, "Duplicate modbus command found");
      // update the payload of the queued command
      // replaces a previous command
      item->payload = command.payload;
      return;
    }
  }
  command_queue_.push_back(make_unique<ModbusCommandItem>(command));
}

void ModbusController::update_range_(RegisterRange &r) {
  ESP_LOGV(TAG, "Range : %X Size: %x (%d) skip: %d", r.start_address, r.register_count, (int) r.register_type,
           r.skip_updates_counter);
  if (r.skip_updates_counter == 0) {
    // if a custom command is used the user supplied custom_data is only available in the SensorItem.
    if (r.register_type == ModbusRegisterType::CUSTOM) {
      auto it = this->find_register_(r.register_type, r.start_address);
      if (it != sensormap_.end()) {
        auto command_item = ModbusCommandItem::create_custom_command(
            this, it->second->custom_data,
            [this](ModbusRegisterType register_type, uint16_t start_address, const std::vector<uint8_t> &data) {
              this->on_register_data(ModbusRegisterType::CUSTOM, start_address, data);
            });
        command_item.register_address = it->second->start_address;
        command_item.register_count = it->second->register_count;
        command_item.function_code = ModbusFunctionCode::CUSTOM;
        queue_command(command_item);
      }
    } else {
      queue_command(ModbusCommandItem::create_read_command(this, r.register_type, r.start_address, r.register_count));
    }
    r.skip_updates_counter = r.skip_updates;  // reset counter to config value
  } else {
    r.skip_updates_counter--;
  }
}
//
// Queue the modbus requests to be send.
// Once we get a response to the command it is removed from the queue and the next command is send
//
void ModbusController::update() {
  if (!command_queue_.empty()) {
    ESP_LOGV(TAG, "%zu modbus commands already in queue", command_queue_.size());
  } else {
    ESP_LOGV(TAG, "Updating modbus component");
  }

  for (auto &r : this->register_ranges_) {
    ESP_LOGVV(TAG, "Updating range 0x%X", r.start_address);
    update_range_(r);
  }
}

// walk through the sensors and determine the registerranges to read
size_t ModbusController::create_register_ranges_() {
  register_ranges_.clear();
  uint8_t n = 0;
  if (sensormap_.empty()) {
    return 0;
  }

  auto ix = sensormap_.begin();
  auto prev = ix;
  int total_register_count = 0;
  uint16_t current_start_address = ix->second->start_address;
  uint8_t buffer_offset = ix->second->offset;
  uint8_t skip_updates = ix->second->skip_updates;
  auto first_sensorkey = ix->second->getkey();
  total_register_count = 0;
  while (ix != sensormap_.end()) {
    ESP_LOGV(TAG, "Register: 0x%X %d %d  0x%llx (%d) buffer_offset = %d (0x%X) skip=%u", ix->second->start_address,
             ix->second->register_count, ix->second->offset, ix->second->getkey(), total_register_count, buffer_offset,
             buffer_offset, ix->second->skip_updates);
    // if this is a sequential address based on number of registers and address of previous sensor
    // convert to an offset to the previous sensor (address 0x101 becomes address 0x100 offset 2 bytes)
    if (!ix->second->force_new_range && total_register_count >= 0 &&
        prev->second->register_type == ix->second->register_type &&
        prev->second->start_address + total_register_count == ix->second->start_address &&
        prev->second->start_address < ix->second->start_address) {
      ix->second->start_address = prev->second->start_address;
      ix->second->offset += prev->second->offset + prev->second->get_register_size();

      // replace entry in sensormap_
      auto const value = ix->second;
      sensormap_.erase(ix);
      sensormap_.insert({value->getkey(), value});
      // move iterator back to new element
      ix = sensormap_.find(value->getkey());  // next(prev, 1);
    }
    if (current_start_address != ix->second->start_address ||
        //  ( prev->second->start_address + prev->second->offset != ix->second->start_address) ||
        ix->second->register_type != prev->second->register_type) {
      // Difference doesn't match so we have a gap
      if (n > 0) {
        RegisterRange r;
        r.start_address = current_start_address;
        r.register_count = total_register_count;
        if (prev->second->register_type == ModbusRegisterType::COIL ||
            prev->second->register_type == ModbusRegisterType::DISCRETE_INPUT) {
          r.register_count = prev->second->offset + 1;
        }
        r.register_type = prev->second->register_type;
        r.first_sensorkey = first_sensorkey;
        r.skip_updates = skip_updates;
        r.skip_updates_counter = 0;
        ESP_LOGV(TAG, "Add range 0x%X %d skip:%d", r.start_address, r.register_count, r.skip_updates);
        register_ranges_.push_back(r);
      }
      skip_updates = ix->second->skip_updates;
      current_start_address = ix->second->start_address;
      first_sensorkey = ix->second->getkey();
      total_register_count = ix->second->register_count;
      buffer_offset = ix->second->offset;
      n = 1;
    } else {
      n++;
      if (ix->second->offset != prev->second->offset || n == 1) {
        total_register_count += ix->second->register_count;
        buffer_offset += ix->second->get_register_size();
      }
      // use the lowest non zero value for the whole range
      // Because zero is the default value for skip_updates it is excluded from getting the min value.
      if (ix->second->skip_updates != 0) {
        if (skip_updates != 0) {
          skip_updates = std::min(skip_updates, ix->second->skip_updates);
        } else {
          skip_updates = ix->second->skip_updates;
        }
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
    if (prev->second->register_type == ModbusRegisterType::COIL ||
        prev->second->register_type == ModbusRegisterType::DISCRETE_INPUT) {
      r.register_count = prev->second->offset + 1;
    }
    r.register_type = prev->second->register_type;
    r.first_sensorkey = first_sensorkey;
    r.skip_updates = skip_updates;
    r.skip_updates_counter = 0;
    ESP_LOGV(TAG, "Add last range 0x%X %d skip:%d", r.start_address, r.register_count, r.skip_updates);
    register_ranges_.push_back(r);
  }
  return register_ranges_.size();
}

void ModbusController::dump_config() {
  ESP_LOGCONFIG(TAG, "ModbusController:");
  ESP_LOGCONFIG(TAG, "  Address: 0x%02X", this->address_);
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
  ESP_LOGCONFIG(TAG, "sensormap");
  for (auto &it : sensormap_) {
    ESP_LOGCONFIG("TAG", "  Sensor 0x%llX start=0x%X count=%d size=%d", it.second->getkey(), it.second->start_address,
                  it.second->register_count, it.second->get_register_size());
  }
#endif
}

void ModbusController::loop() {
  // Incoming data to process?
  if (!incoming_queue_.empty()) {
    auto &message = incoming_queue_.front();
    if (message != nullptr)
      process_modbus_data_(message.get());
    incoming_queue_.pop();

  } else {
    // all messages processed send pending commmands
    send_next_command_();
  }
}

void ModbusController::on_write_register_response(ModbusRegisterType register_type, uint16_t start_address,
                                                  const std::vector<uint8_t> &data) {
  ESP_LOGV(TAG, "Command ACK 0x%X %d ", get_data<uint16_t>(data, 0), get_data<int16_t>(data, 1));
}

void ModbusController::dump_sensormap_() {
  ESP_LOGV("modbuscontroller.h", "sensormap");
  for (auto &it : sensormap_) {
    ESP_LOGV("modbuscontroller.h", "  Sensor 0x%llX start=0x%X count=%d size=%d", it.second->getkey(),
             it.second->start_address, it.second->register_count, it.second->get_register_size());
  }
}

ModbusCommandItem ModbusCommandItem::create_read_command(
    ModbusController *modbusdevice, ModbusRegisterType register_type, uint16_t start_address, uint16_t register_count,
    std::function<void(ModbusRegisterType register_type, uint16_t start_address, const std::vector<uint8_t> &data)>
        &&handler) {
  ModbusCommandItem cmd;
  cmd.modbusdevice = modbusdevice;
  cmd.register_type = register_type;
  cmd.function_code = modbus_register_read_function(register_type);
  cmd.register_address = start_address;
  cmd.register_count = register_count;
  cmd.on_data_func = std::move(handler);
  return cmd;
}

ModbusCommandItem ModbusCommandItem::create_read_command(ModbusController *modbusdevice,
                                                         ModbusRegisterType register_type, uint16_t start_address,
                                                         uint16_t register_count) {
  ModbusCommandItem cmd;
  cmd.modbusdevice = modbusdevice;
  cmd.register_type = register_type;
  cmd.function_code = modbus_register_read_function(register_type);
  cmd.register_address = start_address;
  cmd.register_count = register_count;
  cmd.on_data_func = [modbusdevice](ModbusRegisterType register_type, uint16_t start_address,
                                    const std::vector<uint8_t> &data) {
    modbusdevice->on_register_data(register_type, start_address, data);
  };
  return cmd;
}

ModbusCommandItem ModbusCommandItem::create_write_multiple_command(ModbusController *modbusdevice,
                                                                   uint16_t start_address, uint16_t register_count,
                                                                   const std::vector<uint16_t> &values) {
  ModbusCommandItem cmd;
  cmd.modbusdevice = modbusdevice;
  cmd.register_type = ModbusRegisterType::HOLDING;
  cmd.function_code = ModbusFunctionCode::WRITE_MULTIPLE_REGISTERS;
  cmd.register_address = start_address;
  cmd.register_count = register_count;
  cmd.on_data_func = [modbusdevice, cmd](ModbusRegisterType register_type, uint16_t start_address,
                                         const std::vector<uint8_t> &data) {
    modbusdevice->on_write_register_response(cmd.register_type, start_address, data);
  };
  for (auto v : values) {
    cmd.payload.push_back((v / 256) & 0xFF);
    cmd.payload.push_back(v & 0xFF);
  }
  return cmd;
}

ModbusCommandItem ModbusCommandItem::create_write_single_coil(ModbusController *modbusdevice, uint16_t address,
                                                              bool value) {
  ModbusCommandItem cmd;
  cmd.modbusdevice = modbusdevice;
  cmd.register_type = ModbusRegisterType::COIL;
  cmd.function_code = ModbusFunctionCode::WRITE_SINGLE_COIL;
  cmd.register_address = address;
  cmd.register_count = 1;
  cmd.on_data_func = [modbusdevice, cmd](ModbusRegisterType register_type, uint16_t start_address,
                                         const std::vector<uint8_t> &data) {
    modbusdevice->on_write_register_response(cmd.register_type, start_address, data);
  };
  cmd.payload.push_back(value ? 0xFF : 0);
  cmd.payload.push_back(0);
  return cmd;
}

ModbusCommandItem ModbusCommandItem::create_write_multiple_coils(ModbusController *modbusdevice, uint16_t start_address,
                                                                 const std::vector<bool> &values) {
  ModbusCommandItem cmd;
  cmd.modbusdevice = modbusdevice;
  cmd.register_type = ModbusRegisterType::COIL;
  cmd.function_code = ModbusFunctionCode::WRITE_MULTIPLE_COILS;
  cmd.register_address = start_address;
  cmd.register_count = values.size();
  cmd.on_data_func = [modbusdevice, cmd](ModbusRegisterType register_type, uint16_t start_address,
                                         const std::vector<uint8_t> &data) {
    modbusdevice->on_write_register_response(cmd.register_type, start_address, data);
  };

  uint8_t bitmask = 0;
  int bitcounter = 0;
  for (auto coil : values) {
    if (coil) {
      bitmask |= (1 << bitcounter);
    }
    bitcounter++;
    if (bitcounter % 8 == 0) {
      cmd.payload.push_back(bitmask);
      bitmask = 0;
    }
  }
  // add remaining bits
  if (bitcounter % 8) {
    cmd.payload.push_back(bitmask);
  }
  return cmd;
}

ModbusCommandItem ModbusCommandItem::create_write_single_command(ModbusController *modbusdevice, uint16_t start_address,
                                                                 int16_t value) {
  ModbusCommandItem cmd;
  cmd.modbusdevice = modbusdevice;
  cmd.register_type = ModbusRegisterType::HOLDING;
  cmd.function_code = ModbusFunctionCode::WRITE_SINGLE_REGISTER;
  cmd.register_address = start_address;
  cmd.register_count = 1;  // not used here anyways
  cmd.on_data_func = [modbusdevice, cmd](ModbusRegisterType register_type, uint16_t start_address,
                                         const std::vector<uint8_t> &data) {
    modbusdevice->on_write_register_response(cmd.register_type, start_address, data);
  };
  cmd.payload.push_back((value / 256) & 0xFF);
  cmd.payload.push_back((value % 256) & 0xFF);
  return cmd;
}

ModbusCommandItem ModbusCommandItem::create_custom_command(
    ModbusController *modbusdevice, const std::vector<uint8_t> &values,
    std::function<void(ModbusRegisterType register_type, uint16_t start_address, const std::vector<uint8_t> &data)>
        &&handler) {
  ModbusCommandItem cmd;
  cmd.modbusdevice = modbusdevice;
  cmd.function_code = ModbusFunctionCode::CUSTOM;
  if (handler == nullptr) {
    cmd.on_data_func = [](ModbusRegisterType, uint16_t, const std::vector<uint8_t> &data) {
      ESP_LOGI(TAG, "Custom Command sent");
    };
  } else {
    cmd.on_data_func = handler;
  }
  cmd.payload = values;

  return cmd;
}

bool ModbusCommandItem::send() {
  if (this->function_code != ModbusFunctionCode::CUSTOM) {
    modbusdevice->send(uint8_t(this->function_code), this->register_address, this->register_count, this->payload.size(),
                       this->payload.empty() ? nullptr : &this->payload[0]);
  } else {
    modbusdevice->send_raw(this->payload);
  }
  ESP_LOGV(TAG, "Command sent %d 0x%X %d", uint8_t(this->function_code), this->register_address, this->register_count);
  send_countdown--;
  return true;
}

std::vector<uint16_t> float_to_payload(float value, SensorValueType value_type) {
  union {
    float float_value;
    uint32_t raw;
  } raw_to_float;

  std::vector<uint16_t> data;
  int32_t val;

  switch (value_type) {
    case SensorValueType::U_WORD:
    case SensorValueType::S_WORD:
      // cast truncates the float do some rounding here
      data.push_back(lroundf(value) & 0xFFFF);
      break;
    case SensorValueType::U_DWORD:
    case SensorValueType::S_DWORD:
      val = lroundf(value);
      data.push_back((val & 0xFFFF0000) >> 16);
      data.push_back(val & 0xFFFF);
      break;
    case SensorValueType::U_DWORD_R:
    case SensorValueType::S_DWORD_R:
      val = lroundf(value);
      data.push_back(val & 0xFFFF);
      data.push_back((val & 0xFFFF0000) >> 16);
      break;
    case SensorValueType::FP32:
      raw_to_float.float_value = value;
      data.push_back((raw_to_float.raw & 0xFFFF0000) >> 16);
      data.push_back(raw_to_float.raw & 0xFFFF);
      break;
    case SensorValueType::FP32_R:
      raw_to_float.float_value = value;
      data.push_back(raw_to_float.raw & 0xFFFF);
      data.push_back((raw_to_float.raw & 0xFFFF0000) >> 16);
      break;
    default:
      ESP_LOGE(TAG, "Invalid data type for modbus float to payload conversation");
      break;
  }
  return data;
}

float payload_to_float(const std::vector<uint8_t> &data, SensorValueType sensor_value_type, uint8_t offset,
                       uint32_t bitmask) {
  union {
    float float_value;
    uint32_t raw;
  } raw_to_float;

  int64_t value = 0;  // int64_t because it can hold signed and unsigned 32 bits
  float result = NAN;

  switch (sensor_value_type) {
    case SensorValueType::U_WORD:
      value = mask_and_shift_by_rightbit(get_data<uint16_t>(data, offset), bitmask);  // default is 0xFFFF ;
      result = static_cast<float>(value);
      break;
    case SensorValueType::U_DWORD:
      value = get_data<uint32_t>(data, offset);
      value = mask_and_shift_by_rightbit((uint32_t) value, bitmask);
      result = static_cast<float>(value);
      break;
    case SensorValueType::U_DWORD_R:
      value = get_data<uint32_t>(data, offset);
      value = static_cast<uint32_t>(value & 0xFFFF) << 16 | (value & 0xFFFF0000) >> 16;
      value = mask_and_shift_by_rightbit((uint32_t) value, bitmask);
      result = static_cast<float>(value);
      break;
    case SensorValueType::S_WORD:
      value = mask_and_shift_by_rightbit(get_data<int16_t>(data, offset),
                                         bitmask);  // default is 0xFFFF ;
      result = static_cast<float>(value);
      break;
    case SensorValueType::S_DWORD:
      value = mask_and_shift_by_rightbit(get_data<int32_t>(data, offset), bitmask);
      result = static_cast<float>(value);
      break;
    case SensorValueType::S_DWORD_R: {
      value = get_data<uint32_t>(data, offset);
      // Currently the high word is at the low position
      // the sign bit is therefore at low before the switch
      uint32_t sign_bit = (value & 0x8000) << 16;
      value = mask_and_shift_by_rightbit(
          static_cast<int32_t>(((value & 0x7FFF) << 16 | (value & 0xFFFF0000) >> 16) | sign_bit), bitmask);
      result = static_cast<float>(value);
    } break;
    case SensorValueType::U_QWORD:
      // Ignore bitmask for U_QWORD
      value = get_data<uint64_t>(data, offset);
      result = static_cast<float>(value);
      break;

    case SensorValueType::S_QWORD:
      // Ignore bitmask for S_QWORD
      value = get_data<int64_t>(data, offset);
      result = static_cast<float>(value);
      break;
    case SensorValueType::U_QWORD_R:
      // Ignore bitmask for U_QWORD
      value = get_data<uint64_t>(data, offset);
      value = static_cast<uint64_t>(value & 0xFFFF) << 48 | (value & 0xFFFF000000000000) >> 48 |
              static_cast<uint64_t>(value & 0xFFFF0000) << 32 | (value & 0x0000FFFF00000000) >> 32 |
              static_cast<uint64_t>(value & 0xFFFF00000000) << 16 | (value & 0x00000000FFFF0000) >> 16;
      result = static_cast<float>(value);
      break;

    case SensorValueType::S_QWORD_R:
      // Ignore bitmask for S_QWORD
      value = get_data<int64_t>(data, offset);
      result = static_cast<float>(value);
      break;
    case SensorValueType::FP32:
      raw_to_float.raw = get_data<uint32_t>(data, offset);
      ESP_LOGD(TAG, "FP32 = 0x%08X => %f", raw_to_float.raw, raw_to_float.float_value);
      result = raw_to_float.float_value;
      break;
    case SensorValueType::FP32_R: {
      auto tmp = get_data<uint32_t>(data, offset);
      raw_to_float.raw = static_cast<uint32_t>(tmp & 0xFFFF) << 16 | (tmp & 0xFFFF0000) >> 16;
      ESP_LOGD(TAG, "FP32_R = 0x%08X => %f", raw_to_float.raw, raw_to_float.float_value);
      result = raw_to_float.float_value;
    } break;
    case SensorValueType::RAW:
      result = NAN;
      break;
    default:
      break;
  }
  return result;
}

}  // namespace modbus_controller
}  // namespace esphome
