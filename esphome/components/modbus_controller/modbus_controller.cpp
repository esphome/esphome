#include "modbus_controller.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome {
namespace modbus_controller {

static const char *const TAG = "modbus_controller";

void ModbusController::setup() { this->create_register_ranges_(); }

/*
 To work with the existing modbus class and avoid polling for responses a command queue is used.
 send_next_command will submit the command at the top of the queue and set the corresponding callback
 to handle the response from the device.
 Once the response has been processed it is removed from the queue and the next command is sent
*/
bool ModbusController::send_next_command_() {
  uint32_t last_send = millis() - this->last_command_timestamp_;

  if ((last_send > this->command_throttle_) && !waiting_for_response() && !this->command_queue_.empty()) {
    auto &command = this->command_queue_.front();

    // remove from queue if command was sent too often
    if (!command->should_retry(this->max_cmd_retries_)) {
      if (!this->module_offline_) {
        ESP_LOGW(TAG, "Modbus device=%d set offline", this->address_);

        if (this->offline_skip_updates_ > 0) {
          // Update skip_updates_counter to stop flooding channel with timeouts
          for (auto &r : this->register_ranges_) {
            r.skip_updates_counter = this->offline_skip_updates_;
          }
        }
      }
      this->module_offline_ = true;
      ESP_LOGD(TAG, "Modbus command to device=%d register=0x%02X no response received - removed from send queue",
               this->address_, command->register_address);
      this->command_queue_.pop_front();
    } else {
      ESP_LOGV(TAG, "Sending next modbus command to device %d register 0x%02X count %d", this->address_,
               command->register_address, command->register_count);
      command->send();

      this->last_command_timestamp_ = millis();

      this->command_sent_callback_.call((int) command->function_code, command->register_address);

      // remove from queue if no handler is defined
      if (!command->on_data_func) {
        this->command_queue_.pop_front();
      }
    }
  }
  return (!this->command_queue_.empty());
}

// Queue incoming response
void ModbusController::on_modbus_data(const std::vector<uint8_t> &data) {
  auto &current_command = this->command_queue_.front();
  if (current_command != nullptr) {
    if (this->module_offline_) {
      ESP_LOGW(TAG, "Modbus device=%d back online", this->address_);

      if (this->offline_skip_updates_ > 0) {
        // Restore skip_updates_counter to restore commands updates
        for (auto &r : this->register_ranges_) {
          r.skip_updates_counter = 0;
        }
      }
    }
    this->module_offline_ = false;

    // Move the commandItem to the response queue
    current_command->payload = data;
    this->incoming_queue_.push(std::move(current_command));
    ESP_LOGV(TAG, "Modbus response queued");
    this->command_queue_.pop_front();
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
             "Modbus error - last command: function code=0x%X  register address = 0x%X  "
             "registers count=%d "
             "payload size=%zu",
             function_code, current_command->register_address, current_command->register_count,
             current_command->payload.size());
    this->command_queue_.pop_front();
  }
}

void ModbusController::on_modbus_read_registers(uint8_t function_code, uint16_t start_address,
                                                uint16_t number_of_registers) {
  ESP_LOGD(TAG,
           "Received read holding/input registers for device 0x%X. FC: 0x%X. Start address: 0x%X. Number of registers: "
           "0x%X.",
           this->address_, function_code, start_address, number_of_registers);

  std::vector<uint16_t> sixteen_bit_response;
  for (uint16_t current_address = start_address; current_address < start_address + number_of_registers;) {
    bool found = false;
    for (auto *server_register : this->server_registers_) {
      if (server_register->address == current_address) {
        float value = server_register->read_lambda();

        ESP_LOGD(TAG, "Matched register. Address: 0x%02X. Value type: %zu. Register count: %u. Value: %0.1f.",
                 server_register->address, static_cast<uint8_t>(server_register->value_type),
                 server_register->register_count, value);
        std::vector<uint16_t> payload = float_to_payload(value, server_register->value_type);
        sixteen_bit_response.insert(sixteen_bit_response.end(), payload.cbegin(), payload.cend());
        current_address += server_register->register_count;
        found = true;
        break;
      }
    }

    if (!found) {
      ESP_LOGW(TAG, "Could not match any register to address %02X. Sending exception response.", current_address);
      std::vector<uint8_t> error_response;
      error_response.push_back(this->address_);
      error_response.push_back(0x81);
      error_response.push_back(0x02);
      this->send_raw(error_response);
      return;
    }
  }

  std::vector<uint8_t> response;
  for (auto v : sixteen_bit_response) {
    auto decoded_value = decode_value(v);
    response.push_back(decoded_value[0]);
    response.push_back(decoded_value[1]);
  }

  this->send(function_code, start_address, number_of_registers, response.size(), response.data());
}

