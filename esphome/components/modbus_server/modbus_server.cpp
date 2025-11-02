#include "modbus_server.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome {
namespace modbus_server {

static const char *const TAG = "modbus_server";

void ModbusServer::on_modbus_read_registers(uint8_t function_code, uint16_t start_address,
                                            uint16_t number_of_registers) {
  ESP_LOGD(TAG,
           "Received read holding/input registers for device 0x%X. FC: 0x%X. Start address: 0x%X. Number of registers: "
           "0x%X.",
           this->address_, function_code, start_address, number_of_registers);

  if (number_of_registers == 0 || number_of_registers > modbus::MAX_NUM_OF_REGISTERS_TO_READ) {
    ESP_LOGW(TAG, "Invalid number of registers %d. Sending exception response.", number_of_registers);
    this->send_error(function_code, ModbusExceptionCode::ILLEGAL_DATA_ADDRESS);
    return;
  }

  std::vector<uint16_t> sixteen_bit_response;
  for (uint16_t current_address = start_address; current_address < start_address + number_of_registers;) {
    bool found = false;
    for (auto *server_register : this->server_registers_) {
      if (server_register->address == current_address) {
        if (!server_register->read_lambda) {
          break;
        }
        int64_t value = server_register->read_lambda();
        ESP_LOGD(TAG, "Matched register. Address: 0x%02X. Value type: %zu. Register count: %u. Value: %s.",
                 server_register->address, static_cast<size_t>(server_register->value_type),
                 server_register->register_count, server_register->format_value(value).c_str());

        std::vector<uint16_t> payload;
        payload.reserve(server_register->register_count * 2);
        number_to_payload(payload, value, server_register->value_type);
        sixteen_bit_response.insert(sixteen_bit_response.end(), payload.cbegin(), payload.cend());
        current_address += server_register->register_count;
        found = true;
        break;
      }
    }

    if (!found) {
      if (this->server_courtesy_response_.enabled &&
          (current_address <= this->server_courtesy_response_.register_last_address)) {
        ESP_LOGD(TAG,
                 "Could not match any register to address 0x%02X, but default allowed. "
                 "Returning default value: %d.",
                 current_address, this->server_courtesy_response_.register_value);
        sixteen_bit_response.push_back(this->server_courtesy_response_.register_value);
        current_address += 1;  // Just increment by 1, as the default response is a single register
      } else {
        ESP_LOGW(TAG,
                 "Could not match any register to address 0x%02X and default not allowed. Sending exception response.",
                 current_address);
        this->send_error(function_code, ModbusExceptionCode::ILLEGAL_DATA_ADDRESS);
        return;
      }
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

void ModbusServer::on_modbus_write_registers(uint8_t function_code, const std::vector<uint8_t> &data) {
  uint16_t number_of_registers;
  uint16_t payload_offset;

  if (function_code == ModbusFunctionCode::WRITE_MULTIPLE_REGISTERS) {
    number_of_registers = uint16_t(data[3]) | (uint16_t(data[2]) << 8);
    if (number_of_registers == 0 || number_of_registers > modbus::MAX_NUM_OF_REGISTERS_TO_WRITE) {
      ESP_LOGW(TAG, "Invalid number of registers %d. Sending exception response.", number_of_registers);
      this->send_error(function_code, ModbusExceptionCode::ILLEGAL_DATA_VALUE);
      return;
    }
    uint16_t payload_size = data[4];
    if (payload_size != number_of_registers * 2) {
      ESP_LOGW(TAG, "Payload size of %d bytes is not 2 times the number of registers (%d). Sending exception response.",
               payload_size, number_of_registers);
      this->send_error(function_code, ModbusExceptionCode::ILLEGAL_DATA_VALUE);
      return;
    }
    payload_offset = 5;
  } else if (function_code == ModbusFunctionCode::WRITE_SINGLE_REGISTER) {
    number_of_registers = 1;
    payload_offset = 2;
  } else {
    ESP_LOGW(TAG, "Invalid function code 0x%X. Sending exception response.", function_code);
    this->send_error(function_code, ModbusExceptionCode::ILLEGAL_FUNCTION);
    return;
  }

  uint16_t start_address = uint16_t(data[1]) | (uint16_t(data[0]) << 8);
  ESP_LOGD(TAG,
           "Received write holding registers for device 0x%X. FC: 0x%X. Start address: 0x%X. Number of registers: "
           "0x%X.",
           this->address_, function_code, start_address, number_of_registers);

  auto for_each_register = [this, start_address, number_of_registers, payload_offset](
                               const std::function<bool(ServerRegister *, uint16_t offset)> &callback) -> bool {
    uint16_t offset = payload_offset;
    for (uint16_t current_address = start_address; current_address < start_address + number_of_registers;) {
      bool ok = false;
      for (auto *server_register : this->server_registers_) {
        if (server_register->address == current_address) {
          ok = callback(server_register, offset);
          current_address += server_register->register_count;
          offset += server_register->register_count * sizeof(uint16_t);
          break;
        }
      }

      if (!ok) {
        return false;
      }
    }
    return true;
  };

  // check all registers are writable before writing to any of them:
  if (!for_each_register([](ServerRegister *server_register, uint16_t offset) -> bool {
        return server_register->write_lambda != nullptr;
      })) {
    this->send_error(function_code, ModbusExceptionCode::ILLEGAL_FUNCTION);
    return;
  }

  // Actually write to the registers:
  if (!for_each_register([&data](ServerRegister *server_register, uint16_t offset) {
        int64_t number = payload_to_number(data, server_register->value_type, offset, 0xFFFFFFFF);
        return server_register->write_lambda(number);
      })) {
    this->send_error(function_code, ModbusExceptionCode::SERVICE_DEVICE_FAILURE);
    return;
  }

  std::vector<uint8_t> response;
  response.reserve(6);
  response.push_back(this->address_);
  response.push_back(function_code);
  response.insert(response.end(), data.begin(), data.begin() + 4);
  this->send_raw(response);
}

void ModbusServer::dump_config() {
  ESP_LOGCONFIG(TAG,
                "ModbusServer:\n"
                "  Address: 0x%02X\n"
                "  Server Courtesy Response:\n"
                "    Enabled: %s\n"
                "    Register Last Address: 0x%02X\n"
                "    Register Value: %d",
                this->address_, this->server_courtesy_response_.enabled ? "true" : "false",
                this->server_courtesy_response_.register_last_address, this->server_courtesy_response_.register_value);

#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
  ESP_LOGCONFIG(TAG, "server registers");
  for (auto &r : this->server_registers_) {
    ESP_LOGCONFIG(TAG, "  Address=0x%02X value_type=%zu register_count=%u", r->address,
                  static_cast<uint8_t>(r->value_type), r->register_count);
  }
#endif
}

// TODO: Share this with controller
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

// TODO: Share this with controller
int64_t payload_to_number(const std::vector<uint8_t> &data, SensorValueType sensor_value_type, uint8_t offset,
                          uint32_t bitmask) {
  int64_t value = 0;  // int64_t because it can hold signed and unsigned 32 bits

  size_t size = data.size() - offset;
  bool error = false;
  switch (sensor_value_type) {
    case SensorValueType::U_WORD:
      if (size >= 2) {
        value = mask_and_shift_by_rightbit(get_data<uint16_t>(data, offset), bitmask);  // default is 0xFFFF ;
      } else {
        error = true;
      }
      break;
    case SensorValueType::U_DWORD:
    case SensorValueType::FP32:
      if (size >= 4) {
        value = get_data<uint32_t>(data, offset);
        value = mask_and_shift_by_rightbit((uint32_t) value, bitmask);
      } else {
        error = true;
      }
      break;
    case SensorValueType::U_DWORD_R:
    case SensorValueType::FP32_R:
      if (size >= 4) {
        value = get_data<uint32_t>(data, offset);
        value = static_cast<uint32_t>(value & 0xFFFF) << 16 | (value & 0xFFFF0000) >> 16;
        value = mask_and_shift_by_rightbit((uint32_t) value, bitmask);
      } else {
        error = true;
      }
      break;
    case SensorValueType::S_WORD:
      if (size >= 2) {
        value = mask_and_shift_by_rightbit(get_data<int16_t>(data, offset),
                                           bitmask);  // default is 0xFFFF ;
      } else {
        error = true;
      }
      break;
    case SensorValueType::S_DWORD:
      if (size >= 4) {
        value = mask_and_shift_by_rightbit(get_data<int32_t>(data, offset), bitmask);
      } else {
        error = true;
      }
      break;
    case SensorValueType::S_DWORD_R: {
      if (size >= 4) {
        value = get_data<uint32_t>(data, offset);
        // Currently the high word is at the low position
        // the sign bit is therefore at low before the switch
        uint32_t sign_bit = (value & 0x8000) << 16;
        value = mask_and_shift_by_rightbit(
            static_cast<int32_t>(((value & 0x7FFF) << 16 | (value & 0xFFFF0000) >> 16) | sign_bit), bitmask);
      } else {
        error = true;
      }
    } break;
    case SensorValueType::U_QWORD:
    case SensorValueType::S_QWORD:
      // Ignore bitmask for QWORD
      if (size >= 8) {
        value = get_data<uint64_t>(data, offset);
      } else {
        error = true;
      }
      break;
    case SensorValueType::U_QWORD_R:
    case SensorValueType::S_QWORD_R: {
      // Ignore bitmask for QWORD
      if (size >= 8) {
        uint64_t tmp = get_data<uint64_t>(data, offset);
        value = (tmp << 48) | (tmp >> 48) | ((tmp & 0xFFFF0000) << 16) | ((tmp >> 16) & 0xFFFF0000);
      } else {
        error = true;
      }
    } break;
    case SensorValueType::RAW:
    default:
      break;
  }
  if (error)
    ESP_LOGE(TAG, "not enough data for value");
  return value;
}

}  // namespace modbus_server
}  // namespace esphome
