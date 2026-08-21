#include "modbus_helpers.h"
#include "esphome/core/log.h"

#include <algorithm>

namespace esphome::modbus::helpers {

static const char *const TAG = "modbus_helpers";

// A quantity/address pair is standard when the quantity is non-zero, within the per-table maximum,
// and the range [start_address, start_address + quantity) stays inside the 16-bit address space.
// Non-logging twin of register_block_in_range(): the same three predicates for the parser side, taking a
// uint16_t quantity. register_block_in_range() is the builder-side variant that also logs which half failed.
static bool quantity_in_range(uint16_t start_address, uint16_t quantity, uint16_t max_quantity) {
  return quantity != 0 && quantity <= max_quantity && address_range_fits(start_address, quantity);
}

// The spec allows exactly ON (0xFF00) and OFF (0x0000) for a single-coil value, on the request and
// on its echoed response alike.
static bool is_canonical_coil_value(uint8_t high_byte, uint8_t low_byte) {
  return (high_byte == 0xFF || high_byte == 0x00) && low_byte == 0x00;
}

uint16_t server_pdu_length(const uint8_t *frame, size_t size) {
  if (size < MIN_PDU_SIZE)
    return MIN_PDU_SIZE;
  if (is_function_code_exception(frame[0])) {
    return 2;  // function(1) + exception(1)
  }
  switch (static_cast<FunctionCode>(frame[0])) {
    case FunctionCode::READ_COILS:
    case FunctionCode::READ_DISCRETE_INPUTS:
    case FunctionCode::READ_HOLDING_REGISTERS:
    case FunctionCode::READ_INPUT_REGISTERS:
      // function(1) + byte count(1) + data
      return 2 + (size > 1 ? std::min(frame[1], uint8_t(MAX_NUM_OF_REGISTERS_TO_READ * 2)) : 0);
    case FunctionCode::WRITE_SINGLE_COIL:
    case FunctionCode::WRITE_SINGLE_REGISTER:
    case FunctionCode::WRITE_MULTIPLE_COILS:
    case FunctionCode::WRITE_MULTIPLE_REGISTERS:
      return 5;  // function(1) + output/register address(2) + value(2)
    // Unsupported function codes. Included here to prevent parser failures. Excluding Serial Line specific functions.
    case FunctionCode::READ_FILE_RECORD:
    case FunctionCode::WRITE_FILE_RECORD:
      // function(1) + byte count(1) + data
      return 2 + (size > 1 ? std::min(frame[1], uint8_t(MAX_PDU_SIZE - 2)) : 0);
    case FunctionCode::MASK_WRITE_REGISTER:
      return 7;  // function(1) + reference address(2) + AND mask(2) + OR mask(2)
    case FunctionCode::READ_WRITE_MULTIPLE_REGISTERS:
      // function(1) + byte count(1) + data
      return 2 + (size > 1 ? std::min(frame[1], uint8_t(MAX_NUM_OF_REGISTERS_TO_READ * 2)) : 0);
    case FunctionCode::READ_FIFO_QUEUE:
      // function(1) + fifo address(2)
      return 3;
    default:
      return MIN_PDU_SIZE;  // unknown length
  }
}

uint16_t client_pdu_length(const uint8_t *frame, size_t size) {
  if (size < MIN_PDU_SIZE)
    return MIN_PDU_SIZE;
  switch (static_cast<FunctionCode>(frame[0])) {
    case FunctionCode::READ_COILS:
    case FunctionCode::READ_DISCRETE_INPUTS:
    case FunctionCode::READ_HOLDING_REGISTERS:
    case FunctionCode::READ_INPUT_REGISTERS:
      // function(1) + start address(2) + quantity(2)
    case FunctionCode::WRITE_SINGLE_COIL:
    case FunctionCode::WRITE_SINGLE_REGISTER:
      return 5;  // function(1) + output/register address(2) + value(2)
    case FunctionCode::WRITE_MULTIPLE_COILS:
      // function(1) + start address(2) + quantity(2) + byte count(1) + packed coil data (8 coils per byte).
      return 6 + (size > 5 ? std::min(frame[5], uint8_t(packed_bit_bytes(MAX_NUM_OF_COILS_TO_WRITE))) : 0);
    case FunctionCode::WRITE_MULTIPLE_REGISTERS:
      // function(1) + start address(2) + quantity(2) + byte count(1) + register data (2 bytes per register).
      return 6 + (size > 5 ? std::min(frame[5], uint8_t(MAX_NUM_OF_REGISTERS_TO_WRITE * 2)) : 0);
    // Unsupported function codes. Included here to prevent parser failures. Excluding Serial Line specific functions.
    case FunctionCode::READ_FILE_RECORD:
    case FunctionCode::WRITE_FILE_RECORD:
      // function(1) + byte count(1) + data
      return 2 + (size > 1 ? std::min(frame[1], uint8_t(MAX_PDU_SIZE - 2)) : 0);
    case FunctionCode::MASK_WRITE_REGISTER:
      return 7;  // function(1) + reference address(2) + AND mask(2) + OR mask(2)
    case FunctionCode::READ_WRITE_MULTIPLE_REGISTERS:
      // function(1) + read start address(2) + read quantity(2) + write start address(2) +
      // write quantity(2) + byte count(1) + data
      return 10 + (size > 9 ? std::min(frame[9], uint8_t(MAX_NUM_OF_REGISTERS_TO_WRITE_RW * 2)) : 0);
    case FunctionCode::READ_FIFO_QUEUE:
      // function(1) + fifo address(2)
      return 3;
    default:
      return MIN_PDU_SIZE;  // unknown length
  }
}

bool is_server_pdu_standard(const uint8_t *pdu, size_t size) {
  if (server_pdu_length(pdu, size) != size)
    return false;

  const auto function_code = static_cast<FunctionCode>(pdu[0]);
  switch (function_code) {
    case FunctionCode::READ_COILS:
    case FunctionCode::READ_DISCRETE_INPUTS:
      // A conformant bit-read response carries at least one packed byte (up to 2000 bits = 250 bytes).
      return pdu[1] != 0 && pdu[1] <= uint8_t(packed_bit_bytes(MAX_NUM_OF_COILS_TO_READ));
    case FunctionCode::READ_HOLDING_REGISTERS:
    case FunctionCode::READ_INPUT_REGISTERS:
      // Registers are 2 bytes each: the byte count must be a non-zero even count within the read maximum.
      return pdu[1] != 0 && pdu[1] % 2 == 0 && pdu[1] <= uint8_t(MAX_NUM_OF_REGISTERS_TO_READ * 2);
    case FunctionCode::READ_FILE_RECORD:
    case FunctionCode::WRITE_FILE_RECORD:
      return pdu[1] <= uint8_t(MAX_PDU_SIZE - 2);
    case FunctionCode::READ_WRITE_MULTIPLE_REGISTERS:
      return pdu[1] != 0 && pdu[1] % 2 == 0 && pdu[1] <= uint8_t(MAX_NUM_OF_REGISTERS_TO_READ * 2);
    case FunctionCode::WRITE_MULTIPLE_COILS:
    case FunctionCode::WRITE_MULTIPLE_REGISTERS: {
      // The response echoes start address and quantity: bound them like the request side does.
      const bool bits = function_code == FunctionCode::WRITE_MULTIPLE_COILS;
      const uint16_t start_address = get_data<uint16_t>(pdu, 1);
      const uint16_t quantity = get_data<uint16_t>(pdu, 3);
      const uint16_t max_quantity = bits ? MAX_NUM_OF_COILS_TO_WRITE : MAX_NUM_OF_REGISTERS_TO_WRITE;
      return quantity_in_range(start_address, quantity, max_quantity);
    }
    case FunctionCode::WRITE_SINGLE_COIL:
      // The response echoes the request, so the same ON/OFF constraint applies.
      return is_canonical_coil_value(pdu[3], pdu[4]);
    default:
      return true;  // All other function codes validated by length alone
  }
}

bool is_client_pdu_standard(const uint8_t *pdu, size_t size) {
  if (client_pdu_length(pdu, size) != size)
    return false;

  const auto function_code = static_cast<FunctionCode>(pdu[0]);
  switch (function_code) {
    case FunctionCode::READ_COILS:
    case FunctionCode::READ_DISCRETE_INPUTS:
    case FunctionCode::READ_HOLDING_REGISTERS:
    case FunctionCode::READ_INPUT_REGISTERS: {
      const bool bits =
          function_code == FunctionCode::READ_COILS || function_code == FunctionCode::READ_DISCRETE_INPUTS;
      const uint16_t start_address = get_data<uint16_t>(pdu, 1);
      const uint16_t quantity = get_data<uint16_t>(pdu, 3);
      const uint16_t max_quantity = bits ? MAX_NUM_OF_COILS_TO_READ : MAX_NUM_OF_REGISTERS_TO_READ;
      return quantity_in_range(start_address, quantity, max_quantity);
    }
    case FunctionCode::WRITE_MULTIPLE_COILS:
    case FunctionCode::WRITE_MULTIPLE_REGISTERS: {
      const bool bits = function_code == FunctionCode::WRITE_MULTIPLE_COILS;
      const uint16_t start_address = get_data<uint16_t>(pdu, 1);
      const uint16_t quantity = get_data<uint16_t>(pdu, 3);
      const uint16_t max_quantity = bits ? MAX_NUM_OF_COILS_TO_WRITE : MAX_NUM_OF_REGISTERS_TO_WRITE;
      // Coils are packed 8 per data byte; registers are 2 bytes each.
      const size_t expected_data_bytes = bits ? packed_bit_bytes(quantity) : quantity * 2;
      return quantity_in_range(start_address, quantity, max_quantity) && pdu[5] == expected_data_bytes;
    }
    case FunctionCode::READ_FILE_RECORD:
    case FunctionCode::WRITE_FILE_RECORD:
      return pdu[1] <= MAX_PDU_SIZE - 2;
    case FunctionCode::READ_WRITE_MULTIPLE_REGISTERS: {
      const uint16_t start_address_read = get_data<uint16_t>(pdu, 1);
      const uint16_t quantity_read = get_data<uint16_t>(pdu, 3);
      const uint16_t start_address_write = get_data<uint16_t>(pdu, 5);
      const uint16_t quantity_write = get_data<uint16_t>(pdu, 7);
      return quantity_in_range(start_address_read, quantity_read, MAX_NUM_OF_REGISTERS_TO_READ) &&
             quantity_in_range(start_address_write, quantity_write, MAX_NUM_OF_REGISTERS_TO_WRITE_RW) &&
             pdu[9] == quantity_write * 2;
    }
    case FunctionCode::WRITE_SINGLE_COIL:
      // The one variable field in an otherwise fixed-shape PDU: the spec allows exactly ON/OFF.
      return is_canonical_coil_value(pdu[3], pdu[4]);
    default:
      return true;  // All other function codes validated by length alone
  }
}

static size_t required_payload_size(SensorValueType sensor_value_type) {
  switch (sensor_value_type) {
    case SensorValueType::U_WORD:
    case SensorValueType::U_WORD_S:
    case SensorValueType::S_WORD:
    case SensorValueType::S_WORD_S:
      return 2;
    case SensorValueType::U_DWORD:
    case SensorValueType::FP32:
    case SensorValueType::U_DWORD_R:
    case SensorValueType::FP32_R:
    case SensorValueType::S_DWORD:
    case SensorValueType::S_DWORD_R:
      return 4;
    case SensorValueType::U_QWORD:
    case SensorValueType::S_QWORD:
    case SensorValueType::U_QWORD_R:
    case SensorValueType::S_QWORD_R:
      return 8;
    case SensorValueType::RAW:
    default:
      return 0;
  }
}

void log_unsupported_value_type(SensorValueType value_type) {
  ESP_LOGE(TAG, "Invalid data type for modbus number to payload conversion: %d", static_cast<uint16_t>(value_type));
}

std::optional<int64_t> payload_to_number(const uint8_t *data, size_t size, SensorValueType sensor_value_type,
                                         uint8_t offset, uint32_t bitmask) {
  int64_t value = 0;  // int64_t because it can hold signed and unsigned 32 bits

  // Validate offset against the buffer for all types, including RAW/unsupported, so
  // a malformed or misconfigured frame still produces an error log.
  if (static_cast<size_t>(offset) > size) {
    ESP_LOGE(TAG, "not enough data for value type=%u offset=%u size=%zu", static_cast<unsigned int>(sensor_value_type),
             static_cast<unsigned int>(offset), size);
    return std::nullopt;
  }

  const size_t required_size = required_payload_size(sensor_value_type);
  if (required_size == 0) {
    return value;
  }

  if (size - offset < required_size) {
    ESP_LOGE(TAG, "not enough data for value type=%u offset=%u size=%zu required=%zu",
             static_cast<unsigned int>(sensor_value_type), static_cast<unsigned int>(offset), size, required_size);
    return std::nullopt;
  }

  switch (sensor_value_type) {
    case SensorValueType::U_WORD:
      value = mask_and_shift_by_rightbit(get_data<uint16_t>(data, offset), bitmask);  // default is 0xFFFF ;
      break;
    case SensorValueType::U_WORD_S: {
      uint16_t word = byteswap(get_data<uint16_t>(data, offset));
      value = mask_and_shift_by_rightbit(word, bitmask);
      break;
    }
    case SensorValueType::U_DWORD:
    case SensorValueType::FP32:
      value = get_data<uint32_t>(data, offset);
      value = mask_and_shift_by_rightbit((uint32_t) value, bitmask);
      break;
    case SensorValueType::U_DWORD_R:
    case SensorValueType::FP32_R:
      value = get_data<uint32_t>(data, offset);
      value = static_cast<uint32_t>(value & 0xFFFF) << 16 | (value & 0xFFFF0000) >> 16;
      value = mask_and_shift_by_rightbit((uint32_t) value, bitmask);
      break;
    case SensorValueType::S_WORD:
      value = mask_and_shift_by_rightbit(get_data<int16_t>(data, offset), bitmask);  // default is 0xFFFF ;
      break;
    case SensorValueType::S_WORD_S: {
      uint16_t word = byteswap(get_data<uint16_t>(data, offset));
      value = mask_and_shift_by_rightbit(static_cast<int16_t>(word), bitmask);
      break;
    }
    case SensorValueType::S_DWORD:
      value = mask_and_shift_by_rightbit(get_data<int32_t>(data, offset), bitmask);
      break;
    case SensorValueType::S_DWORD_R: {
      value = get_data<uint32_t>(data, offset);
      // Currently the high word is at the low position
      // the sign bit is therefore at low before the switch
      uint32_t sign_bit = (value & 0x8000) << 16;
      value = mask_and_shift_by_rightbit(
          static_cast<int32_t>(((value & 0x7FFF) << 16 | (value & 0xFFFF0000) >> 16) | sign_bit), bitmask);
    } break;
    case SensorValueType::U_QWORD:
    case SensorValueType::S_QWORD:
      // Ignore bitmask for QWORD
      value = get_data<uint64_t>(data, offset);
      break;
    case SensorValueType::U_QWORD_R:
    case SensorValueType::S_QWORD_R: {
      // Ignore bitmask for QWORD
      uint64_t tmp = get_data<uint64_t>(data, offset);
      value = (tmp << 48) | (tmp >> 48) | ((tmp & 0xFFFF0000) << 16) | ((tmp >> 16) & 0xFFFF0000);
    } break;
    case SensorValueType::RAW:
    default:
      break;
  }
  return value;
}

std::optional<int64_t> registers_to_number(const uint16_t *registers, size_t count, SensorValueType sensor_value_type) {
  const size_t required_size = required_payload_size(sensor_value_type);
  if (required_size == 0) {
    return 0;  // RAW/unsupported: nothing to read
  }
  const size_t required_words = required_size / 2;
  if (required_words > count) {
    ESP_LOGE(TAG, "not enough registers for value type=%u count=%zu required=%zu",
             static_cast<unsigned int>(sensor_value_type), count, required_words);
    return std::nullopt;
  }
  // Serialize the needed words back to big-endian bytes and reuse the audited byte decoder so the
  // sign-extension behaviour stays identical to the wire path.
  uint8_t bytes[8];  // at most 4 registers (QWORD)
  for (size_t i = 0; i < required_words; i++) {
    uint16_t reg = registers[i];
    bytes[i * 2] = static_cast<uint8_t>(reg >> 8);
    bytes[i * 2 + 1] = static_cast<uint8_t>(reg & 0xFF);
  }
  return payload_to_number(bytes, required_size, sensor_value_type, 0, 0xFFFFFFFF);
}

// Append a 16-bit value to a PDU in big-endian (wire) byte order.
template<size_t CAP> static void append_pdu_word(StaticVector<uint8_t, CAP> &pdu, uint16_t value) {
  pdu.push_back(value >> 8);
  pdu.push_back(value >> 0);
}

// Every request PDU opens with the same 5-byte layout: function code, then two big-endian 16-bit
// fields (start address + quantity for reads and multi-writes, address + value for single writes).
template<size_t CAP>
static void append_pdu_header(StaticVector<uint8_t, CAP> &pdu, FunctionCode function_code, uint16_t first,
                              uint16_t second) {
  pdu.push_back(static_cast<uint8_t>(function_code));
  append_pdu_word(pdu, first);
  append_pdu_word(pdu, second);
}

// Zero the unused bits of a multi-coil write's final data byte, as the spec requires. Kept in one
// place so the generic and typed coil builders produce identical wire bytes for the same write.
// The caller must pass a span whose LAST byte is the final packed-bit byte - both builders pass the
// whole PDU, which qualifies because the coil data is always the PDU's tail.
static void mask_trailing_pad_bits(std::span<uint8_t> data, uint16_t bit_count) {
  if (data.empty() || bit_count % 8 == 0)
    return;
  data.back() &= static_cast<uint8_t>((1 << (bit_count % 8)) - 1);
}

ReadPdu create_read_pdu(FunctionCode function_code, uint16_t start_address, uint16_t number_of_entities) {
  ReadPdu pdu;  // declared before every return so NRVO fires (all paths return the same object)
  if (number_of_entities == 0) {
    ESP_LOGE(TAG, "Number of entities is zero for function code %02X", static_cast<uint8_t>(function_code));
    return pdu;
  }
  if (!address_range_fits(start_address, number_of_entities)) {
    ESP_LOGE(TAG, "Read of %u entities at %u runs past the 16-bit address space, dropping request", number_of_entities,
             start_address);
    return pdu;
  }

  switch (function_code) {
    case FunctionCode::READ_COILS:
      if (number_of_entities > MAX_NUM_OF_COILS_TO_READ) {
        ESP_LOGE(TAG, "number_of_entities %u exceeds maximum coils to read %u for function code %02X",
                 number_of_entities, MAX_NUM_OF_COILS_TO_READ, static_cast<uint8_t>(function_code));
        return pdu;
      }
      break;
    case FunctionCode::READ_DISCRETE_INPUTS:
      if (number_of_entities > MAX_NUM_OF_DISCRETE_INPUTS_TO_READ) {
        ESP_LOGE(TAG, "number_of_entities %u exceeds maximum discrete inputs to read %u for function code %02X",
                 number_of_entities, MAX_NUM_OF_DISCRETE_INPUTS_TO_READ, static_cast<uint8_t>(function_code));
        return pdu;
      }
      break;
    case FunctionCode::READ_HOLDING_REGISTERS:
    case FunctionCode::READ_INPUT_REGISTERS:
      if (number_of_entities > MAX_NUM_OF_REGISTERS_TO_READ) {
        ESP_LOGE(TAG, "number_of_entities %u exceeds maximum registers to read %u for function code %02X",
                 number_of_entities, MAX_NUM_OF_REGISTERS_TO_READ, static_cast<uint8_t>(function_code));
        return pdu;
      }
      break;
    default:
      ESP_LOGE(TAG, "Unsupported function code %02X for read PDU creation", static_cast<uint8_t>(function_code));
      return pdu;
  }

  append_pdu_header(pdu, function_code, start_address, number_of_entities);
  return pdu;
}

PduBuffer create_client_pdu(FunctionCode function_code, uint16_t start_address, uint16_t number_of_entities,
                            const uint8_t *values, size_t values_len) {
  PduBuffer pdu;  // declared before every return so NRVO fires (all paths return the same object)
  // Generic entry point; prefer the direction- and type-specific builders (create_read_pdu(),
  // create_write_registers_pdu(), etc.) which bound their inputs per spec.
  if (is_function_code_read_only(static_cast<uint8_t>(function_code))) {
    if (values != nullptr || values_len > 0) {
      ESP_LOGW(TAG, "Values provided for read function code %02X, but will be ignored",
               static_cast<uint8_t>(function_code));
    }
    auto read_pdu = create_read_pdu(function_code, start_address, number_of_entities);
    pdu.assign(read_pdu.begin(), read_pdu.end());
    return pdu;
  }
  // Exact codes only: is_function_code_write() masks the exception bit, which would let the
  // exception-flagged forms (0x85/0x86/0x8F/0x90) build a request announcing itself as an exception.
  const bool is_single =
      function_code == FunctionCode::WRITE_SINGLE_COIL || function_code == FunctionCode::WRITE_SINGLE_REGISTER;
  const bool is_multi =
      function_code == FunctionCode::WRITE_MULTIPLE_COILS || function_code == FunctionCode::WRITE_MULTIPLE_REGISTERS;
  if (!is_single && !is_multi) {
    ESP_LOGE(TAG, "Unsupported function code %02X for client PDU creation", static_cast<uint8_t>(function_code));
    return pdu;
  }

  // Generic write builder: raw caller-supplied bytes, so we can only guard against the PDU byte capacity here.
  if (values == nullptr || values_len == 0) {
    ESP_LOGE(TAG, "No values provided for write function code %02X", static_cast<uint8_t>(function_code));
    return pdu;
  }
  if (number_of_entities == 0) {
    ESP_LOGE(TAG, "Number of entities is zero for function code %02X", static_cast<uint8_t>(function_code));
    return pdu;
  }
  // number_of_entities is ignored for single write, so only validate it for the multiple variants.
  // The bound is per function code (coils pack 8 per byte, so their quantity limit is far higher) -
  // the same limits is_client_pdu_standard() accepts, so builder and validator agree.
  const uint16_t max_entities =
      function_code == FunctionCode::WRITE_MULTIPLE_COILS ? MAX_NUM_OF_COILS_TO_WRITE : MAX_NUM_OF_REGISTERS_TO_WRITE;
  if (!is_single && number_of_entities > max_entities) {
    ESP_LOGE(TAG, "number_of_entities %u exceeds maximum %u for function code %02X", number_of_entities, max_entities,
             static_cast<uint8_t>(function_code));
    return pdu;
  }
  if (!is_single && !address_range_fits(start_address, number_of_entities)) {
    ESP_LOGE(TAG, "Write of %u entities at %u runs past the 16-bit address space, dropping request", number_of_entities,
             start_address);
    return pdu;
  }

  if (is_single) {
    // Write single register or coil: the two value bytes are the header's second field.
    if (values_len < 2) {
      ESP_LOGE(TAG, "values_len %zu too small for write-single command (need 2), dropping request", values_len);
      return pdu;
    }
    // The spec allows exactly ON (0xFF00) and OFF (0x0000) for a single-coil write - the same rule
    // is_client_pdu_standard() enforces, so a built frame cannot be misclassified on reply.
    if (function_code == FunctionCode::WRITE_SINGLE_COIL && !is_canonical_coil_value(values[0], values[1])) {
      ESP_LOGE(TAG, "Invalid single-coil value %02X%02X (must be FF00 or 0000), dropping request", values[0],
               values[1]);
      return pdu;
    }
    append_pdu_header(pdu, function_code, start_address, uint16_t((values[0] << 8) | values[1]));
    return pdu;
  }
  // The quantity is spec-bounded above, so the data length just has to agree with it exactly
  // (registers are 2 bytes each, coils pack 8 per byte). This is the same consistency the response
  // dispatch enforces via is_client_pdu_standard(), so a frame built here can never be classified
  // non-standard on reply, and the spec bound keeps the PDU within capacity by construction.
  // Checked before the header append: a failed check must return an empty PDU, not a 5-byte partial one.
  const bool bits = function_code == FunctionCode::WRITE_MULTIPLE_COILS;
  const size_t expected_len = bits ? packed_bit_bytes(number_of_entities) : static_cast<size_t>(number_of_entities) * 2;
  if (values_len != expected_len) {
    ESP_LOGE(TAG, "values_len %zu does not match %u entities (expected %zu) for function code %02X, dropping request",
             values_len, number_of_entities, expected_len, static_cast<uint8_t>(function_code));
    return pdu;
  }
  append_pdu_header(pdu, function_code, start_address, number_of_entities);
  pdu.push_back(values_len);  // Byte count is required for write multiple
  for (size_t i = 0; i < values_len; i++)
    pdu.push_back(values[i]);
  if (bits)
    mask_trailing_pad_bits(pdu, number_of_entities);
  return pdu;
}

// Validate one register block for a client builder: a non-zero quantity within max_quantity that does not
// run past the 16-bit address space (register count × 2 stays within MAX_PDU_SIZE as a result). On failure
// it logs the reason and returns false, on which the caller returns an empty PDU. `role` names the block in
// the log ("Read"/"Write"). Logging twin of quantity_in_range(): the same three predicates, split so each
// failure names its reason, and taking size_t so an oversize span is caught before any narrowing.
static bool register_block_in_range(const LogString *role, uint16_t start_address, size_t quantity,
                                    uint16_t max_quantity) {
  if (quantity == 0 || quantity > max_quantity) {
    ESP_LOGE(TAG, "%s count %zu out of range [1, %u], dropping request", LOG_STR_ARG(role), quantity, max_quantity);
    return false;
  }
  if (!address_range_fits(start_address, quantity)) {
    ESP_LOGE(TAG, "%s of %zu registers at %u runs past the 16-bit address space, dropping request", LOG_STR_ARG(role),
             quantity, start_address);
    return false;
  }
  return true;
}

PduBuffer create_write_registers_pdu(uint16_t start_address, std::span<const uint16_t> values) {
  PduBuffer pdu;  // declared before every return so NRVO fires (all paths return the same object)
  if (!register_block_in_range(LOG_STR("Write"), start_address, values.size(), MAX_NUM_OF_REGISTERS_TO_WRITE)) {
    return pdu;
  }
  append_pdu_header(pdu, FunctionCode::WRITE_MULTIPLE_REGISTERS, start_address, values.size());
  pdu.push_back(static_cast<uint8_t>(values.size() * 2));  // byte count
  for (auto v : values) {
    append_pdu_word(pdu, v);
  }
  return pdu;
}

PduBuffer create_read_write_multiple_registers_pdu(uint16_t read_start_address, uint16_t read_count,
                                                   uint16_t write_start_address,
                                                   std::span<const uint16_t> write_values) {
  PduBuffer pdu;
  if (!register_block_in_range(LOG_STR("Read"), read_start_address, read_count, MAX_NUM_OF_REGISTERS_TO_READ)) {
    return pdu;
  }
  if (!register_block_in_range(LOG_STR("Write"), write_start_address, write_values.size(),
                               MAX_NUM_OF_REGISTERS_TO_WRITE_RW)) {
    return pdu;
  }
  // fc + read start(2) + read qty(2) + write start(2) + write qty(2) + write byte count(1) + write values.
  const auto write_count = static_cast<uint16_t>(write_values.size());
  pdu.push_back(static_cast<uint8_t>(FunctionCode::READ_WRITE_MULTIPLE_REGISTERS));
  append_pdu_word(pdu, read_start_address);
  append_pdu_word(pdu, read_count);
  append_pdu_word(pdu, write_start_address);
  append_pdu_word(pdu, write_count);
  pdu.push_back(static_cast<uint8_t>(write_count * 2));  // byte count
  for (auto v : write_values) {
    append_pdu_word(pdu, v);
  }
  return pdu;
}

WriteSinglePdu create_write_single_register_pdu(uint16_t start_address, uint16_t value) {
  WriteSinglePdu pdu;
  append_pdu_header(pdu, FunctionCode::WRITE_SINGLE_REGISTER, start_address, value);
  return pdu;
}

WriteSinglePdu create_write_single_coil_pdu(uint16_t address, bool value) {
  WriteSinglePdu pdu;
  append_pdu_header(pdu, FunctionCode::WRITE_SINGLE_COIL, address, value ? 0xFF00 : 0x0000);
  return pdu;
}

// Shared core for the two coil-write overloads: validates, then builds into the caller's named
// pdu (left empty on failure). Each overload's returns all name one local, so NRVO fires.
static void build_write_coils_pdu(PduBuffer &pdu, uint16_t start_address, PackedBits bits) {
  const uint16_t count = bits.size();
  const std::span<const uint8_t> packed_bits = bits.bytes();
  if (count == 0) {
    ESP_LOGE(TAG, "No coils requested for write multiple coils, dropping request");
    return;
  }
  if (count > MAX_NUM_OF_COILS_TO_WRITE) {
    ESP_LOGE(TAG, "count %u exceeds maximum coils to write %u, dropping request", count, MAX_NUM_OF_COILS_TO_WRITE);
    return;
  }
  if (!address_range_fits(start_address, count)) {
    ESP_LOGE(TAG, "Write of %u coils at %u runs past the 16-bit address space, dropping request", count, start_address);
    return;
  }
  const size_t byte_count = packed_bit_bytes(count);
  if (packed_bits.size() < byte_count) {
    ESP_LOGE(TAG, "packed_bits (%zu bytes) does not cover %u coils (%zu bytes), dropping request", packed_bits.size(),
             count, byte_count);
    return;
  }
  append_pdu_header(pdu, FunctionCode::WRITE_MULTIPLE_COILS, start_address, count);
  pdu.push_back(static_cast<uint8_t>(byte_count));
  for (size_t i = 0; i != byte_count; i++) {
    pdu.push_back(packed_bits[i]);
  }
  mask_trailing_pad_bits(pdu, count);
}

PduBuffer create_write_coils_pdu(uint16_t start_address, PackedBits bits) {
  PduBuffer pdu;
  build_write_coils_pdu(pdu, start_address, bits);
  return pdu;
}

// Shared by the two bool-container overloads: both index the same way, so the packing is written once.
template<typename BoolContainer>
static PduBuffer create_write_coils_pdu_from_bools(uint16_t start_address, const BoolContainer &values) {
  PduBuffer pdu;  // declared before every return so NRVO fires (all paths return the same object)
  const size_t count = values.size();
  // Bound before packing so the transient buffer below cannot overflow; the shared core validates the rest.
  if (count > MAX_NUM_OF_COILS_TO_WRITE) {
    ESP_LOGE(TAG, "values.size() %zu exceeds maximum coils to write %u, dropping request", count,
             MAX_NUM_OF_COILS_TO_WRITE);
    return pdu;
  }
  CoilPackBuffer packed;
  pack_bits(packed, values);
  build_write_coils_pdu(pdu, start_address, PackedBits(std::span<const uint8_t>(packed.data(), packed.size()), count));
  return pdu;
}

PduBuffer create_write_coils_pdu(uint16_t start_address, std::span<const bool> values) {
  return create_write_coils_pdu_from_bools(start_address, values);
}

PduBuffer create_write_coils_pdu(uint16_t start_address, const std::vector<bool> &values) {
  return create_write_coils_pdu_from_bools(start_address, values);
}
}  // namespace esphome::modbus::helpers