SensorSet ModbusController::find_sensors_(ModbusRegisterType register_type, uint16_t start_address) const {
  auto reg_it = find_if(begin(register_ranges_), end(register_ranges_), [=](RegisterRange const &r) {
    return (r.start_address == start_address && r.register_type == register_type);
  });

  if (reg_it == register_ranges_.end()) {
    ESP_LOGE(TAG, "No matching range for sensor found - start_address : 0x%X", start_address);
  } else {
    return reg_it->sensors;
  }

  // not found
  return {};
}
void ModbusController::on_register_data(ModbusRegisterType register_type, uint16_t start_address,
                                        const std::vector<uint8_t> &data) {
  ESP_LOGV(TAG, "data for register address : 0x%X : ", start_address);

  // loop through all sensors with the same start address
  auto sensors = find_sensors_(register_type, start_address);
  for (auto *sensor : sensors) {
    sensor->parse_and_publish(data);
  }
}

void ModbusController::queue_command(const ModbusCommandItem &command) {
  if (!this->allow_duplicate_commands_) {
    // check if this command is already qeued.
    // not very effective but the queue is never really large
    for (auto &item : this->command_queue_) {
      if (item->is_equal(command)) {
        ESP_LOGW(TAG, "Duplicate modbus command found: type=0x%x address=%u count=%u",
                 static_cast<uint8_t>(command.register_type), command.register_address, command.register_count);
        // update the payload of the queued command
        // replaces a previous command
        item->payload = command.payload;
        return;
      }
    }
  }
  this->command_queue_.push_back(make_unique<ModbusCommandItem>(command));
}

