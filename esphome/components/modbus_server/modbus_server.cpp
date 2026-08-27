#include "modbus_server.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome::modbus_server {
using modbus::ExceptionCode;
using modbus::helpers::registers_to_number;

static const char *const TAG = "modbus_server";

// The widest Modbus value type (QWORD) spans four registers.
static constexpr uint8_t MAX_REGISTERS_PER_VALUE = 4;
// number_to_payload() encodes the 64-bit value returned by read_lambda() into 16-bit registers, so the
// widest possible value spans exactly sizeof(int64_t) / sizeof(uint16_t) registers. Tie the bound to that
// source so a future wider value type -- which would require widening the encoded value itself -- can't
// silently overflow the value_words buffer below (StaticVector::push_back drops words past capacity).
static_assert(MAX_REGISTERS_PER_VALUE == sizeof(int64_t) / sizeof(uint16_t),
              "MAX_REGISTERS_PER_VALUE must match the register span of the widest encodable value");

ServerRegister *ModbusServer::find_containing_register_(uint32_t address) const {
  for (auto *server_register : this->server_registers_) {
    if (address >= server_register->address &&
        address < static_cast<uint32_t>(server_register->address) + server_register->register_count) {
      return server_register;
    }
  }
  return nullptr;
}

modbus::ResponseStatus ModbusServer::on_read_registers(uint16_t start_address, uint16_t number_of_registers,
                                                       modbus::RegisterValues &registers) {
  ESP_LOGV(TAG,
           "Received read holding/input registers for device 0x%X. Start address: 0x%X. Number of registers: 0x%X.",
           this->address_, start_address, number_of_registers);

  // No registers configured (e.g. a bits-only server) and no courtesy default: this device does not implement
  // the register-read function, so answer ILLEGAL_FUNCTION. A populated map with a wrong address answers
  // ILLEGAL_DATA_ADDRESS below.
  if (this->server_registers_.empty() && !this->server_courtesy_response_.enabled)
    return ExceptionCode::ILLEGAL_FUNCTION;

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
      return ExceptionCode::ILLEGAL_DATA_ADDRESS;
    }

    if (!server_register->read_lambda) {
      // Registered but not readable (write-only); don't mask it with the courtesy default.
      ESP_LOGW(TAG, "Register at 0x%04X is not readable. Sending exception response.", server_register->address);
      return ExceptionCode::ILLEGAL_DATA_ADDRESS;
    }

    // A multi-register value is normally atomic: the request must start at its first register and cover all of
    // it. A value may opt in to partial reads, in which case the request may start inside it or stop short of
    // its end and we return only the covered words.
    const uint16_t value_offset = static_cast<uint16_t>(current_address - server_register->address);
    const uint16_t words_available = static_cast<uint16_t>(server_register->register_count - value_offset);
    const uint16_t words_wanted = static_cast<uint16_t>(end_address - current_address);
    const uint16_t take = words_available < words_wanted ? words_available : words_wanted;
    const bool clipped = value_offset != 0 || take != server_register->register_count;
    if (clipped && !server_register->allow_partial_read) {
      ESP_LOGW(TAG,
               "Read clips the multi-register value at 0x%04X, which does not allow partial reads. "
               "Sending exception response.",
               server_register->address);
      return ExceptionCode::ILLEGAL_DATA_ADDRESS;
    }

    const optional<int64_t> read_value = server_register->read_lambda();
    if (!read_value.has_value()) {
      ESP_LOGW(TAG, "Register read at 0x%04X declined to produce a value. Sending exception response.",
               server_register->address);
      return ExceptionCode::SERVICE_DEVICE_FAILURE;
    }
    const int64_t value = *read_value;
    char value_buf[ServerRegister::FORMAT_VALUE_BUF_SIZE];
    ESP_LOGV(TAG, "Matched register. Address: 0x%02X. Value type: %zu. Register count: %u. Value: %s.",
             server_register->address, static_cast<size_t>(server_register->value_type),
             server_register->register_count, server_register->format_value(value, value_buf, sizeof(value_buf)));

    // Encode the whole value once (wire word order) and emit only the covered words. Slicing the encoded words
    // handles the reversed value types for free, since number_to_payload already emits in wire order.
    StaticVector<uint16_t, MAX_REGISTERS_PER_VALUE> value_words;
    modbus::helpers::number_to_payload(value_words, value, server_register->value_type);
    if (value_offset + take > value_words.size()) {
      // The value encoded to fewer words than its register span (e.g. a RAW register); treat as a device fault.
      ESP_LOGE(TAG, "Register at 0x%04X did not encode to %u registers", server_register->address,
               server_register->register_count);
      return ExceptionCode::SERVICE_DEVICE_FAILURE;
    }
    for (uint16_t i = 0; i < take; i++) {
      registers.push_back(value_words[value_offset + i]);
    }
    current_address += take;
  }

  return {};
}

