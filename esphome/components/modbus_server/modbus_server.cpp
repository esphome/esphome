#include "modbus_server.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome::modbus_server {
using modbus::ModbusExceptionCode;
using modbus::helpers::registers_to_number;

static const char *const TAG = "modbus_server";

modbus::ServerResponseStatus ModbusServer::on_modbus_read_registers(uint16_t start_address,
                                                                    uint16_t number_of_registers,
                                                                    modbus::ServerRegisterData &out_registers) {
  ESP_LOGD(TAG,
           "Received read holding/input registers for device 0x%X. Start address: 0x%X. Number of registers: 0x%X.",
           this->address_, start_address, number_of_registers);

  for (uint16_t current_address = start_address; current_address < start_address + number_of_registers;) {
    bool found = false;
    for (auto *server_register : this->server_registers_) {
      if (server_register->address == current_address) {
        if (!server_register->read_lambda) {
          break;
        }
        int64_t value = server_register->read_lambda();
        char value_buf[ServerRegister::FORMAT_VALUE_BUF_SIZE];
        ESP_LOGV(TAG, "Matched register. Address: 0x%02X. Value type: %zu. Register count: %u. Value: %s.",
                 server_register->address, static_cast<size_t>(server_register->value_type),
                 server_register->register_count, server_register->format_value(value, value_buf, sizeof(value_buf)));

        modbus::helpers::number_to_payload(out_registers, value, server_register->value_type);
        current_address += server_register->register_count;
        found = true;
        break;
      }
    }

    if (!found) {
      if (this->server_courtesy_response_.enabled &&
          (current_address <= this->server_courtesy_response_.register_last_address)) {
        ESP_LOGV(TAG,
                 "Could not match any register to address 0x%02X, but default allowed. "
                 "Returning default value: %" PRIu16 ".",
                 current_address, this->server_courtesy_response_.register_value);
        out_registers.push_back(this->server_courtesy_response_.register_value);
        current_address += 1;  // Just increment by 1, as the default response is a single register
      } else {
        ESP_LOGW(TAG,
                 "Could not match any register to address 0x%02X and default not allowed. Sending exception response.",
                 current_address);
        return ModbusExceptionCode::ILLEGAL_DATA_ADDRESS;
      }
    }
  }

  return {};
}

modbus::ServerResponseStatus ModbusServer::on_modbus_write_registers(uint16_t start_address,
                                                                     uint16_t number_of_registers,
                                                                     const modbus::ServerRegisterData &in_registers) {
  // in_registers holds the register values in host byte order; the caller has validated the register count.
  ESP_LOGD(TAG, "Received write registers for device 0x%X. Start address: 0x%X. Number of registers: 0x%X.",
           this->address_, start_address, number_of_registers);

  auto for_each_register =
      [this, start_address,
       number_of_registers](const std::function<bool(ServerRegister *, uint16_t register_offset)> &callback) -> bool {
    uint16_t register_offset = 0;
    for (uint16_t current_address = start_address; current_address < start_address + number_of_registers;) {
      bool ok = false;
      for (auto *server_register : this->server_registers_) {
        if (server_register->address == current_address) {
          ok = callback(server_register, register_offset);
          current_address += server_register->register_count;
          register_offset += server_register->register_count;
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
  if (!for_each_register([](ServerRegister *server_register, uint16_t register_offset) -> bool {
        return server_register->write_lambda != nullptr;
      })) {
    ESP_LOGW(TAG, "Invalid register address. Sending exception response.");
    return ModbusExceptionCode::ILLEGAL_DATA_ADDRESS;
  }

  // Actually write to the registers:
  if (!for_each_register([&in_registers](ServerRegister *server_register, uint16_t register_offset) {
        bool error = false;
        int64_t number = registers_to_number(in_registers.data(), in_registers.size(), server_register->value_type,
                                             register_offset, 0xFFFFFFFF, &error);
        if (error) {
          return false;
        } else {
          return server_register->write_lambda(number);
        }
      })) {
    ESP_LOGW(TAG, "Could not write all registers. Sending exception response.");
    return ModbusExceptionCode::SERVICE_DEVICE_FAILURE;
  }

  // Success: the caller builds the write response (an echo of the request header).
  return {};
}

void ModbusServer::dump_config() {
  ESP_LOGCONFIG(TAG,
                "ModbusServer:\n"
                "  Address: 0x%02X\n"
                "  Server Courtesy Response:\n"
                "    Enabled: %s\n"
                "    Register Last Address: 0x%02X\n"
                "    Register Value: %" PRIu16,
                this->address_, this->server_courtesy_response_.enabled ? "true" : "false",
                this->server_courtesy_response_.register_last_address, this->server_courtesy_response_.register_value);

#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
  ESP_LOGCONFIG(TAG, "server registers");
  for (auto &r : this->server_registers_) {
    ESP_LOGCONFIG(TAG, "  Address=0x%02X value_type=%u register_count=%u", r->address,
                  static_cast<uint8_t>(r->value_type), r->register_count);
  }
#endif
}

}  // namespace esphome::modbus_server
