#include "modbus_device.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome {
namespace modbus_device {

static const char *const TAG = "modbus_device";

void ModbusDevice::setup() {
  // Modbus::setup();
  //this->create_register_map_();
}

/*
 To work with the existing modbus class and avoid polling for responses a command queue is used.
 send_next_command will submit the command at the top of the queue and set the corresponding callback
 to handle the response from the device.
 Once the response has been processed it is removed from the queue and the next command is sent
*/
bool ModbusDevice::send_next_command_() {
  uint32_t last_send = millis() - this->last_command_timestamp_;

  if ((last_send > this->command_throttle_) && !command_queue_.empty()) {
    auto &command = command_queue_.front();

    ESP_LOGV(TAG, "Sending next modbus command to device %d register 0x%02X count %d", this->address_,
             command->register_address, command->register_count);
    command->send();
    this->last_command_timestamp_ = millis();
    command_queue_.pop_front();
  }
  return (!command_queue_.empty());
}

// Queue incoming response
void ModbusDevice::on_modbus_data(const std::vector<uint8_t> &data) {
  auto fc = modbus::ModbusFunctionCode(data[0]);
  modbus::ModbusRegisterType register_type = modbus_function_read_register_type(fc);
  if (fc == modbus::ModbusFunctionCode::READ_COILS || fc == modbus::ModbusFunctionCode::READ_DISCRETE_INPUTS || fc == modbus::ModbusFunctionCode::READ_HOLDING_REGISTERS || fc == modbus::ModbusFunctionCode::READ_INPUT_REGISTERS) {
    ESP_LOGV(TAG, "Handling request %s", format_hex_pretty(data).c_str());
    uint16_t start_address = encode_uint16(data[1], data[2]);
    auto command = ModbusCommandItem::create_read_command(this, register_type, start_address, data);
    for (auto &item : incoming_queue_) {
      if (item->is_equal(command)) {
        item->payload = command.payload;
	return;
      }
    }
    incoming_queue_.push_back(make_unique<ModbusCommandItem>(command));
  } else {
    ESP_LOGD(TAG, "Unhandled command %02x received", (unsigned int)fc);
  }
}

// Dispatch the response to the registered handler
void ModbusDevice::process_modbus_data_(const ModbusCommandItem *response) {
  ESP_LOGV(TAG, "Process modbus request for address 0x%X size: %zu FC: %u, RT: %u", response->register_address,
           response->payload.size(), (unsigned int)response->function_code, (unsigned int)response->register_type);
  response->on_data_func(response->register_type, response->register_address, response->payload);
}

void ModbusDevice::on_modbus_error(uint8_t function_code, uint8_t exception_code) {
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
    command_queue_.pop_front();
  }
}

std::pair<uint16_t, uint16_t> ModbusDevice::get_message_info(const uint8_t* payload, size_t payload_len) {
  auto result = std::make_pair<uint16_t, uint16_t>(-1, -1);
  auto function_code = modbus::ModbusFunctionCode(payload[0]);
  if (function_code == modbus::ModbusFunctionCode::READ_COILS || function_code == modbus::ModbusFunctionCode::READ_DISCRETE_INPUTS || function_code == modbus::ModbusFunctionCode::READ_HOLDING_REGISTERS || function_code == modbus::ModbusFunctionCode::READ_INPUT_REGISTERS) {
    result.first = 5;
    result.second = 1;
  } else {
    ESP_LOGV(TAG, "Received Server request %02X", uint16_t(function_code));
  }
  ESP_LOGV(TAG, "Message info offset: %u, len: %u", result.second, result.first);
  return result;
}
SensorSet ModbusDevice::find_sensors_(modbus::ModbusRegisterType register_type, uint16_t start_address) const {
  SensorSet result;
  auto reg_it = copy_if(begin(sensorset_), end(sensorset_), std::inserter(result, result.end()), [=](SensorItem const *s) {
    return (s->register_type == register_type && s->start_address >= start_address);
  });

  if (result.empty()) {
    ESP_LOGE(TAG, "No matching sensor found - start_address : 0x%X", start_address);
  } else {
    return result;
  }

  // not found
  return {};
}

SensorSet ModbusDevice::find_sensors_(modbus::ModbusRegisterType register_type, uint16_t start_address, uint16_t end_address) const {
  SensorSet result;
  auto reg_it = copy_if(begin(sensorset_), end(sensorset_), std::inserter(result, result.end()), [=](SensorItem const *s) {
    return (s->register_type == register_type && s->start_address >= start_address && s->start_address <= end_address);
  });

  if (result.empty()) {
    ESP_LOGE(TAG, "No matching sensor found for range start_address : 0x%04X - 0x%04X", start_address, end_address);
  } else {
    return result;
  }

  // not found
  return {};
}