modbus::ResponseStatus ModbusServer::on_write_registers(uint16_t start_address,
                                                        const modbus::RegisterValues &registers) {
  // registers holds the values to write in host byte order; its size is the register count.
  ESP_LOGV(TAG, "Received write registers for device 0x%X. Start address: 0x%X. Number of registers: 0x%zX.",
           this->address_, start_address, registers.size());

  // No registers configured (e.g. a bits-only server): this device does not implement the register-write
  // function, so answer ILLEGAL_FUNCTION rather than ILLEGAL_DATA_ADDRESS.
  if (this->server_registers_.empty())
    return ExceptionCode::ILLEGAL_FUNCTION;

  auto for_each_register =
      [this, start_address,
       &registers](const std::function<bool(ServerRegister *, uint16_t register_offset)> &callback) -> bool {
    uint16_t register_offset = 0;
    for (uint32_t current_address = start_address; current_address < start_address + registers.size();) {
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
  ExceptionCode precheck = ExceptionCode::ILLEGAL_DATA_ADDRESS;  // unmatched or unwritable register
  if (!for_each_register([&precheck, &registers](ServerRegister *server_register, uint16_t register_offset) -> bool {
        if (server_register->write_lambda == nullptr) {
          return false;  // unwritable -> ILLEGAL_DATA_ADDRESS
        }
        if (!registers_to_number(registers.data() + register_offset, registers.size() - register_offset,
                                 server_register->value_type)
                 .has_value()) {
          precheck = ExceptionCode::ILLEGAL_DATA_VALUE;  // request doesn't supply the full value
          return false;
        }
        return true;
      })) {
    // Only VERBOSE: one handler serves both addressed and broadcast writes, and rejecting a broadcast for
    // registers this device does not map is routine. The hub logs the outcome with the context it has.
    ESP_LOGV(TAG, "Write request rejected before applying any register.");
    return precheck;
  }

  // Commit: every value is known writable and decodable, so the only failure now is a user write callback
  // rejecting the value at runtime -- which cannot be rolled back.
  if (!for_each_register([&registers](ServerRegister *server_register, uint16_t register_offset) {
        int64_t number = registers_to_number(registers.data() + register_offset, registers.size() - register_offset,
                                             server_register->value_type)
                             .value_or(0);
        return server_register->write_lambda(number);
      })) {
    ESP_LOGW(TAG, "A register write callback failed mid-sequence; earlier writes were already applied.");
    return ExceptionCode::SERVICE_DEVICE_FAILURE;
  }

  // Success: the caller builds the write response (an echo of the request header).
  return {};
}

ServerBit *ModbusServer::find_bit_(uint16_t address) const {
  for (auto *server_bit : this->server_bits_) {
    if (server_bit->address == address) {
      return server_bit;
    }
  }
  return nullptr;
}

modbus::ResponseStatus ModbusServer::on_read_bits(uint16_t start_address, modbus::MutablePackedBits bits) {
  ESP_LOGV(TAG, "Received read coils/discrete inputs for device 0x%X. Start address: 0x%X. Count: 0x%X.",
           this->address_, start_address, bits.size());

  // No bits configured: this device does not implement the coil/discrete-input function, so answer
  // ILLEGAL_FUNCTION. A populated table with a wrong address answers ILLEGAL_DATA_ADDRESS below.
  if (this->server_bits_.empty())
    return ExceptionCode::ILLEGAL_FUNCTION;

  for (uint16_t i = 0; i < bits.size(); i++) {
    const uint16_t address = static_cast<uint16_t>(start_address + i);  // range pre-checked by the hub
    ServerBit *server_bit = this->find_bit_(address);
    if (server_bit == nullptr || !server_bit->read_lambda) {
      ESP_LOGW(TAG, "No readable bit at 0x%04X. Sending exception response.", address);
      return ExceptionCode::ILLEGAL_DATA_ADDRESS;
    }
    const optional<bool> value = server_bit->read_lambda(address);
    if (!value.has_value()) {
      ESP_LOGW(TAG, "Bit read at 0x%04X declined to produce a value. Sending exception response.", address);
      return ExceptionCode::SERVICE_DEVICE_FAILURE;
    }
    bits.set(i, *value);
  }
  return {};
}

modbus::ResponseStatus ModbusServer::on_write_coils(uint16_t start_address, modbus::PackedBits bits) {
  ESP_LOGV(TAG, "Received write coils for device 0x%X. Start address: 0x%X. Count: 0x%X.", this->address_,
           start_address, bits.size());

  // No bits configured: this device does not implement the coil function, so answer ILLEGAL_FUNCTION rather
  // than ILLEGAL_DATA_ADDRESS.
  if (this->server_bits_.empty())
    return ExceptionCode::ILLEGAL_FUNCTION;

  // Pre-flight: every targeted bit must exist and be writable, so we never apply a partial write
  // before discovering a problem (mirrors the register write's two passes).
  for (uint16_t i = 0; i < bits.size(); i++) {
    const uint16_t address = static_cast<uint16_t>(start_address + i);
    ServerBit *server_bit = this->find_bit_(address);
    if (server_bit == nullptr || !server_bit->write_lambda) {
      // Only VERBOSE: one handler serves both addressed and broadcast writes, and rejecting a broadcast for
      // bits this device does not map is routine. The hub logs the outcome with the context it has.
      ESP_LOGV(TAG, "No writable bit at 0x%04X; write request rejected before applying any bit.", address);
      return ExceptionCode::ILLEGAL_DATA_ADDRESS;
    }
  }

  // Commit: the pre-flight above proved every address resolves to a writable bit. Re-resolve here rather
  // than caching up to MAX_NUM_OF_COILS_TO_WRITE pointers (a per-request heap allocation), matching the
  // register write's two-pass shape -- but guard the pointer anyway, so a future change to the pre-flight
  // can never turn this into a silent null dereference. The only expected failure is a write callback
  // rejecting the value at runtime, which cannot be rolled back.
  for (uint16_t i = 0; i < bits.size(); i++) {
    const uint16_t address = static_cast<uint16_t>(start_address + i);
    ServerBit *server_bit = this->find_bit_(address);
    if (server_bit == nullptr || !server_bit->write_lambda) {
      ESP_LOGE(TAG, "Bit at 0x%04X unresolved between pre-flight and commit; aborting write.", address);
      return ExceptionCode::SERVICE_DEVICE_FAILURE;
    }
    if (!server_bit->write_lambda(address, bits[i])) {
      ESP_LOGW(TAG, "Bit write callback failed at 0x%04X mid-sequence; earlier writes were already applied.", address);
      return ExceptionCode::SERVICE_DEVICE_FAILURE;
    }
  }
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
  ESP_LOGCONFIG(TAG, "server bits");
  for (auto &b : this->server_bits_) {
    ESP_LOGCONFIG(TAG, "  Address=0x%04X readable=%s writable=%s", b->address, b->read_lambda ? "true" : "false",
                  b->write_lambda ? "true" : "false");
  }
#endif
}

}  // namespace esphome::modbus_server
