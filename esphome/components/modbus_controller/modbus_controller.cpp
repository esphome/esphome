#include "modbus_controller.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome {
namespace modbus_controller {

static const char *const TAG = "modbus_controller";

void ModbusController::setup() {
  // Modbus::setup();
  this->create_register_ranges();
}

/*
 To work with the existing modbus class and avoid polling for responses a command queue is used.
 send_next_command will submit the command at the top of the queue and set the corresponding callback
 to handle the response from the device.
 Once the response has been processed it is removed from the queue and the next command is sent
*/
bool ModbusController::send_next_command_() {
  uint32_t last_send = millis() - this->last_command_timestamp_;

// if (!command_queue_.empty() && this->waiting_for_response()) {
//    ESP_LOGV(TAG, "Sending delayed - waiting for previous response");
//  }

  if ((last_send > this->command_throttle_) && !waiting_for_response() && !command_queue_.empty()) {
    auto &command = command_queue_.front();

    ESP_LOGD(TAG, "Sending next modbus command to device %d register 0x%02X", this->address_,
             command->register_address);
    command->send();
    this->last_command_timestamp_ = millis();
    if (!command->on_data_func) {  // No handler remove from queue directly after sending
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
void ModbusController::process_modbus_data(const ModbusCommandItem *response) {
  ESP_LOGD(TAG, "Process modbus response for address 0x%X size: %zu", response->register_address,
           response->payload.size());
  response->on_data_func(response->function_code, response->register_address, response->payload);
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

void ModbusController::on_register_data(ModbusFunctionCode function_code, uint16_t start_address,
                                        const std::vector<uint8_t> &data) {
  ESP_LOGV(TAG, "data for register address : 0x%X : ", start_address);

  auto vec_it = find_if(begin(register_ranges_), end(register_ranges_), [=](RegisterRange const &r) {
    return (r.start_address == start_address && r.register_type == function_code);
  });

  if (vec_it == register_ranges_.end()) {
    ESP_LOGE(TAG, "Handle incoming data : No matching range for sensor found - start_address :  0x%X", start_address);
    return;
  }
  auto map_it = sensormap_.find(vec_it->first_sensorkey);
  if (map_it == sensormap_.end()) {
    ESP_LOGE(TAG, "Handle incoming data : No sensor found in at start_address :  0x%X", start_address);
    return;
  }
  // loop through all sensors with the same start address
  while (map_it != sensormap_.end() && map_it->second->start_address == start_address) {
    if (map_it->second->register_type == function_code) {
      float val = map_it->second->parse_and_publish(data);
    }
    map_it++;
  }
}

void ModbusController::queue_command(const ModbusCommandItem &command) {
  // check if this commmand is already qeued.
  // not very effective but the queue is never really large
  for (auto &item : command_queue_) {
    if (item->register_address == command.register_address && item->register_count == command.register_count &&
        item->function_code == command.function_code) {
      ESP_LOGW(TAG, "Duplicate modbus command found");
      // update the payload of the queued command
      // replaces a previous command
      item->payload = command.payload;
      return;
    }
  }
  command_queue_.push_back(make_unique<ModbusCommandItem>(command));
}

void ModbusController::update_range(RegisterRange &r) {
  ESP_LOGV(TAG, "Range : %X Size: %x (%d) skip: %d", r.start_address, r.register_count, (int) r.register_type,
           r.skip_updates_counter);
  if (r.skip_updates_counter == 0) {
    ModbusCommandItem command_item =
        ModbusCommandItem::create_read_command(this, r.register_type, r.start_address, r.register_count);
    queue_command(command_item);
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
    update_range(r);
  }
}

// walk through the sensors and determine the registerranges to read
size_t ModbusController::create_register_ranges() {
  register_ranges_.clear();
  uint8_t n = 0;
  if (sensormap_.empty()) {
    return 0;
  }
  // map is already sorted by keys so we start with the lowest address ;
  auto ix = sensormap_.begin();
  auto prev = ix;
  uint16_t current_start_address = ix->second->start_address;
  uint8_t buffer_offset = ix->second->offset;
  uint8_t skip_updates = ix->second->skip_updates;
  auto first_sensorkey = ix->second->getkey();
  int total_register_count = 0;
  while (ix != sensormap_.end()) {
    // use the lowest non zero value for the whole range
    // Because zero is the default value for skip_updates it is excluded from getting the min value.
    ESP_LOGV(TAG, "Register: 0x%X %d %d  0x%llx (%d) buffer_offset = %d (0x%X) skip=%u",
             ix->second->start_address, ix->second->register_count,
             ix->second->offset, ix->second->getkey(), total_register_count, buffer_offset, buffer_offset,
             ix->second->skip_updates);
    if (current_start_address != ix->second->start_address ||
        //  ( prev->second->start_address + prev->second->offset != ix->second->start_address) ||
        ix->second->register_type != prev->second->register_type) {
      // Difference doesn't match so we have a gap
      if (n > 0) {
        RegisterRange r;
        r.start_address = current_start_address;
        r.register_count = total_register_count;
        if (prev->second->register_type == ModbusFunctionCode::READ_COILS ||
            prev->second->register_type == ModbusFunctionCode::READ_DISCRETE_INPUTS) {
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
      // Use the lowest non zero skip_upates value for the range
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
    if (prev->second->register_type == ModbusFunctionCode::READ_COILS ||
        prev->second->register_type == ModbusFunctionCode::READ_DISCRETE_INPUTS) {
      r.register_count = prev->second->offset + 1;
    }
    r.register_type = prev->second->register_type;
    r.first_sensorkey = first_sensorkey;
    r.skip_updates = skip_updates;
    r.skip_updates_counter = 0;
    register_ranges_.push_back(r);
  }
  return register_ranges_.size();
}

void ModbusController::dump_config() {
  ESP_LOGCONFIG(TAG, "ModbusController:");
  ESP_LOGCONFIG(TAG, "  Address: 0x%02X", this->address_);
}

void ModbusController::loop() {
  // Incoming data to process?

  /*
    if (!incoming_data.empty()) {
      auto &message = incoming_data.front();
      process_modbus_data(message);
      incoming_data.pop();
  */
  //  read_uart();
  if (!incoming_queue_.empty()) {
    auto &message = incoming_queue_.front();
    if (message != nullptr)
      process_modbus_data(message.get());
    incoming_queue_.pop();

  } else {
    // all messages processed send pending commmands
    send_next_command_();
  }
}

void ModbusController::on_write_register_response(ModbusFunctionCode function_code, uint16_t start_address,
                                                  const std::vector<uint8_t> &data) {
  ESP_LOGV(TAG, "Command ACK 0x%X %d ", get_data<uint16_t>(data, 0), get_data<int16_t>(data, 1));
}

// factory methods
ModbusCommandItem ModbusCommandItem::create_read_command(
    ModbusController *modbusdevice, ModbusFunctionCode function_code, uint16_t start_address, uint16_t register_count,
    std::function<void(ModbusFunctionCode function_code, uint16_t start_address, const std::vector<uint8_t> &data)>
        &&handler) {
  ModbusCommandItem cmd;
  cmd.modbusdevice = modbusdevice;
  cmd.function_code = function_code;
  cmd.register_address = start_address;
  cmd.register_count = register_count;
  cmd.on_data_func = std::move(handler);
  return cmd;
}

ModbusCommandItem ModbusCommandItem::create_read_command(ModbusController *modbusdevice,
                                                         ModbusFunctionCode function_code, uint16_t start_address,
                                                         uint16_t register_count) {
  ModbusCommandItem cmd;
  cmd.modbusdevice = modbusdevice;
  cmd.function_code = function_code;
  cmd.register_address = start_address;
  cmd.register_count = register_count;
  cmd.on_data_func = [modbusdevice](ModbusFunctionCode function_code, uint16_t start_address,
                                    const std::vector<uint8_t> &data) {
    modbusdevice->on_register_data(function_code, start_address, data);
  };
  return cmd;
}

ModbusCommandItem ModbusCommandItem::create_write_multiple_command(ModbusController *modbusdevice,
                                                                   uint16_t start_address, uint16_t register_count,
                                                                   const std::vector<uint16_t> &values) {
  ModbusCommandItem cmd;
  cmd.modbusdevice = modbusdevice;
  cmd.function_code = ModbusFunctionCode::WRITE_MULTIPLE_REGISTERS;
  cmd.register_address = start_address;
  cmd.register_count = register_count;
  cmd.on_data_func = [modbusdevice, cmd](ModbusFunctionCode function_code, uint16_t start_address,
                                         const std::vector<uint8_t> &data) {
    modbusdevice->on_write_register_response(cmd.function_code, start_address, data);
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
  cmd.function_code = ModbusFunctionCode::WRITE_SINGLE_COIL;
  cmd.register_address = address;
  cmd.register_count = 1;
  cmd.on_data_func = [modbusdevice, cmd](ModbusFunctionCode function_code, uint16_t start_address,
                                         const std::vector<uint8_t> &data) {
    modbusdevice->on_write_register_response(cmd.function_code, start_address, data);
  };
  cmd.payload.push_back(value ? 0xFF : 0);
  cmd.payload.push_back(0);
  return cmd;
}

ModbusCommandItem ModbusCommandItem::create_write_multiple_coils(ModbusController *modbusdevice, uint16_t start_address,
                                                                 const std::vector<bool> &values) {
  ModbusCommandItem cmd;
  cmd.modbusdevice = modbusdevice;
  cmd.function_code = ModbusFunctionCode::WRITE_MULTIPLE_COILS;
  cmd.register_address = start_address;
  cmd.register_count = values.size();
  cmd.on_data_func = [modbusdevice, cmd](ModbusFunctionCode function_code, uint16_t start_address,
                                         const std::vector<uint8_t> &data) {
    modbusdevice->on_write_register_response(cmd.function_code, start_address, data);
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
  cmd.function_code = ModbusFunctionCode::WRITE_SINGLE_REGISTER;
  cmd.register_address = start_address;
  cmd.register_count = 1;  // not used here anyways
  cmd.on_data_func = [modbusdevice, cmd](ModbusFunctionCode function_code, uint16_t start_address,
                                         const std::vector<uint8_t> &data) {
    modbusdevice->on_write_register_response(cmd.function_code, start_address, data);
  };
  cmd.payload.push_back((value / 256) & 0xFF);
  cmd.payload.push_back((value % 256) & 0xFF);
  return cmd;
}

ModbusCommandItem ModbusCommandItem::create_custom_command(ModbusController *modbusdevice,
                                                           const std::vector<uint8_t> &values) {
  ModbusCommandItem cmd;
  cmd.modbusdevice = modbusdevice;
  cmd.function_code = ModbusFunctionCode::CUSTOM;
  cmd.on_data_func = [](ModbusFunctionCode, uint16_t, const std::vector<uint8_t> &data) {
    ESP_LOGI(TAG, "Custom Command sent");
  };
  cmd.payload = values;

  return cmd;
}

bool ModbusCommandItem::send() {
  if (this->function_code != ModbusFunctionCode::CUSTOM) {
    modbusdevice->send_with_payload(uint8_t(this->function_code), this->register_address, this->register_count,
                                    this->payload.size(), this->payload.empty() ? nullptr : &this->payload[0]);
  } else {
    modbusdevice->send_raw(this->payload);
  }
  ESP_LOGV(TAG, "Command sent %d 0x%X %d", uint8_t(this->function_code), this->register_address, this->register_count);
  return true;
}

}  // namespace modbus_controller
}  // namespace esphome