void ModbusDevice::on_register_data(modbus::ModbusRegisterType register_type, uint16_t start_address,
                                        const std::vector<uint8_t> &data) {
  ESP_LOGV(TAG, "data for register address : 0x%X : ", start_address);

  // loop through all sensors with the same start address
  /*auto sensors = find_sensors_(register_type, start_address);
  for (auto *sensor : sensors) {
    sensor->parse_and_publish(data);
  }*/
}

void ModbusDevice::queue_command(const ModbusCommandItem &command) {
  // check if this command is already qeued.
  // not very effective but the queue is never really large
  for (auto &item : command_queue_) {
    if (item->is_equal(command)) {
      ESP_LOGW(TAG, "Duplicate modbus command found: type=0x%x address=%u count=%u",
               static_cast<uint8_t>(command.register_type), command.register_address, command.register_count);
      // update the payload of the queued command
      // replaces a previous command
      item->payload = command.payload;
      return;
    }
  }
  command_queue_.push_back(make_unique<ModbusCommandItem>(command));
}

//
// Queue the modbus requests to be send.
// Once we get a response to the command it is removed from the queue and the next command is send
//
void ModbusDevice::update() {
  if (!command_queue_.empty()) {
    ESP_LOGV(TAG, "%zu modbus commands already in queue", command_queue_.size());
  } else {
    ESP_LOGV(TAG, "Updating modbus component");
  }
  // update written sensors here
}

void ModbusDevice::on_read_register_request(modbus::ModbusRegisterType register_type, uint16_t start_address, uint16_t count) {
  std::vector<uint16_t> payload;
  //payload.resize(count, 0);
  for (auto *it : sensorset_) {
    if (it->register_type == register_type && start_address <= it->start_address && (start_address + count) >= it->start_address) {
      ESP_LOGV(TAG, "Adding data for device 0x%02x[0x02x/0x02x], type 0x%02x", it->start_address, start_address, start_address + count, static_cast<uint8_t>(it->register_type));
      it->add_values_to_payload(payload, it->start_address - start_address);
    }
  }
  if( payload.size() != count) {
    ESP_LOGV(TAG, "Resizing paylaod from 0x%02x to 0x%02x", payload.size(), count);
    payload.resize(count, 0);
  }
  ESP_LOGV(TAG, "Create read response with payload[%u]: %s", payload.size(), format_hex_pretty(payload).c_str());
  auto response = ModbusCommandItem::create_read_response(this, start_address, register_type, payload);
  this->queue_command(response);
}

void ModbusDevice::dump_config() {
  ESP_LOGCONFIG(TAG, "ModbusDevice:");
  ESP_LOGCONFIG(TAG, "  Address: 0x%02X", this->address_);
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
#endif
}

void ModbusDevice::loop() {
  // Incoming data to process?
  if (!incoming_queue_.empty()) {
    auto &message = incoming_queue_.front();
    if (message != nullptr)
      process_modbus_data_(message.get());
    incoming_queue_.pop_front();

  } else {
    // all messages processed send pending commands
    send_next_command_();
  }
}

void ModbusDevice::dump_sensors_() {
  ESP_LOGV(TAG, "sensors");
  for (auto &it : sensorset_) {
    ESP_LOGV(TAG, "  Sensor start=0x%X count=%d size=%d offset=%d", it->start_address, it->register_count,
             it->get_register_size(), it->offset);
  }
}

ModbusCommandItem ModbusCommandItem::create_read_command(ModbusDevice *modbusdevice,
                                                         modbus::ModbusRegisterType register_type, uint16_t start_address,
                                                         std::vector<uint8_t> data) {
  ModbusCommandItem cmd;
  cmd.modbusdevice = modbusdevice;
  cmd.register_type = register_type;
  cmd.function_code = modbus_register_read_function(register_type);
  cmd.register_address = start_address;
  cmd.payload = data;
  cmd.on_data_func = [modbusdevice](modbus::ModbusRegisterType register_type, uint16_t start_address,
                                    std::vector<uint8_t> data) {

    uint16_t count = encode_uint16(data[3], data[4]);
    modbusdevice->on_read_register_request(register_type, start_address, count);
  };
  return cmd;
}

