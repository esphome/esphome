#include "modbus_helpers.h"
#include "esphome/core/log.h"

#include <algorithm>

namespace esphome::modbus::helpers {

static const char *const TAG = "modbus_helpers";

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
    case FunctionCode::WRITE_MULTIPLE_REGISTERS:
      // function(1) + start address(2) + quantity(2) + byte count(1) + data
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

  switch (static_cast<FunctionCode>(pdu[0])) {
    case FunctionCode::READ_COILS:
    case FunctionCode::READ_DISCRETE_INPUTS:
    case FunctionCode::READ_HOLDING_REGISTERS:
    case FunctionCode::READ_INPUT_REGISTERS:
      return pdu[1] <= uint8_t(MAX_NUM_OF_REGISTERS_TO_READ * 2);
    case FunctionCode::READ_FILE_RECORD:
    case FunctionCode::WRITE_FILE_RECORD:
      return pdu[1] <= uint8_t(MAX_PDU_SIZE - 2);
    case FunctionCode::READ_WRITE_MULTIPLE_REGISTERS:
      return pdu[1] <= uint8_t(MAX_NUM_OF_REGISTERS_TO_READ * 2);
    default:
      return true;  // All other function codes validated by length alone
  }
}

bool is_client_pdu_standard(const uint8_t *pdu, size_t size) {
  if (client_pdu_length(pdu, size) != size)
    return false;

  switch (static_cast<FunctionCode>(pdu[0])) {
    case FunctionCode::READ_COILS:
    case FunctionCode::READ_DISCRETE_INPUTS:
    case FunctionCode::READ_HOLDING_REGISTERS:
    case FunctionCode::READ_INPUT_REGISTERS: {
      const bool bits = static_cast<FunctionCode>(pdu[0]) == FunctionCode::READ_COILS ||
                        static_cast<FunctionCode>(pdu[0]) == FunctionCode::READ_DISCRETE_INPUTS;
      const uint16_t start_address = get_data<uint16_t>(pdu, 1);
      const uint16_t quantity = get_data<uint16_t>(pdu, 3);
      const uint16_t max_quantity = bits ? MAX_NUM_OF_COILS_TO_READ : MAX_NUM_OF_REGISTERS_TO_READ;
      return quantity != 0 && quantity <= max_quantity && (uint32_t) start_address + quantity <= 0x10000u;
    }
    case FunctionCode::WRITE_MULTIPLE_COILS:
    case FunctionCode::WRITE_MULTIPLE_REGISTERS: {
      const bool bits = static_cast<FunctionCode>(pdu[0]) == FunctionCode::WRITE_MULTIPLE_COILS;
      const uint16_t start_address = get_data<uint16_t>(pdu, 1);
      const uint16_t quantity = get_data<uint16_t>(pdu, 3);
      const uint16_t max_quantity = bits ? MAX_NUM_OF_COILS_TO_WRITE : MAX_NUM_OF_REGISTERS_TO_WRITE;
      // Coils are packed 8 per data byte; registers are 2 bytes each.
      const size_t expected_data_bytes = bits ? (static_cast<size_t>(quantity) + 7) / 8 : quantity * 2;
      return quantity != 0 && quantity <= max_quantity && (uint32_t) start_address + quantity <= 0x10000u &&
             pdu[5] == expected_data_bytes;
    }
    case FunctionCode::READ_FILE_RECORD:
    case FunctionCode::WRITE_FILE_RECORD:
      return pdu[1] <= MAX_PDU_SIZE - 2;
    case FunctionCode::READ_WRITE_MULTIPLE_REGISTERS: {
      const uint16_t start_address_read = get_data<uint16_t>(pdu, 1);
      const uint16_t quantity_read = get_data<uint16_t>(pdu, 3);
      const uint16_t start_address_write = get_data<uint16_t>(pdu, 5);
      const uint16_t quantity_write = get_data<uint16_t>(pdu, 7);
      return quantity_read != 0 && quantity_read <= MAX_NUM_OF_REGISTERS_TO_READ && quantity_write != 0 &&
             quantity_write <= MAX_NUM_OF_REGISTERS_TO_WRITE_RW &&
             (uint32_t) start_address_read + quantity_read <= 0x10000u &&
             (uint32_t) start_address_write + quantity_write <= 0x10000u && pdu[9] == quantity_write * 2;
    }
    default:
      return true;  // All other function codes validated by length alone
  }
}

