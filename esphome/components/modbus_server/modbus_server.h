#pragma once

#include "esphome/core/component.h"

#include "esphome/components/modbus/modbus.h"
#include "esphome/components/modbus/modbus_helpers.h"
#include "esphome/core/automation.h"

#include <utility>
#include <vector>

namespace esphome::modbus_server {

using modbus::helpers::SensorValueType;

struct ServerCourtesyResponse {
  bool enabled{false};
  uint16_t register_last_address{0xFFFF};
  uint16_t register_value{0};
};

class ServerRegister {
  using ReadLambda = std::function<int64_t()>;
  using WriteLambda = std::function<bool(int64_t value)>;

 public:
  ServerRegister(uint16_t address, SensorValueType value_type, uint8_t register_count) {
    this->address = address;
    this->value_type = value_type;
    this->register_count = register_count;
  }

  template<typename T> void set_read_lambda(const std::function<T(uint16_t address)> &&user_read_lambda) {
    this->read_lambda = [this, user_read_lambda]() -> int64_t {
      T user_value = user_read_lambda(this->address);
      if constexpr (std::is_same_v<T, float>) {
        return bit_cast<uint32_t>(user_value);
      } else {
        return static_cast<int64_t>(user_value);
      }
    };
  }

  template<typename T>
  void set_write_lambda(const std::function<bool(uint16_t address, const T v)> &&user_write_lambda) {
    this->write_lambda = [this, user_write_lambda](int64_t number) {
      if constexpr (std::is_same_v<T, float>) {
        float float_value = bit_cast<float>(static_cast<uint32_t>(number));
        return user_write_lambda(this->address, float_value);
      }
      return user_write_lambda(this->address, static_cast<T>(number));
    };
  }

  // max 44: float with %.1f can be up to 42 chars (3.4e38 → 39 integer digits + sign + decimal + 1 digit)
  // plus null terminator = 43, rounded to 44 for 4-byte alignment
  static constexpr size_t FORMAT_VALUE_BUF_SIZE = 44;

  // Formats a raw value into a caller-provided buffer based on the value type for debugging.
  // Returns buf for convenience.
  const char *format_value(int64_t value, char *buf, size_t buf_size) const {
    switch (this->value_type) {
      case SensorValueType::U_WORD:
      case SensorValueType::U_DWORD:
      case SensorValueType::U_DWORD_R:
      case SensorValueType::U_QWORD:
      case SensorValueType::U_QWORD_R:
        buf_append_printf(buf, buf_size, 0, "%" PRIu64, static_cast<uint64_t>(value));
        return buf;
      case SensorValueType::S_WORD:
      case SensorValueType::S_DWORD:
      case SensorValueType::S_DWORD_R:
      case SensorValueType::S_QWORD:
      case SensorValueType::S_QWORD_R:
        buf_append_printf(buf, buf_size, 0, "%" PRId64, value);
        return buf;
      case SensorValueType::FP32_R:
      case SensorValueType::FP32:
        buf_append_printf(buf, buf_size, 0, "%.1f", bit_cast<float>(static_cast<uint32_t>(value)));
        return buf;
      default:
        buf_append_printf(buf, buf_size, 0, "%" PRId64, value);
        return buf;
    }
  }

  void set_allow_partial_read(bool allow_partial_read) { this->allow_partial_read = allow_partial_read; }

  uint16_t address{0};
  SensorValueType value_type{SensorValueType::RAW};
  uint8_t register_count{0};
  // When true, a read may cover only part of this multi-register value; otherwise it must read the whole value.
  bool allow_partial_read{false};
  ReadLambda read_lambda;
  WriteLambda write_lambda;
};

class ModbusServer final : public Component, public modbus::ModbusServerDevice {
 public:
  void dump_config() override;

  /// Registers a server register with the controller. Called by esphomes code generator
  void add_server_register(ServerRegister *server_register) { server_registers_.push_back(server_register); }
  /// called when a modbus request (function code 0x03 or 0x04) was parsed without errors
  modbus::ResponseStatus on_read_registers(uint16_t start_address, uint16_t number_of_registers,
                                           modbus::RegisterValues &registers) final;
  /// called when a modbus request (function code 0x06 or 0x10) was parsed without errors
  modbus::ResponseStatus on_write_registers(uint16_t start_address, const modbus::RegisterValues &registers) final;
  /// Called by esphome generated code to set the server courtesy response object
  void set_server_courtesy_response(const ServerCourtesyResponse &server_courtesy_response) {
    this->server_courtesy_response_ = server_courtesy_response;
  }
  /// Get the server courtesy response object
  ServerCourtesyResponse get_server_courtesy_response() const { return this->server_courtesy_response_; }

 protected:
  /// Find the registered value whose register span contains address, or nullptr if none does.
  ServerRegister *find_containing_register_(uint32_t address) const;
  /// Collection of all server registers for this component
  std::vector<ServerRegister *> server_registers_{};
  /// Server courtesy response
  ServerCourtesyResponse server_courtesy_response_{
      .enabled = false, .register_last_address = 0xFFFF, .register_value = 0};
};

}  // namespace esphome::modbus_server
