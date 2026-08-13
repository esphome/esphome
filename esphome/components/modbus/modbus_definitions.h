#pragma once

#include <algorithm>
#include <cstdint>
#include <span>

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

namespace esphome::modbus {

/// Modbus definitions from specs:
/// https://modbus.org/docs/Modbus_Application_Protocol_V1_1b3.pdf
// 5 Function Code Categories
const uint8_t FUNCTION_CODE_USER_DEFINED_SPACE_1_INIT = 65;  // 0x41
const uint8_t FUNCTION_CODE_USER_DEFINED_SPACE_1_END = 72;   // 0x48

const uint8_t FUNCTION_CODE_USER_DEFINED_SPACE_2_INIT = 100;  // 0x64
const uint8_t FUNCTION_CODE_USER_DEFINED_SPACE_2_END = 110;   // 0x6E

enum class FunctionCode : uint8_t {
  INVALID = 0x00,  // 0x00 is not a valid function code (even for custom functions).
  CUSTOM = 0x00,   // The CUSTOM alias should be removed in future.
  READ_COILS = 0x01,
  READ_DISCRETE_INPUTS = 0x02,
  READ_HOLDING_REGISTERS = 0x03,
  READ_INPUT_REGISTERS = 0x04,
  WRITE_SINGLE_COIL = 0x05,
  WRITE_SINGLE_REGISTER = 0x06,
  READ_EXCEPTION_STATUS = 0x07,   // not implemented
  DIAGNOSTICS = 0x08,             // not implemented
  GET_COMM_EVENT_COUNTER = 0x0B,  // not implemented
  GET_COMM_EVENT_LOG = 0x0C,      // not implemented
  WRITE_MULTIPLE_COILS = 0x0F,
  WRITE_MULTIPLE_REGISTERS = 0x10,
  REPORT_SERVER_ID = 0x11,     // not implemented
  READ_FILE_RECORD = 0x14,     // not implemented
  WRITE_FILE_RECORD = 0x15,    // not implemented
  MASK_WRITE_REGISTER = 0x16,  // not implemented
  READ_WRITE_MULTIPLE_REGISTERS = 0x17,
  READ_FIFO_QUEUE = 0x18,  // not implemented
};

// Remove before 2027.2.0
using ModbusFunctionCode ESPDEPRECATED("Use modbus::FunctionCode instead. Removed in 2027.2.0",
                                       "2026.8.0") = FunctionCode;

/*Allow direct comparison operators between FunctionCode and uint8_t*/
inline bool operator==(FunctionCode lhs, uint8_t rhs) { return static_cast<uint8_t>(lhs) == rhs; }
inline bool operator==(uint8_t lhs, FunctionCode rhs) { return lhs == static_cast<uint8_t>(rhs); }
inline bool operator!=(FunctionCode lhs, uint8_t rhs) { return !(static_cast<uint8_t>(lhs) == rhs); }
inline bool operator!=(uint8_t lhs, FunctionCode rhs) { return !(lhs == static_cast<uint8_t>(rhs)); }

// 4.3 MODBUS Data model. "Entity" is the spec's umbrella for the four primary tables; only the
// 16-bit tables are registers (coils and discrete inputs are bits), so the enum is not named
// RegisterType.
enum class EntityType : uint8_t {
  CUSTOM = 0x00,
  COIL = 0x01,
  DISCRETE_INPUT = 0x02,
  HOLDING = 0x03,
  // Named INPUT_REGISTER (not INPUT) because Arduino cores define INPUT as a macro.
  INPUT_REGISTER = 0x04,
  // Remove before 2027.2.0
  READ ESPDEPRECATED("Use EntityType::INPUT_REGISTER instead. Removed in 2027.2.0", "2026.7.0") = INPUT_REGISTER,
};

// Remove before 2027.2.0
using ModbusRegisterType ESPDEPRECATED("Use modbus::EntityType instead. Removed in 2027.2.0", "2026.8.0") = EntityType;

// 7 MODBUS Exception Responses:
const uint8_t FUNCTION_CODE_MASK = 0x7F;
const uint8_t FUNCTION_CODE_EXCEPTION_MASK = 0x80;

enum class ExceptionCode : uint8_t {
  ILLEGAL_FUNCTION = 0x01,
  ILLEGAL_DATA_ADDRESS = 0x02,
  ILLEGAL_DATA_VALUE = 0x03,
  SERVICE_DEVICE_FAILURE = 0x04,
  ACKNOWLEDGE = 0x05,
  SERVER_DEVICE_BUSY = 0x06,
  MEMORY_PARITY_ERROR = 0x08,
  GATEWAY_PATH_UNAVAILABLE = 0x0A,
  GATEWAY_TARGET_DEVICE_FAILED_TO_RESPOND = 0x0B,
};

// Remove before 2027.2.0
using ModbusExceptionCode ESPDEPRECATED("Use modbus::ExceptionCode instead. Removed in 2027.2.0",
                                        "2026.8.0") = ExceptionCode;

// 6.11 15 (0x0F) Write Multiple Coils
static constexpr uint16_t MAX_NUM_OF_COILS_TO_WRITE = 1968;  // 0x7B0

// 6.12 16 (0x10) Write Multiple registers:
static constexpr uint16_t MAX_NUM_OF_REGISTERS_TO_WRITE = 123;  // 0x7B

// 6.17 23 (0x17) Read/Write Multiple Registers:
static constexpr uint16_t MAX_NUM_OF_REGISTERS_TO_WRITE_RW = 121;  // 0x79

// 6.1 01 (0x01) Read Coils
// 6.2 02 (0x02) Read Discrete Inputs
static constexpr uint16_t MAX_NUM_OF_COILS_TO_READ = 2000;            // 0x7D0
static constexpr uint16_t MAX_NUM_OF_DISCRETE_INPUTS_TO_READ = 2000;  // 0x7D0

// 6.3 03 (0x03) Read Holding Registers
// 6.4 04 (0x04) Read Input Registers
static constexpr uint16_t MAX_NUM_OF_REGISTERS_TO_READ = 125;  // 0x7D

// Smallest possible frame is 4 bytes (custom function with no data): address(1) + function(1) + CRC(2)
static constexpr uint16_t MIN_FRAME_SIZE = 4;
static constexpr uint16_t MIN_PDU_SIZE = 1;
static constexpr uint16_t MAX_PDU_SIZE = 253;  // Max PDU size is 256 - address(1) - CRC(2) = 253
static constexpr uint16_t MAX_RAW_SIZE = 254;  // Max RAW size is 256 - CRC(2) = 254
// A read request PDU is always function code(1) + start address(2) + quantity(2)
static constexpr uint16_t READ_PDU_SIZE = 5;
// A single-write PDU is always function code(1) + address(2) + value(2)
static constexpr uint16_t WRITE_SINGLE_PDU_SIZE = 5;
static constexpr uint16_t MAX_FRAME_SIZE = 256;

// 4.1 Address 0 is the broadcast address: the request is processed by every device and never answered.
static constexpr uint8_t BROADCAST_ADDRESS = 0;

// Both send paths bound their payload so the framed result lands exactly on the RTU limit: a client
// PDU gains an address byte and a CRC, a raw server frame gains a CRC. send_frame_() therefore never
// has to check the framed size - it cannot be exceeded.
static_assert(MAX_PDU_SIZE + 3 == MAX_FRAME_SIZE, "a framed client PDU must fill the RTU frame limit");
static_assert(MAX_RAW_SIZE + 2 == MAX_FRAME_SIZE, "a framed raw server payload must fill the RTU frame limit");
/// Bits pack 8 per data byte, rounded up to whole bytes.
constexpr size_t packed_bit_bytes(size_t bits) { return (bits + 7) / 8; }

// A coil/discrete-input read answers with byte count(1) + packed_bit_bytes(count) bytes, which has to fit
// the raw frame body. The runtime check on that path catches a caller entering with bytes already written;
// this catches the other way in, raising the ceiling past what a frame can carry.
static_assert(1 + packed_bit_bytes(MAX_NUM_OF_COILS_TO_READ) <= MAX_RAW_SIZE,
              "MAX_NUM_OF_COILS_TO_READ yields a read response larger than MAX_RAW_SIZE");
static_assert(1 + packed_bit_bytes(MAX_NUM_OF_DISCRETE_INPUTS_TO_READ) <= MAX_RAW_SIZE,
              "MAX_NUM_OF_DISCRETE_INPUTS_TO_READ yields a read response larger than MAX_RAW_SIZE");

// The coil and discrete-input ceilings are separate limits in the spec but hold the same value, so the
// read paths validate both against MAX_NUM_OF_COILS_TO_READ. Should the spec ever split them, this fires.
static_assert(MAX_NUM_OF_COILS_TO_READ == MAX_NUM_OF_DISCRETE_INPUTS_TO_READ,
              "the coil and discrete-input read ceilings must match");

/** Read-only view of Modbus-packed bits: bit 0 of byte 0 is the first bit (LSB first), the layout
 * coil/discrete-input values use on the wire. Bundles the bit count with the packed bytes so the
 * two cannot desynchronize. The view does not own the bytes - it is only valid while they are.
 * Reads (operator[]) are unchecked by design - the caller owns the bit < size() precondition, as
 * with any subscript. Writes and forwarding are defensive: set() drops out-of-range bits and
 * bytes() clamps to the real span, because those paths touch buffers and the wire directly.
 */
class PackedBits {
 public:
  PackedBits(std::span<const uint8_t> data, uint16_t count) : data_(data), count_(count) {}
  /// Value of the given bit; bit must be < size().
  bool operator[](size_t bit) const { return (this->data_[bit / 8] & (1 << (bit % 8))) != 0; }
  /// Number of bits in the view.
  uint16_t size() const { return this->count_; }
  /// The underlying packed bytes: exactly ceil(size() / 8) bytes, even when the view was constructed
  /// over a larger buffer - forwarding this span onto the wire can never leak trailing buffer content.
  /// Clamped to the actual span so a view over a too-short buffer stays detectable instead of UB.
  std::span<const uint8_t> bytes() const {
    return this->data_.first(std::min<size_t>(packed_bit_bytes(this->count_), this->data_.size()));
  }

