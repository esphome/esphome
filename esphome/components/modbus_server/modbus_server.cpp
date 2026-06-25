#include "modbus_server.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome::modbus_server {
using modbus::ModbusExceptionCode;
using modbus::helpers::registers_to_number;

static const char *const TAG = "modbus_server";

ServerRegister *ModbusServer::find_containing_register_(uint32_t address) const {
  for (auto *server_register : this->server_registers_) {
    if (address >= server_register->address &&
        address < static_cast<uint32_t>(server_register->address) + server_register->register_count) {
      return server_register;
    }
  }
  return nullptr;
}

modbus::ServerResponseStatus ModbusServer::on_modbus_read_registers(uint16_t start_address,
                                                                    uint16_t number_of_registers,
                                                                    modbus::RegisterValues &registers) {
  ESP_LOGV(TAG,
           "Received read holding/input registers for device 0x%X. Start address: 0x%X. Number of registers: 0x%X.",
           this->address_, start_address, number_of_registers);

  const uint32_t end_address = static_cast<uint32_t>(start_address) + number_of_registers;
  uint32_t current_address = start_address;
  while (current_address < end_address) {
    ServerRegister *server_register = this->find_containing_register_(current_address);

    if (server_register == nullptr) {
      // Unregistered address: optionally answer with the courtesy default, otherwise reject.
      if (this->server_courtesy_response_.enabled &&
          current_address <= this->server_courtesy_response_.register_last_address) {
        ESP_LOGV(TAG, "No register at 0x%04X; returning courtesy default %" PRIu16 ".",
                 static_cast<uint16_t>(current_address), this->server_courtesy_response_.register_value);
        registers.push_back(this->server_courtesy_response_.register_value);
        current_address += 1;  // the courtesy default is always a single register
        continue;
      }
      ESP_LOGW(TAG, "No register at 0x%04X and courtesy default not allowed. Sending exception response.",
               static_cast<uint16_t>(current_address));
      return ModbusExceptionCode::ILLEGAL_DATA_ADDRESS;
    }

    if (!server_register->read_lambda) {
      // Registered but not readable (write-only); don't mask it with the courtesy default.
      ESP_LOGW(TAG, "Register at 0x%04X is not readable. Sending exception response.", server_register->address);
      return ModbusExceptionCode::ILLEGAL_DATA_ADDRESS;
    }

    // A multi-register value is atomic: the request must start at the value's first register and cover all of
    // it. Starting inside the value, or stopping short of its end, clips it and is rejected.
    const bool clipped =
        current_address != server_register->address ||
        static_cast<uint32_t>(server_register->address) + server_register->register_count > end_address;
    if (clipped) {
      ESP_LOGW(TAG, "Read clips the multi-register value at 0x%04X. Sending exception response.",
               server_register->address);
      return ModbusExceptionCode::ILLEGAL_DATA_ADDRESS;
    }

    int64_t value = server_register->read_lambda();
    char value_buf[ServerRegister::FORMAT_VALUE_BUF_SIZE];
    ESP_LOGV(TAG, "Matched register. Address: 0x%02X. Value type: %zu. Register count: %u. Value: %s.",
             server_register->address, static_cast<size_t>(server_register->value_type),
             server_register->register_count, server_register->format_value(value, value_buf, sizeof(value_buf)));
    modbus::helpers::number_to_payload(registers, value, server_register->value_type);
    current_address += server_register->register_count;
  }

  return {};
}

modbus::ServerResponseStatus ModbusServer::on_modbus_write_registers(uint16_t start_address,
                                                                     const modbus::RegisterValues &registers) {
  // registers holds the values to write in host byte order; its size is the register count.
  ESP_LOGV(TAG, "Received write registers for device 0x%X. Start address: 0x%X. Number of registers: 0x%zX.",
           this->address_, start_address, registers.size());

  auto for_each_register =
      [this, start_address,
       &registers](const std::function<bool(ServerRegister *, uint16_t register_offset)> &callback) -> bool {
    uint16_t register_offset = 0;
    for (uint16_t current_address = start_address; current_address < start_address + registers.size();) {
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

  // Pre-flight: every targeted register must be writable AND have its full value present in the request,
  // so we never apply a partial write before discovering a problem. The commit pass below re-runs
  // registers_to_number rather than caching the decoded values: using the same function for the check and
  // the write keeps a single source of truth for the decode bound, independent of how register_count was set.
  ModbusExceptionCode precheck = ModbusExceptionCode::ILLEGAL_DATA_ADDRESS;  // unmatched or unwritable register
  if (!for_each_register([&precheck, &registers](ServerRegister *server_register, uint16_t register_offset) -> bool {
        if (server_register->write_lambda == nullptr) {
          return false;  // unwritable -> ILLEGAL_DATA_ADDRESS
        }
        bool error = false;
        registers_to_number(registers.data() + register_offset, registers.size() - register_offset,
                            server_register->value_type, &error);
        if (error) {
          precheck = ModbusExceptionCode::ILLEGAL_DATA_VALUE;  // request doesn't supply the full value
          return false;
        }
        return true;
      })) {
    ESP_LOGW(TAG, "Write request rejected before applying any register. Sending exception response.");
    return precheck;
  }

  // Commit: every value is known writable and decodable, so the only failure now is a user write callback
  // rejecting the value at runtime -- which cannot be rolled back.
  if (!for_each_register([&registers](ServerRegister *server_register, uint16_t register_offset) {
        int64_t number = registers_to_number(registers.data() + register_offset, registers.size() - register_offset,
                                             server_register->value_type);
        return server_register->write_lambda(number);
      })) {
    ESP_LOGW(TAG, "A register write callback failed mid-sequence; earlier writes were already applied.");
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
