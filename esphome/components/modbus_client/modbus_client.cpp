#include "modbus_client.h"
#include "esphome/core/hal.h"

#include <array>

namespace esphome::modbus_client {

using modbus::helpers::PduBuffer;

PduBuffer static_write_registers_pdu(uint16_t start, const uint16_t *values, size_t len) {
#ifdef USE_ESP8266
  std::array<uint16_t, modbus::MAX_NUM_OF_REGISTERS_TO_WRITE> staging;
  progmem_memcpy(staging.data(), values, len * sizeof(uint16_t));
  values = staging.data();
#endif
  return modbus::helpers::create_write_registers_pdu(start, std::span<const uint16_t>(values, len));
}

PduBuffer static_write_coils_pdu(uint16_t start, const uint8_t *packed, uint16_t count) {
  const size_t len = modbus::packed_bit_bytes(count);
#ifdef USE_ESP8266
  std::array<uint8_t, modbus::packed_bit_bytes(modbus::MAX_NUM_OF_COILS_TO_WRITE)> staging;
  progmem_memcpy(staging.data(), packed, len);
  packed = staging.data();
#endif
  return modbus::helpers::create_write_coils_pdu(start,
                                                 modbus::PackedBits(std::span<const uint8_t>(packed, len), count));
}

PduBuffer static_read_write_registers_pdu(uint16_t read_start, uint16_t read_count, uint16_t write_start,
                                          const uint16_t *values, size_t len) {
#ifdef USE_ESP8266
  std::array<uint16_t, modbus::MAX_NUM_OF_REGISTERS_TO_WRITE_RW> staging;
  progmem_memcpy(staging.data(), values, len * sizeof(uint16_t));
  values = staging.data();
#endif
  return modbus::helpers::create_read_write_multiple_registers_pdu(read_start, read_count, write_start,
                                                                   std::span<const uint16_t>(values, len));
}

}  // namespace esphome::modbus_client