 private:
  std::span<const uint8_t> data_;  // must cover ceil(count_ / 8) bytes
  uint16_t count_;
};

/** Mutable counterpart of PackedBits: set() writes bits in place (deliberately no proxy operator[]=).
 * Converts implicitly to PackedBits for read access.
 */
class MutablePackedBits {
 public:
  MutablePackedBits(std::span<uint8_t> data, uint16_t count) : data_(data), count_(count) {}
  bool operator[](size_t bit) const { return (this->data_[bit / 8] & (1 << (bit % 8))) != 0; }
  /// Set or clear the given bit. Out-of-range bits are dropped: on the server read path the span wraps a
  /// stack response buffer, so a handler looping past size() must not be able to smash the frame.
  void set(size_t bit, bool value) {
    if (bit >= this->count_ || bit / 8 >= this->data_.size())
      return;
    if (value) {
      this->data_[bit / 8] |= (1 << (bit % 8));
    } else {
      this->data_[bit / 8] &= ~(1 << (bit % 8));
    }
  }
  uint16_t size() const { return this->count_; }
  operator PackedBits() const { return PackedBits(this->data_, this->count_); }

 private:
  std::span<uint8_t> data_;  // must cover ceil(count_ / 8) bytes
  uint16_t count_;
};

/// End of Modbus definitions
}  // namespace esphome::modbus
