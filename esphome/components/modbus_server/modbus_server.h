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

  // Formats a raw value into a string representation based on the value type for debugging
  std::string format_value(int64_t value) const {
    // max 44: float with %.1f can be up to 42 chars (3.4e38 → 39 integer digits + sign + decimal + 1 digit)
    // plus null terminator = 43, rounded to 44 for 4-byte alignment
    char buf[44];
    switch (this->value_type) {
      case SensorValueType::U_WORD:
      case SensorValueType::U_DWORD:
      case SensorValueType::U_DWORD_R:
      case SensorValueType::U_QWORD:
      case SensorValueType::U_QWORD_R:
        buf_append_printf(buf, sizeof(buf), 0, "%" PRIu64, static_cast<uint64_t>(value));
        return buf;
      case SensorValueType::S_WORD:
      case SensorValueType::S_DWORD:
      case SensorValueType::S_DWORD_R:
      case SensorValueType::S_QWORD:
      case SensorValueType::S_QWORD_R:
        buf_append_printf(buf, sizeof(buf), 0, "%" PRId64, value);
        return buf;
      case SensorValueType::FP32_R:
      case SensorValueType::FP32:
        buf_append_printf(buf, sizeof(buf), 0, "%.1f", bit_cast<float>(static_cast<uint32_t>(value)));
        return buf;
      default:
        buf_append_printf(buf, sizeof(buf), 0, "%" PRId64, value);
        return buf;
    }
  }

  uint16_t address{0};
  SensorValueType value_type{SensorValueType::RAW};
  uint8_t register_count{0};
  ReadLambda read_lambda;
  WriteLambda write_lambda;
};

class ModbusServer : public Component, public modbus::ModbusDevice {
 public:
  void dump_config() override;

  /// Not used for ModbusServer.
  void on_modbus_data(const std::vector<uint8_t> &data) override{};
  /// Registers a server register with the controller. Called by esphomes code generator
  void add_server_register(ServerRegister *server_register) { server_registers_.push_back(server_register); }
  /// called when a modbus request (function code 0x03 or 0x04) was parsed without errors
  void on_modbus_read_registers(uint8_t function_code, uint16_t start_address, uint16_t number_of_registers) final;
  /// called when a modbus request (function code 0x06 or 0x10) was parsed without errors
  void on_modbus_write_registers(uint8_t function_code, const std::vector<uint8_t> &data) final;
  /// Called by esphome generated code to set the server courtesy response object
  void set_server_courtesy_response(const ServerCourtesyResponse &server_courtesy_response) {
    this->server_courtesy_response_ = server_courtesy_response;
  }
  /// Get the server courtesy response object
  ServerCourtesyResponse get_server_courtesy_response() const { return this->server_courtesy_response_; }

 protected:
  /// Collection of all server registers for this component
  std::vector<ServerRegister *> server_registers_{};
  /// Server courtesy response
  ServerCourtesyResponse server_courtesy_response_{
      .enabled = false, .register_last_address = 0xFFFF, .register_value = 0};
};

}  // namespace esphome::modbus_server