static size_t required_payload_size(SensorValueType sensor_value_type) {
  switch (sensor_value_type) {
    case SensorValueType::U_WORD:
    case SensorValueType::S_WORD:
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

StaticVector<uint8_t, MAX_PDU_SIZE> create_client_pdu(FunctionCode function_code, uint16_t start_address,
                                                      uint16_t number_of_entities, const uint8_t *values,
                                                      size_t values_len) {
  if (is_function_code_read(static_cast<uint8_t>(function_code))) {
    if (values != nullptr || values_len > 0) {
      ESP_LOGW(TAG, "Values provided for read function code %02X, but will be ignored",
               static_cast<uint8_t>(function_code));
    }
  } else if (is_function_code_write(static_cast<uint8_t>(function_code))) {
    if (values == nullptr || values_len == 0) {
      ESP_LOGE(TAG, "No values provided for write function code %02X", static_cast<uint8_t>(function_code));
      return {};
    }
  } else {
    ESP_LOGE(TAG, "Unsupported function code %02X for client PDU creation", static_cast<uint8_t>(function_code));
    return {};
  }

  if (number_of_entities == 0) {
    ESP_LOGE(TAG, "Number of entities is zero for function code %02X", static_cast<uint8_t>(function_code));
    return {};
  }

  switch (function_code) {
    case FunctionCode::READ_COILS:
      if (number_of_entities > MAX_NUM_OF_COILS_TO_READ) {
        ESP_LOGE(TAG, "number_of_entities %u exceeds maximum coils to read %u for function code %02X",
                 number_of_entities, MAX_NUM_OF_COILS_TO_READ, static_cast<uint8_t>(function_code));
        return {};
      }
      break;
    case FunctionCode::READ_DISCRETE_INPUTS:
      if (number_of_entities > MAX_NUM_OF_DISCRETE_INPUTS_TO_READ) {
        ESP_LOGE(TAG, "number_of_entities %u exceeds maximum discrete inputs to read %u for function code %02X",
                 number_of_entities, MAX_NUM_OF_DISCRETE_INPUTS_TO_READ, static_cast<uint8_t>(function_code));
        return {};
      }
      break;
    case FunctionCode::READ_HOLDING_REGISTERS:
    case FunctionCode::READ_INPUT_REGISTERS:
      if (number_of_entities > MAX_NUM_OF_REGISTERS_TO_READ) {
        ESP_LOGE(TAG, "number_of_entities %u exceeds maximum registers to read %u for function code %02X",
                 number_of_entities, MAX_NUM_OF_REGISTERS_TO_READ, static_cast<uint8_t>(function_code));
        return {};
      }
      break;
    case FunctionCode::WRITE_SINGLE_COIL:
    case FunctionCode::WRITE_SINGLE_REGISTER:
      break;  // number_of_entities is ignored for single write, so no need to validate
    case FunctionCode::WRITE_MULTIPLE_COILS:
    case FunctionCode::WRITE_MULTIPLE_REGISTERS:
      if (number_of_entities > MAX_NUM_OF_REGISTERS_TO_WRITE) {
        ESP_LOGE(TAG, "number_of_entities %u exceeds maximum registers to write %u for function code %02X",
                 number_of_entities, MAX_NUM_OF_REGISTERS_TO_WRITE, static_cast<uint8_t>(function_code));
        return {};
      }
      break;
    default:
      ESP_LOGE(TAG, "Unsupported function code %u for client PDU creation", static_cast<unsigned int>(function_code));
      return {};
  }

  StaticVector<uint8_t, MAX_PDU_SIZE> pdu;
  pdu.push_back(static_cast<uint8_t>(function_code));
  pdu.push_back(start_address >> 8);
  pdu.push_back(start_address >> 0);
  if (function_code != FunctionCode::WRITE_SINGLE_COIL && function_code != FunctionCode::WRITE_SINGLE_REGISTER) {
    pdu.push_back(number_of_entities >> 8);
    pdu.push_back(number_of_entities >> 0);
  }

  if (is_function_code_write(static_cast<uint8_t>(function_code))) {
    if (function_code == FunctionCode::WRITE_MULTIPLE_COILS ||
        function_code == FunctionCode::WRITE_MULTIPLE_REGISTERS) {
      // 6 bytes of overhead (fc + start_addr×2 + qty×2 + byte_count) leave MAX_PDU_SIZE-6 bytes for values
      static constexpr size_t MAX_WRITE_MULTIPLE_VALUES_LEN = MAX_PDU_SIZE - 6;
      if (values_len > MAX_WRITE_MULTIPLE_VALUES_LEN) {
        ESP_LOGE(TAG, "values_len %zu exceeds PDU capacity %zu, dropping request", values_len,
                 MAX_WRITE_MULTIPLE_VALUES_LEN);
        return {};
      }
      pdu.push_back(values_len);  // Byte count is required for write multiple
      for (size_t i = 0; i < values_len; i++)
        pdu.push_back(values[i]);
    } else {
      // Write single register or coil (2 bytes)
      if (values_len < 2) {
        ESP_LOGE(TAG, "values_len %zu too small for write-single command (need 2), dropping request", values_len);
        return {};
      }
      pdu.push_back(values[0]);
      pdu.push_back(values[1]);
    }
  }
  return pdu;
}
}  // namespace esphome::modbus::helpers
