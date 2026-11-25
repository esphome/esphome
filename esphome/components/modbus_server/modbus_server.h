#pragma once

#include "esphome/core/component.h"

#include "esphome/components/modbus/modbus.h"
#include "esphome/components/modbus/modbus_helpers.h"
#include "esphome/core/automation.h"
#include "esphome/core/helpers.h"

#include <utility>

namespace esphome::modbus_server {

using modbus::helpers::RegisterValueType;
using modbus::helpers::SensorValueType;

struct ServerCourtesyResponse {
  bool enabled{false};
  uint16_t register_last_address{0xFFFF};
  uint16_t register_value{0};
};

class ServerRegister final {
 public:
  template<typename T> using ReadFn = optional<T> (*)(uint16_t address);
  template<typename T> using WriteFn = bool (*)(uint16_t address, T value);

  ServerRegister(uint16_t address, SensorValueType value_type, uint8_t register_count)
      : address(address), value_type(value_type), register_count(register_count) {}

  /// VALUE_TYPE must be this register's value_type; read() and write() cast back by it.
  /// An empty optional declines the read, answering the whole request with SERVICE_DEVICE_FAILURE.
  template<SensorValueType VALUE_TYPE> void set_read_lambda(ReadFn<RegisterValueType<VALUE_TYPE>> read_fn) {
    this->read_fn_ = reinterpret_cast<ErasedFn>(read_fn);
  }
  template<SensorValueType VALUE_TYPE> void set_write_lambda(WriteFn<RegisterValueType<VALUE_TYPE>> write_fn) {
    this->write_fn_ = reinterpret_cast<ErasedFn>(write_fn);
  }

  bool is_readable() const { return this->read_fn_ != nullptr; }
  bool is_writable() const { return this->write_fn_ != nullptr; }
  /// Calls the read lambda and returns its value as a raw number (floats as their IEEE 754 bits).
  optional<int64_t> read() const;
  /// Converts a raw number (floats as their IEEE 754 bits) to the register's type and calls the write lambda.
  bool write(int64_t number) const;

  // max 44: float with %.1f can be up to 42 chars (3.4e38 → 39 integer digits + sign + decimal + 1 digit)
  // plus null terminator = 43, rounded to 44 for 4-byte alignment
  static constexpr size_t FORMAT_VALUE_BUF_SIZE = 44;

  // Formats a raw value as verbose log text for the value type, into a caller-provided buffer.
  // Returns buf for convenience.
  const char *format_value(int64_t value, char *buf, size_t buf_size) const;

  void set_allow_partial_read(bool allow_partial_read) { this->allow_partial_read = allow_partial_read; }

  uint16_t address{0};
  SensorValueType value_type{SensorValueType::RAW};
  uint8_t register_count{0};
  // When true, a read may cover only part of this multi-register value; otherwise it must read the whole value.
  bool allow_partial_read{false};

 private:
  using ErasedFn = void (*)();
  // Typed by value_type, which is why these are private.
  ErasedFn read_fn_{nullptr};
  ErasedFn write_fn_{nullptr};
};

/// A single bit in the server's coil/discrete-input table. Coils (0x01/0x05/0x0F) and discrete
/// inputs (0x02) share one bit address space, mirroring how holding and input registers share the
/// register table: both read function codes are served from the same bits.
class ServerBit final {
 public:
  /// Returning an empty optional declines the read: the whole request is answered with a
  /// SERVICE_DEVICE_FAILURE exception. `return true;`/`return false;` convert implicitly.
  using ReadFn = optional<bool> (*)(uint16_t address);
  using WriteFn = bool (*)(uint16_t address, bool value);

  explicit ServerBit(uint16_t address) : address(address) {}
  void set_read_lambda(ReadFn read_fn) { this->read_lambda = read_fn; }
  void set_write_lambda(WriteFn write_fn) { this->write_lambda = write_fn; }

  ReadFn read_lambda{nullptr};
  WriteFn write_lambda{nullptr};
  uint16_t address{0};
};

class ModbusServer final : public Component, public modbus::ModbusServerDevice {
 public:
  void dump_config() override;

  SLOT_LIST(ServerRegister, server_registers_, add_server_register, MODBUS_SERVER_REGISTER_COUNT)
  SLOT_LIST(ServerBit, server_bits_, add_server_bit, MODBUS_SERVER_BIT_COUNT)
  /// called when a modbus request (function code 0x03 or 0x04) was parsed without errors
  modbus::ResponseStatus on_read_registers(uint16_t start_address, uint16_t number_of_registers,
                                           modbus::RegisterValues &registers) final;
  /// called when a modbus request (function code 0x06 or 0x10) was parsed without errors
  modbus::ResponseStatus on_write_registers(uint16_t start_address, const modbus::RegisterValues &registers) final;
  /// called when a modbus request (function code 0x01 or 0x02) was parsed without errors; both are
  /// served from the same bit table (see ServerBit)
  modbus::ResponseStatus on_read_bits(uint16_t start_address, modbus::MutablePackedBits bits) final;
  /// called when a modbus request (function code 0x05 or 0x0F) was parsed without errors
  modbus::ResponseStatus on_write_coils(uint16_t start_address, modbus::PackedBits bits) final;
  /// Called by esphome generated code to set the server courtesy response object
  void set_server_courtesy_response(const ServerCourtesyResponse &server_courtesy_response) {
    this->server_courtesy_response_ = server_courtesy_response;
  }
  /// Get the server courtesy response object
  ServerCourtesyResponse get_server_courtesy_response() const { return this->server_courtesy_response_; }

 protected:
  /// Find the registered value whose register span contains address, or nullptr if none does.
  ServerRegister *find_containing_register_(uint32_t address) const;
  /// Find the registered bit at address, or nullptr if none is.
  ServerBit *find_bit_(uint16_t address) const;
  /// Server courtesy response
  ServerCourtesyResponse server_courtesy_response_{
      .enabled = false, .register_last_address = 0xFFFF, .register_value = 0};
};

}  // namespace esphome::modbus_server
