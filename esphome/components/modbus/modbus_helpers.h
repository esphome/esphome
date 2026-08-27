#pragma once

#include <cmath>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "esphome/core/helpers.h"
#include "esphome/components/modbus/modbus_definitions.h"

namespace esphome::modbus::helpers {

// Pure read codes (0x01-0x04): they only read, so they are idempotent and safe to retry.
inline bool is_function_code_read_only(uint8_t function_code) {
  FunctionCode masked_function_code = static_cast<FunctionCode>(function_code & FUNCTION_CODE_MASK);
  return masked_function_code == FunctionCode::READ_COILS ||
         masked_function_code == FunctionCode::READ_DISCRETE_INPUTS ||
         masked_function_code == FunctionCode::READ_HOLDING_REGISTERS ||
         masked_function_code == FunctionCode::READ_INPUT_REGISTERS;
}

// Codes whose response carries read-back data: the pure reads plus 0x17, which reads and writes at once.
inline bool is_function_code_read(uint8_t function_code) {
  return is_function_code_read_only(function_code) ||
         static_cast<FunctionCode>(function_code & FUNCTION_CODE_MASK) == FunctionCode::READ_WRITE_MULTIPLE_REGISTERS;
}

// Codes that mutate registers or coils: the pure writes, 0x16 mask-write, and 0x17 read/write multiple.
inline bool is_function_code_write(uint8_t function_code) {
  FunctionCode masked_function_code = static_cast<FunctionCode>(function_code & FUNCTION_CODE_MASK);
  return masked_function_code == FunctionCode::WRITE_SINGLE_COIL ||
         masked_function_code == FunctionCode::WRITE_SINGLE_REGISTER ||
         masked_function_code == FunctionCode::WRITE_MULTIPLE_COILS ||
         masked_function_code == FunctionCode::WRITE_MULTIPLE_REGISTERS ||
         masked_function_code == FunctionCode::MASK_WRITE_REGISTER ||
         masked_function_code == FunctionCode::READ_WRITE_MULTIPLE_REGISTERS;
}

// True if [start_address, start_address + count) fits within the 16-bit Modbus address space. The 32-bit
// promotion is the overflow guard - a 16-bit sum could wrap and pass.
inline bool address_range_fits(uint16_t start_address, size_t count) {
  return uint32_t(start_address) + count <= 0x10000u;
}

inline bool is_function_code_exception(uint8_t function_code) {
  return (static_cast<uint8_t>(function_code) & FUNCTION_CODE_EXCEPTION_MASK) != 0;
}

inline bool is_function_code_custom(uint8_t function_code) {
  uint8_t masked_function_code = function_code & FUNCTION_CODE_MASK;
  return (masked_function_code >= FUNCTION_CODE_USER_DEFINED_SPACE_1_INIT &&
          masked_function_code <= FUNCTION_CODE_USER_DEFINED_SPACE_1_END) ||
         (masked_function_code >= FUNCTION_CODE_USER_DEFINED_SPACE_2_INIT &&
          masked_function_code <= FUNCTION_CODE_USER_DEFINED_SPACE_2_END);
}

/// True for any function code whose frame length the parsers cannot predict - everything the
/// server_pdu_length()/client_pdu_length() switches fall through to `default` on (keep the case list
/// in step with those switches). Deliberately wider than is_function_code_custom(): the user-defined
/// ranges are unknown to the parser too, but so are the assigned-but-unimplemented codes
/// (READ_EXCEPTION_STATUS, DIAGNOSTICS, GET_COMM_EVENT_*, REPORT_SERVER_ID) and every unassigned value.
/// The 0x80 exception flag is masked off first, so a frame with it set classifies by its base code -
/// even though a spec exception reply has a known 2-byte PDU. That is deliberate, matching what
/// is_function_code_custom() has always done: some vendors use codes with the 0x80 bit set as ordinary
/// codes with longer payloads, so the response parser CRC-scans these rather than assuming the spec
/// length. For an intact spec exception the scan matches at its first candidate, so only a corrupt one
/// pays (recovery by timeout instead of an immediate CRC failure).
inline bool is_function_code_unknown_length(uint8_t function_code) {
  switch (static_cast<FunctionCode>(function_code & FUNCTION_CODE_MASK)) {
    case FunctionCode::READ_COILS:
    case FunctionCode::READ_DISCRETE_INPUTS:
    case FunctionCode::READ_HOLDING_REGISTERS:
    case FunctionCode::READ_INPUT_REGISTERS:
    case FunctionCode::WRITE_SINGLE_COIL:
    case FunctionCode::WRITE_SINGLE_REGISTER:
    case FunctionCode::WRITE_MULTIPLE_COILS:
    case FunctionCode::WRITE_MULTIPLE_REGISTERS:
    case FunctionCode::READ_FILE_RECORD:
    case FunctionCode::WRITE_FILE_RECORD:
    case FunctionCode::MASK_WRITE_REGISTER:
    case FunctionCode::READ_WRITE_MULTIPLE_REGISTERS:
    case FunctionCode::READ_FIFO_QUEUE:
      return false;
    default:
      return true;
  }
}

// Returns the expected length of a server response PDU based on the function code.
// If too few bytes have arrived to determine the length, returns the minimum length. `size` is the
// number of bytes available so far, which may exceed the eventual PDU (e.g. include the frame's CRC
// bytes): only fixed header positions are interpreted, so surplus bytes are never misread.
uint16_t server_pdu_length(const uint8_t *frame, size_t size);
// Frame counterpart: address(1) + PDU + CRC(2). Passes every received byte after the address through,
// so header fields (e.g. a byte count) are interpreted as soon as they arrive.
inline uint16_t server_frame_length(const uint8_t *frame, size_t size) {
  if (size < 2)
    return MIN_FRAME_SIZE;  // function code not received yet
  return server_pdu_length(frame + 1, size - 1) + 3;
}

// Returns the expected length of a client request PDU based on the function code.
// Same contract as server_pdu_length(): `size` is bytes available so far, may exceed the PDU.
uint16_t client_pdu_length(const uint8_t *frame, size_t size);
inline uint16_t client_frame_length(const uint8_t *frame, size_t size) {
  if (size < 2)
    return MIN_FRAME_SIZE;  // function code not received yet
  return client_pdu_length(frame + 1, size - 1) + 3;
}

// Returns true if pdu is a complete transaction whose shape is consistent with its function code.
// Unlike *_pdu_length(), `size` here is the exact PDU length: a size mismatch is non-conformant.
// Function codes with nothing variable to cross-check are validated by their fixed length alone: the
// single writes (except 0x05's value field, which must be 0x0000 or 0xFF00), mask-write and FIFO, and
// - deliberately - custom/unknown codes and exception responses, so a dispatcher can still route
// them by function code rather than reject them outright. Tests pin this contract.
bool is_server_pdu_standard(const uint8_t *pdu, size_t size);

// Client counterpart: additionally checks quantity bounds and address-range arithmetic per function code.
// The same acceptance rule applies to custom/unknown function codes.
bool is_client_pdu_standard(const uint8_t *pdu, size_t size);

// Remove before 2027.2.0
ESPDEPRECATED("Use server_pdu_payload() on the response PDU instead. Removed in 2027.2.0", "2026.8.0")
inline uint8_t server_frame_data_offset(const uint8_t *frame, size_t size) {
  if (size < 2)
    return 0;
  switch (static_cast<FunctionCode>(frame[1])) {
    case FunctionCode::READ_COILS:
    case FunctionCode::READ_DISCRETE_INPUTS:
    case FunctionCode::READ_HOLDING_REGISTERS:
    case FunctionCode::READ_INPUT_REGISTERS:
      return 3;  // address(1) + function(1) + byte count(1) + data + CRC(2)
    default:
      return 2;
  }
}

/** Returns the payload portion of a server response PDU: the bytes after the function code, and for the
 * read responses (0x01-0x04 and 0x17) also after the byte-count byte. Response 0x14 also carries a
 * byte-count byte, but that code is not implemented and its count byte is left in the payload. For
 * an exception PDU the payload is the exception code byte (the read check must not see the masked
 * function code, or an exception-of-read would classify as a read and return an empty span). Returns an
 * empty span if the PDU is too short.
 */
inline std::span<const uint8_t> server_pdu_payload(std::span<const uint8_t> pdu) {
  if (pdu.empty())
    return {};
  const size_t offset = (!is_function_code_exception(pdu[0]) && is_function_code_read(pdu[0])) ? 2 : 1;
  return pdu.size() > offset ? pdu.subspan(offset) : std::span<const uint8_t>();
}

inline uint8_t client_frame_data_offset(const uint8_t *, size_t) { return 2; }

enum class SensorValueType : uint8_t {
  RAW = 0x00,     // variable length
  U_WORD = 0x1,   // 1 Register unsigned
  U_DWORD = 0x2,  // 2 Registers unsigned
  S_WORD = 0x3,   // 1 Register signed
  S_DWORD = 0x4,  // 2 Registers signed
  BIT = 0x5,
  U_DWORD_R = 0x6,  // 2 Registers unsigned
  S_DWORD_R = 0x7,  // 2 Registers unsigned
  U_QWORD = 0x8,
  S_QWORD = 0x9,
  U_QWORD_R = 0xA,
  S_QWORD_R = 0xB,
  FP32 = 0xC,
  FP32_R = 0xD,
  U_WORD_S = 0xE,  // 1 Register unsigned, bytes swapped
  S_WORD_S = 0xF,  // 1 Register signed, bytes swapped
};

inline bool value_type_is_float(SensorValueType v) {
  return v == SensorValueType::FP32 || v == SensorValueType::FP32_R;
}

/// Coils and discrete inputs are the bit-addressed entity tables; the other types are 16-bit registers.
inline bool is_entity_type_binary(EntityType type) {
  return type == EntityType::COIL || type == EntityType::DISCRETE_INPUT;
}

inline FunctionCode modbus_register_read_function(EntityType reg_type) {
  switch (reg_type) {
    case EntityType::COIL:
      return FunctionCode::READ_COILS;
    case EntityType::DISCRETE_INPUT:
      return FunctionCode::READ_DISCRETE_INPUTS;
    case EntityType::HOLDING:
      return FunctionCode::READ_HOLDING_REGISTERS;
    case EntityType::INPUT_REGISTER:
      return FunctionCode::READ_INPUT_REGISTERS;
    default:
      return FunctionCode::INVALID;
  }
}

inline FunctionCode modbus_register_write_function(EntityType reg_type, bool multiple = false) {
  switch (reg_type) {
    case EntityType::COIL:
      return multiple ? FunctionCode::WRITE_MULTIPLE_COILS : FunctionCode::WRITE_SINGLE_COIL;
    case EntityType::HOLDING:
      return multiple ? FunctionCode::WRITE_MULTIPLE_REGISTERS : FunctionCode::WRITE_SINGLE_REGISTER;
    // These register types can't be written (per spec)
    case EntityType::INPUT_REGISTER:
    case EntityType::DISCRETE_INPUT:
    default:
      return FunctionCode::INVALID;
  }
}

inline uint8_t c_to_hex(char c) { return (c >= 'A') ? (c >= 'a') ? (c - 'a' + 10) : (c - 'A' + 10) : (c - '0'); }

/** Get a byte from a hex string
 *  byte_from_hex_str("1122", 1) returns uint_8 value 0x22 == 34
 *  byte_from_hex_str("1122", 0) returns 0x11
 * @param value string containing hex encoding
 * @param position  offset in bytes. Because each byte is encoded in 2 hex digits the position of the original byte in
 * the hex string is byte_pos * 2
 * @return byte value
 */
inline uint8_t byte_from_hex_str(const std::string &value, uint8_t pos) {
  if (value.length() < pos * 2 + 2)
    return 0;
  return (c_to_hex(value[pos * 2]) << 4) | c_to_hex(value[pos * 2 + 1]);
}

/** Get a word from a hex string
 * @param value string containing hex encoding
 * @param position  offset in bytes. Because each byte is encoded in 2 hex digits the position of the original byte in
 * the hex string is byte_pos * 2
 * @return word value
 */
inline uint16_t word_from_hex_str(const std::string &value, uint8_t pos) {
  return byte_from_hex_str(value, pos) << 8 | byte_from_hex_str(value, pos + 1);
}

/** Get a dword from a hex string
 * @param value string containing hex encoding
 * @param position  offset in bytes. Because each byte is encoded in 2 hex digits the position of the original byte in
 * the hex string is byte_pos * 2
 * @return dword value
 */
inline uint32_t dword_from_hex_str(const std::string &value, uint8_t pos) {
  return word_from_hex_str(value, pos) << 16 | word_from_hex_str(value, pos + 2);
}

/** Get a qword from a hex string
 * @param value string containing hex encoding
 * @param position  offset in bytes. Because each byte is encoded in 2 hex digits the position of the original byte in
 * the hex string is byte_pos * 2
 * @return qword value
 */
inline uint64_t qword_from_hex_str(const std::string &value, uint8_t pos) {
  return static_cast<uint64_t>(dword_from_hex_str(value, pos)) << 32 | dword_from_hex_str(value, pos + 4);
}

// Extract data from modbus response buffer
/** Extract data from modbus response buffer
 * @param T one of supported integer data types int_8,int_16,int_32,int_64
 * @param data modbus response buffer (uint8_t)
 * @param buffer_offset  offset in bytes.
 * @return value of type T extracted from buffer
 */
template<typename T> T get_data(const uint8_t *data, size_t buffer_offset) {
  if (sizeof(T) == sizeof(uint8_t)) {
    return T(data[buffer_offset]);
  }
  if (sizeof(T) == sizeof(uint16_t)) {
    return T((uint16_t(data[buffer_offset + 0]) << 8) | (uint16_t(data[buffer_offset + 1]) << 0));
  }
  if (sizeof(T) == sizeof(uint32_t)) {
    return static_cast<uint32_t>(get_data<uint16_t>(data, buffer_offset)) << 16 |
           static_cast<uint32_t>(get_data<uint16_t>(data, buffer_offset + 2));
  }
  if (sizeof(T) == sizeof(uint64_t)) {
    return static_cast<uint64_t>(get_data<uint32_t>(data, buffer_offset)) << 32 |
           (static_cast<uint64_t>(get_data<uint32_t>(data, buffer_offset + 4)));
  }
  static_assert(sizeof(T) == sizeof(uint8_t) || sizeof(T) == sizeof(uint16_t) || sizeof(T) == sizeof(uint32_t) ||
                    sizeof(T) == sizeof(uint64_t),
                "Unsupported type size in get_data; only 1, 2, 4, or 8-byte integer types are supported.");
  return T{};
}

template<typename T> T get_data(const std::vector<uint8_t> &data, size_t buffer_offset) {
  return get_data<T>(data.data(), buffer_offset);
}

/** Extract coil data from modbus response buffer
 * Responses for coil are packed into bytes .
 * coil 3 is bit 3 of the first response byte
 * coil 9 is bit 2 of the second response byte
 * @param coil number of the cil
 * @param data modbus response buffer (uint8_t)
 * @return content of coil register
 */
inline bool bit_from_packed(int bit, std::span<const uint8_t> data) {
  auto data_byte = bit / 8;
  return (data[data_byte] & (1 << (bit % 8))) > 0;
}

// Remove before 2027.2.0
ESPDEPRECATED("Use bit_from_packed() instead. Removed in 2027.2.0", "2026.8.0")
inline bool coil_from_vector(int coil, std::span<const uint8_t> data) { return bit_from_packed(coil, data); }

/** Append packed bytes (LSB first) for the given bits onto a growable byte container.
 * push_back-based so callers can build a payload incrementally (e.g. a std::vector<uint8_t>
 * with no fixed upper bound). A non-byte-aligned count appends n+1 bytes, the last holding
 * the remaining bits in its low positions.
 * @param out destination byte container exposing push_back(uint8_t)
 * @param bits container of bool exposing range-based iteration
 */
template<typename Out, typename Bits> void pack_bits(Out &out, const Bits &bits) {
  uint8_t byte = 0;
  uint8_t bit = 0;
  for (bool b : bits) {
    if (b)
      byte |= (1 << bit);
    if (++bit == 8) {
      out.push_back(byte);
      byte = 0;
      bit = 0;
    }
  }
  if (bit != 0)  // flush the final partial byte
    out.push_back(byte);
}

/** Extract bits from value and shift right according to the bitmask
 * if the bitmask is 0x00F0  we want the values frrom bit 5 - 8.
 * the result is then shifted right by the position if the first right set bit in the mask
 * Useful for modbus data where more than one value is packed in a 16 bit register
 * Example: on Epever the "Length of night" register 0x9065 encodes values of the whole night length of time as
 * D15 - D8 =  hour, D7 - D0 = minute
 * To get the hours use mask 0xFF00 and  0x00FF for the minute
 * @param data an integral value between 16 aand 32 bits,
 * @param bitmask the bitmask to apply
 */
template<typename N> N mask_and_shift_by_rightbit(N data, uint32_t mask) {
  auto result = (mask & data);
  if (result == 0 || mask == 0xFFFFFFFF) {
    return result;
  }
  for (size_t pos = 0; pos < sizeof(N) << 3; pos++) {
    if (pos < 32 && (mask & (1UL << pos)) != 0)
      return result >> pos;
  }
  return 0;
}

// Logs an error for an unsupported value type. Defined in the .cpp so logging stays out of headers.
void log_unsupported_value_type(SensorValueType value_type);

/** Append the Modbus register words for value to data.
 * Works with any container exposing push_back(uint16_t) (e.g. std::vector or StaticVector).
 */
template<typename Container> void number_to_payload(Container &data, int64_t value, SensorValueType value_type) {
  switch (value_type) {
    case SensorValueType::U_WORD:
    case SensorValueType::S_WORD:
      data.push_back(value & 0xFFFF);
      break;
    case SensorValueType::U_WORD_S:
    case SensorValueType::S_WORD_S:
      data.push_back(byteswap(static_cast<uint16_t>(value & 0xFFFF)));
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
      log_unsupported_value_type(value_type);
      break;
  }
}

/** Convert a raw response payload to a number.
 * @param data payload with the data to convert
 * @param size number of bytes available in data
 * @param sensor_value_type defines if 16/32/64 bits or FP32 is used
 * @param offset offset to the data in data
 * @param bitmask bitmask used for masking and shifting
 * @return 64-bit number of the payload
 */
std::optional<int64_t> payload_to_number(const uint8_t *data, size_t size, SensorValueType sensor_value_type,
                                         uint8_t offset, uint32_t bitmask);

/** Convert a response payload span to number; std::nullopt if the payload is too short. */
inline std::optional<int64_t> payload_to_number(std::span<const uint8_t> data, SensorValueType sensor_value_type,
                                                uint8_t offset, uint32_t bitmask) {
  return payload_to_number(data.data(), data.size(), sensor_value_type, offset, bitmask);
}

// Remove before 2027.2.0
ESPDEPRECATED("Use the std::span overload returning std::optional<int64_t> instead. Removed in 2027.2.0", "2026.8.0")
inline int64_t payload_to_number(const std::vector<uint8_t> &data, SensorValueType sensor_value_type, uint8_t offset,
                                 uint32_t bitmask) {
  // Released behavior: a too-short payload logs an error and decodes to 0.
  return payload_to_number(std::span<const uint8_t>(data), sensor_value_type, offset, bitmask).value_or(0);
}

/** Reconstruct a number from register words (host byte order). Inverse of number_to_payload.
 * Decodes the value at the start of the given span; advance the pointer to read successive values.
 * @param registers register values in host byte order
 * @param count number of registers available in registers
 * @param sensor_value_type defines if 16/32/64 bits or FP32 is used
 * @return 64-bit number of the registers
 */
std::optional<int64_t> registers_to_number(const uint16_t *registers, size_t count, SensorValueType sensor_value_type);

// Named PDU buffer types: the builders' storage strategy (currently stack-allocated StaticVector,
// right-sized per shape) can be swapped in one place without touching every signature.
using PduBuffer = StaticVector<uint8_t, MAX_PDU_SIZE>;
using ReadPdu = StaticVector<uint8_t, READ_PDU_SIZE>;
using WriteSinglePdu = StaticVector<uint8_t, WRITE_SINGLE_PDU_SIZE>;
/// Scratch space for packing coils into wire layout: one bit per coil, sized for the spec maximum.
using CoilPackBuffer = StaticVector<uint8_t, packed_bit_bytes(MAX_NUM_OF_COILS_TO_WRITE)>;

/** Create a modbus read request PDU.
 * @param function_code one of READ_COILS, READ_DISCRETE_INPUTS, READ_HOLDING_REGISTERS, READ_INPUT_REGISTERS
 * @param start_address coil/register/input starting address
 * @param number_of_entities number of coils/registers/inputs to read
 * @return PDU (function code + data, no address, no CRC); empty on invalid input
 */
ReadPdu create_read_pdu(FunctionCode function_code, uint16_t start_address, uint16_t number_of_entities);

/** Create a modbus client pdu for reading/writing single/multiple coils/register/inputs.
 * Generic entry point; prefer the direction- and type-specific builders (create_read_pdu(),
 * create_write_registers_pdu(), create_write_single_register_pdu(), create_write_coils_pdu(),
 * create_write_single_coil_pdu()) which bound their inputs per spec.
 * @param function_code the modbus function code to use. One of:
 * READ_COILS
 * READ_DISCRETE_INPUTS
 * READ_HOLDING_REGISTERS
 * READ_INPUT_REGISTERS
 * WRITE_SINGLE_COIL
 * WRITE_SINGLE_REGISTER
 * WRITE_MULTIPLE_COILS
 * WRITE_MULTIPLE_REGISTERS
 * @param start_address coil/register/input starting address
 * @param number_of_entities number of coils/registers/inputs to read/write
 * @param values optional payload bytes to write (nullptr for read commands)
 * @param values_len length of values array
 * @return PDU (function code + data, no address, no CRC)
 */
PduBuffer create_client_pdu(FunctionCode function_code, uint16_t start_address, uint16_t number_of_entities,
                            const uint8_t *values = nullptr, size_t values_len = 0);

/** Create modbus write multiple registers command
 *  Function 0x10 Write Multiple Registers
 * @param start_address modbus address of the first register to write
 * @param values register values to write; the register count is values.size() (at most
 *               MAX_NUM_OF_REGISTERS_TO_WRITE, an over-long set is rejected and an empty PDU is returned).
 *               Any contiguous uint16_t container converts (std::vector, std::array).
 * @return PDU (function code + data, no address, no CRC)
 */
PduBuffer create_write_registers_pdu(uint16_t start_address, std::span<const uint16_t> values);

/** Create modbus read/write multiple registers command
 *  Function 0x17 Read/Write Multiple Registers
 * Writes write_values then reads read_count registers in one transaction (write first, per Modbus 6.17);
 * the response carries only the read registers.
 * @param read_start_address modbus address of the first register to read back
 * @param read_count number of registers to read (at most MAX_NUM_OF_REGISTERS_TO_READ)
 * @param write_start_address modbus address of the first register to write
 * @param write_values register values to write; the register count is write_values.size() (at most
 *               MAX_NUM_OF_REGISTERS_TO_WRITE_RW). Any contiguous uint16_t container converts.
 * @return PDU (function code + data, no address, no CRC); an empty PDU on any out-of-range input
 */
PduBuffer create_read_write_multiple_registers_pdu(uint16_t read_start_address, uint16_t read_count,
                                                   uint16_t write_start_address,
                                                   std::span<const uint16_t> write_values);

/** Create modbus write single register command
 *  Function 0x06 Write Single Register
 * @param start_address modbus address of the register to write
 * @param value uint16_t value to write
 * @return PDU (function code + data, no address, no CRC)
 */
WriteSinglePdu create_write_single_register_pdu(uint16_t start_address, uint16_t value);

/** Create modbus write single coil command
 *  Function 0x05 Write Single Coil
 * @param address modbus address of the coil to write
 * @param value coil value to write
 * @return PDU (function code + data, no address, no CRC)
 */
WriteSinglePdu create_write_single_coil_pdu(uint16_t address, bool value);

/** Create modbus write multiple coils command
 *  Function 0x0F Write Multiple Coils
 * @param start_address modbus address of the first coil to write
 * @param values coil values to write; the coil count is values.size() (at most MAX_NUM_OF_COILS_TO_WRITE, an
 *               over-long set is rejected and an empty PDU is returned)
 * @return PDU (function code + data, no address, no CRC)
 */
PduBuffer create_write_coils_pdu(uint16_t start_address, std::span<const bool> values);

/** Create modbus write multiple coils command (function 0x0F) from a std::vector<bool>.
 * Prefer the span overload above whenever the coils are already in contiguous storage - a std::array<bool, N>
 * or any other contiguous bool container converts to it. This overload exists only because std::vector<bool>
 * is bit-packed and so cannot convert to a span; without it every caller holding one re-implements the packing.
 * @param start_address modbus address of the first coil to write
 * @param values coil values to write; the coil count is values.size() (at most MAX_NUM_OF_COILS_TO_WRITE, an
 *               over-long set is rejected and an empty PDU is returned)
 * @return PDU (function code + data, no address, no CRC)
 */
PduBuffer create_write_coils_pdu(uint16_t start_address, const std::vector<bool> &values);

/** Create modbus write multiple coils command (function 0x0F) from bits packed as on the wire.
 * @param start_address modbus address of the first coil to write
 * @param bits PackedBits view of the coils to write (at most MAX_NUM_OF_COILS_TO_WRITE); invalid
 *             input returns an empty PDU
 * @return PDU (function code + data, no address, no CRC)
 */
PduBuffer create_write_coils_pdu(uint16_t start_address, PackedBits bits);

/** Append a float converted to register words to any push_back container (heap-free with StaticVector).
 * @param data container the register words are appended to
 * @param value value to convert
 * @param value_type  defines if 16/32/64 bits or FP32 is used
 */
template<typename Container> void float_to_payload(Container &data, float value, SensorValueType value_type) {
  int64_t val;

  if (value_type_is_float(value_type)) {
    val = bit_cast<uint32_t>(value);
  } else {
    val = llroundf(value);
  }

  number_to_payload(data, val, value_type);
}

// Remove before 2027.2.0
ESPDEPRECATED("Use the container overload of float_to_payload() instead. Removed in 2027.2.0", "2026.8.0")
inline std::vector<uint16_t> float_to_payload(float value, SensorValueType value_type) {
  std::vector<uint16_t> data;
  float_to_payload(data, value, value_type);
  return data;
}

}  // namespace esphome::modbus::helpers