void ModbusController::update_range_(RegisterRange &r) {
  ESP_LOGV(TAG, "Range : %X Size: %x (%d) skip: %d", r.start_address, r.register_count, (int) r.register_type,
           r.skip_updates_counter);
  if (r.skip_updates_counter == 0) {
    // if a custom command is used the user supplied custom_data is only available in the SensorItem.
    if (r.register_type == ModbusRegisterType::CUSTOM) {
      auto sensors = this->find_sensors_(r.register_type, r.start_address);
      if (!sensors.empty()) {
        auto sensor = sensors.cbegin();
        auto command_item = ModbusCommandItem::create_custom_command(
            this, (*sensor)->custom_data,
            [this](ModbusRegisterType register_type, uint16_t start_address, const std::vector<uint8_t> &data) {
              this->on_register_data(ModbusRegisterType::CUSTOM, start_address, data);
            });
        command_item.register_address = (*sensor)->start_address;
        command_item.register_count = (*sensor)->register_count;
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
  if (!this->command_queue_.empty()) {
    ESP_LOGV(TAG, "%zu modbus commands already in queue", this->command_queue_.size());
  } else {
    ESP_LOGV(TAG, "Updating modbus component");
  }

  for (auto &r : this->register_ranges_) {
    ESP_LOGVV(TAG, "Updating range 0x%X", r.start_address);
    update_range_(r);
  }
}

// walk through the sensors and determine the register ranges to read
size_t ModbusController::create_register_ranges_() {
  register_ranges_.clear();
  if (this->parent_->role == modbus::ModbusRole::CLIENT && sensorset_.empty()) {
    ESP_LOGW(TAG, "No sensors registered");
    return 0;
  }

  // iterator is sorted see SensorItemsComparator for details
  auto ix = sensorset_.begin();
  RegisterRange r = {};
  uint8_t buffer_offset = 0;
  SensorItem *prev = nullptr;
  while (ix != sensorset_.end()) {
    SensorItem *curr = *ix;

    ESP_LOGV(TAG, "Register: 0x%X %d %d %d offset=%u skip=%u addr=%p", curr->start_address, curr->register_count,
             curr->offset, curr->get_register_size(), curr->offset, curr->skip_updates, curr);

    if (r.register_count == 0) {
      // this is the first register in range
      r.start_address = curr->start_address;
      r.register_count = curr->register_count;
      r.register_type = curr->register_type;
      r.sensors.insert(curr);
      r.skip_updates = curr->skip_updates;
      r.skip_updates_counter = 0;
      buffer_offset = curr->get_register_size();

      ESP_LOGV(TAG, "Started new range");
    } else {
      // this is not the first register in range so it might be possible
      // to reuse the last register or extend the current range
      if (!curr->force_new_range && r.register_type == curr->register_type &&
          curr->register_type != ModbusRegisterType::CUSTOM) {
        if (curr->start_address == (r.start_address + r.register_count - prev->register_count) &&
            curr->register_count == prev->register_count && curr->get_register_size() == prev->get_register_size()) {
          // this register can re-use the data from the previous register

          // remove this sensore because start_address is changed (sort-order)
          ix = sensorset_.erase(ix);

          curr->start_address = r.start_address;
          curr->offset += prev->offset;

          sensorset_.insert(curr);
          // move iterator backwards because it will be incremented later
          ix--;

          ESP_LOGV(TAG, "Re-use previous register - change to register: 0x%X %d offset=%u", curr->start_address,
                   curr->register_count, curr->offset);
        } else if (curr->start_address == (r.start_address + r.register_count)) {
          // this register can extend the current range

          // remove this sensore because start_address is changed (sort-order)
          ix = sensorset_.erase(ix);

          curr->start_address = r.start_address;
          curr->offset += buffer_offset;
          buffer_offset += curr->get_register_size();
          r.register_count += curr->register_count;

          sensorset_.insert(curr);
          // move iterator backwards because it will be incremented later
          ix--;

          ESP_LOGV(TAG, "Extend range - change to register: 0x%X %d offset=%u", curr->start_address,
                   curr->register_count, curr->offset);
        }
      }
    }

    if (curr->start_address == r.start_address && curr->register_type == r.register_type) {
      // use the lowest non zero value for the whole range
      // Because zero is the default value for skip_updates it is excluded from getting the min value.
      if (curr->skip_updates != 0) {
        if (r.skip_updates != 0) {
          r.skip_updates = std::min(r.skip_updates, curr->skip_updates);
        } else {
          r.skip_updates = curr->skip_updates;
        }
      }

      // add sensor to this range
      r.sensors.insert(curr);

      ix++;
    } else {
      ESP_LOGV(TAG, "Add range 0x%X %d skip:%d", r.start_address, r.register_count, r.skip_updates);
      register_ranges_.push_back(r);
      r = {};
      buffer_offset = 0;
      // do not increment the iterator here because the current sensor has to be re-evaluated
    }

    prev = curr;
  }

  if (r.register_count > 0) {
    // Add the last range
    ESP_LOGV(TAG, "Add last range 0x%X %d skip:%d", r.start_address, r.register_count, r.skip_updates);
    register_ranges_.push_back(r);
  }

  return register_ranges_.size();
}

void ModbusController::dump_config() {
  ESP_LOGCONFIG(TAG, "ModbusController:");
  ESP_LOGCONFIG(TAG, "  Address: 0x%02X", this->address_);
  ESP_LOGCONFIG(TAG, "  Max Command Retries: %d", this->max_cmd_retries_);
  ESP_LOGCONFIG(TAG, "  Offline Skip Updates: %d", this->offline_skip_updates_);
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
  ESP_LOGCONFIG(TAG, "sensormap");
  for (auto &it : sensorset_) {
    ESP_LOGCONFIG(TAG, " Sensor type=%zu start=0x%X offset=0x%X count=%d size=%d",
                  static_cast<uint8_t>(it->register_type), it->start_address, it->offset, it->register_count,
                  it->get_register_size());
  }
  ESP_LOGCONFIG(TAG, "ranges");
  for (auto &it : register_ranges_) {
    ESP_LOGCONFIG(TAG, "  Range type=%zu start=0x%X count=%d skip_updates=%d", static_cast<uint8_t>(it.register_type),
                  it.start_address, it.register_count, it.skip_updates);
  }
  ESP_LOGCONFIG(TAG, "server registers");
  for (auto &r : server_registers_) {
    ESP_LOGCONFIG(TAG, "  Address=0x%02X value_type=%zu register_count=%u", r->address,
                  static_cast<uint8_t>(r->value_type), r->register_count);
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
    // all messages processed send pending commands
    send_next_command_();
  }
}

void ModbusController::on_write_register_response(ModbusRegisterType register_type, uint16_t start_address,
                                                  const std::vector<uint8_t> &data) {
  ESP_LOGV(TAG, "Command ACK 0x%X %d ", get_data<uint16_t>(data, 0), get_data<int16_t>(data, 1));
}

void ModbusController::dump_sensors_() {
  ESP_LOGV(TAG, "sensors");
  for (auto &it : sensorset_) {
    ESP_LOGV(TAG, "  Sensor start=0x%X count=%d size=%d offset=%d", it->start_address, it->register_count,
             it->get_register_size(), it->offset);
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
    auto decoded_value = decode_value(v);
    cmd.payload.push_back(decoded_value[0]);
    cmd.payload.push_back(decoded_value[1]);
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
                                                                 uint16_t value) {
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

  auto decoded_value = decode_value(value);
  cmd.payload.push_back(decoded_value[0]);
  cmd.payload.push_back(decoded_value[1]);
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
    cmd.on_data_func = [](ModbusRegisterType register_type, uint16_t start_address, const std::vector<uint8_t> &data) {
      ESP_LOGI(TAG, "Custom Command sent");
    };
  } else {
    cmd.on_data_func = handler;
  }
  cmd.payload = values;

  return cmd;
}

ModbusCommandItem ModbusCommandItem::create_custom_command(
    ModbusController *modbusdevice, const std::vector<uint16_t> &values,
    std::function<void(ModbusRegisterType register_type, uint16_t start_address, const std::vector<uint8_t> &data)>
        &&handler) {
  ModbusCommandItem cmd = {};
  cmd.modbusdevice = modbusdevice;
  cmd.function_code = ModbusFunctionCode::CUSTOM;
  if (handler == nullptr) {
    cmd.on_data_func = [](ModbusRegisterType register_type, uint16_t start_address, const std::vector<uint8_t> &data) {
      ESP_LOGI(TAG, "Custom Command sent");
    };
  } else {
    cmd.on_data_func = handler;
  }
  for (auto v : values) {
    cmd.payload.push_back((v >> 8) & 0xFF);
    cmd.payload.push_back(v & 0xFF);
  }

  return cmd;
}

bool ModbusCommandItem::send() {
  if (this->function_code != ModbusFunctionCode::CUSTOM) {
    modbusdevice->send(uint8_t(this->function_code), this->register_address, this->register_count, this->payload.size(),
                       this->payload.empty() ? nullptr : &this->payload[0]);
  } else {
    modbusdevice->send_raw(this->payload);
  }
  this->send_count_++;
  ESP_LOGV(TAG, "Command sent %d 0x%X %d send_count: %d", uint8_t(this->function_code), this->register_address,
           this->register_count, this->send_count_);
  return true;
}

bool ModbusCommandItem::is_equal(const ModbusCommandItem &other) {
  // for custom commands we have to check for identical payloads, since
  // address/count/type fields will be set to zero
  return this->function_code == ModbusFunctionCode::CUSTOM
             ? this->payload == other.payload
             : other.register_address == this->register_address && other.register_count == this->register_count &&
                   other.register_type == this->register_type && other.function_code == this->function_code;
}

void number_to_payload(std::vector<uint16_t> &data, int64_t value, SensorValueType value_type) {
  switch (value_type) {
    case SensorValueType::U_WORD:
    case SensorValueType::S_WORD:
      data.push_back(value & 0xFFFF);
      break;
    case SensorValueType::U_DWORD:
    case SensorValueType::S_DWORD:
    case SensorValueType::FP32:
      data.push_back((value & 0xFFFF0000) >> 16);
      data.push_back(value & 0xFFFF);
      break;
    case SensorValueType::U_DWORD_R:
    case SensorValueType::S_DWORD_R:
    case SensorValueType::FP32_R:
      data.push_back(value & 0xFFFF);
      data.push_back((value & 0xFFFF0000) >> 16);
      break;
    case SensorValueType::U_QWORD:
    case SensorValueType::S_QWORD:
      data.push_back((value & 0xFFFF000000000000) >> 48);
      data.push_back((value & 0xFFFF00000000) >> 32);
      data.push_back((value & 0xFFFF0000) >> 16);
      data.push_back(value & 0xFFFF);
      break;
    case SensorValueType::U_QWORD_R:
    case SensorValueType::S_QWORD_R:
      data.push_back(value & 0xFFFF);
      data.push_back((value & 0xFFFF0000) >> 16);
      data.push_back((value & 0xFFFF00000000) >> 32);
      data.push_back((value & 0xFFFF000000000000) >> 48);
      break;
    default:
      ESP_LOGE(TAG, "Invalid data type for modbus number to payload conversation: %d",
               static_cast<uint16_t>(value_type));
      break;
  }
}

int64_t payload_to_number(const std::vector<uint8_t> &data, SensorValueType sensor_value_type, uint8_t offset,
                          uint32_t bitmask) {
  int64_t value = 0;  // int64_t because it can hold signed and unsigned 32 bits

  switch (sensor_value_type) {
    case SensorValueType::U_WORD:
      value = mask_and_shift_by_rightbit(get_data<uint16_t>(data, offset), bitmask);  // default is 0xFFFF ;
      break;
    case SensorValueType::U_DWORD:
    case SensorValueType::FP32:
      value = get_data<uint32_t>(data, offset);
      value = mask_and_shift_by_rightbit((uint32_t) value, bitmask);
      break;
    case SensorValueType::U_DWORD_R:
    case SensorValueType::FP32_R:
      value = get_data<uint32_t>(data, offset);
      value = static_cast<uint32_t>(value & 0xFFFF) << 16 | (value & 0xFFFF0000) >> 16;
      value = mask_and_shift_by_rightbit((uint32_t) value, bitmask);
      break;
    case SensorValueType::S_WORD:
      value = mask_and_shift_by_rightbit(get_data<int16_t>(data, offset),
                                         bitmask);  // default is 0xFFFF ;
      break;
    case SensorValueType::S_DWORD:
      value = mask_and_shift_by_rightbit(get_data<int32_t>(data, offset), bitmask);
      break;
    case SensorValueType::S_DWORD_R: {
      value = get_data<uint32_t>(data, offset);
      // Currently the high word is at the low position
      // the sign bit is therefore at low before the switch
      uint32_t sign_bit = (value & 0x8000) << 16;
      value = mask_and_shift_by_rightbit(
          static_cast<int32_t>(((value & 0x7FFF) << 16 | (value & 0xFFFF0000) >> 16) | sign_bit), bitmask);
    } break;
    case SensorValueType::U_QWORD:
    case SensorValueType::S_QWORD:
      // Ignore bitmask for QWORD
      value = get_data<uint64_t>(data, offset);
      break;
    case SensorValueType::U_QWORD_R:
    case SensorValueType::S_QWORD_R: {
      // Ignore bitmask for QWORD
      uint64_t tmp = get_data<uint64_t>(data, offset);
      value = (tmp << 48) | (tmp >> 48) | ((tmp & 0xFFFF0000) << 16) | ((tmp >> 16) & 0xFFFF0000);
    } break;
    case SensorValueType::RAW:
    default:
      break;
  }
  return value;
}

void ModbusController::add_on_command_sent_callback(std::function<void(int, int)> &&callback) {
  this->command_sent_callback_.add(std::move(callback));
}

}  // namespace modbus_controller
}  // namespace esphome