ModbusCommandItem ModbusCommandItem::create_read_response(ModbusDevice *modbusdevice, uint16_t start_address, modbus::ModbusRegisterType register_type,
                                                                 const std::vector<uint16_t> &values) {
  ModbusCommandItem cmd;
  cmd.modbusdevice = modbusdevice;
  cmd.register_type = register_type;
  cmd.function_code = modbus_register_read_function(register_type);
  cmd.register_address = start_address;
  cmd.register_count = values.size();
  cmd.payload = std::vector<uint8_t>();
  for (auto r: values) {
    cmd.payload.push_back(r>>8);
    cmd.payload.push_back(r>>0);
  }

  ESP_LOGV(TAG, "Create read response register type: %i, function code %i,  registers[%i]: [%i]%s", (unsigned int)register_type, (unsigned int)cmd.function_code, cmd.register_count, cmd.payload.size(), format_hex_pretty(cmd.payload).c_str());

  return cmd;
}


ModbusCommandItem ModbusCommandItem::create_custom_command(
    ModbusDevice *modbusdevice, const std::vector<uint8_t> &values,
    std::function<void(modbus::ModbusRegisterType register_type, uint16_t start_address, const std::vector<uint8_t> &data)>
        &&handler) {
  ModbusCommandItem cmd;
  cmd.modbusdevice = modbusdevice;
  cmd.function_code = modbus::ModbusFunctionCode::CUSTOM;
  if (handler == nullptr) {
    cmd.on_data_func = [](modbus::ModbusRegisterType register_type, uint16_t start_address, const std::vector<uint8_t> &data) {
      ESP_LOGI(TAG, "Custom Command sent");
    };
  } else {
    cmd.on_data_func = handler;
  }
  cmd.payload = values;

  return cmd;
}

ModbusCommandItem ModbusCommandItem::create_custom_command(
    ModbusDevice *modbusdevice, const std::vector<uint16_t> &values,
    std::function<void(modbus::ModbusRegisterType register_type, uint16_t start_address, const std::vector<uint8_t> &data)>
        &&handler) {
  ModbusCommandItem cmd = {};
  cmd.modbusdevice = modbusdevice;
  cmd.function_code = modbus::ModbusFunctionCode::CUSTOM;
  if (handler == nullptr) {
    cmd.on_data_func = [](modbus::ModbusRegisterType register_type, uint16_t start_address, const std::vector<uint8_t> &data) {
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
  if (this->function_code != modbus::ModbusFunctionCode::CUSTOM) {
    modbusdevice->send_response(uint8_t(this->function_code), this->register_address, this->payload.size(), this->payload.empty() ? nullptr : &this->payload[0]);
  } else {
    modbusdevice->send_raw(this->payload);
  }
  ESP_LOGV(TAG, "Command sent %d 0x%X %d", uint8_t(this->function_code), this->register_address, this->register_count);
  
  return true;
}

bool ModbusCommandItem::is_equal(const ModbusCommandItem &other) {
  // for custom commands we have to check for identical payloads, since
  // address/count/type fields will be set to zero
  return this->function_code == modbus::ModbusFunctionCode::CUSTOM
             ? this->payload == other.payload
             : other.register_address == this->register_address && other.register_count == this->register_count &&
                   other.register_type == this->register_type && other.function_code == this->function_code;
}

void number_to_payload(std::vector<uint16_t> &data, int64_t value, modbus::SensorValueType value_type) {
  switch (value_type) {
    case modbus::SensorValueType::U_WORD:
    case modbus::SensorValueType::S_WORD:
      data.push_back(value & 0xFFFF);
      break;
    case modbus::SensorValueType::U_DWORD:
    case modbus::SensorValueType::S_DWORD:
    case modbus::SensorValueType::FP32:
    case modbus::SensorValueType::FP32_R:
      data.push_back((value & 0xFFFF0000) >> 16);
      data.push_back(value & 0xFFFF);
      break;
    case modbus::SensorValueType::U_DWORD_R:
    case modbus::SensorValueType::S_DWORD_R:
      data.push_back(value & 0xFFFF);
      data.push_back((value & 0xFFFF0000) >> 16);
      break;
    case modbus::SensorValueType::U_QWORD:
    case modbus::SensorValueType::S_QWORD:
      data.push_back((value & 0xFFFF000000000000) >> 48);
      data.push_back((value & 0xFFFF00000000) >> 32);
      data.push_back((value & 0xFFFF0000) >> 16);
      data.push_back(value & 0xFFFF);
      break;
    case modbus::SensorValueType::U_QWORD_R:
    case modbus::SensorValueType::S_QWORD_R:
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

}  // namespace modbus_device
}  // namespace esphome
