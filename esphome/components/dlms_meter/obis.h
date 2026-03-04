#pragma once

#include <cstdint>

namespace esphome::dlms_meter {

// Data types as per specification
enum DataType {
  NULL_DATA = 0x00,
  BOOLEAN = 0x03,
  BIT_STRING = 0x04,
  DOUBLE_LONG = 0x05,
  DOUBLE_LONG_UNSIGNED = 0x06,
  OCTET_STRING = 0x09,
  VISIBLE_STRING = 0x0A,
  UTF8_STRING = 0x0C,
  BINARY_CODED_DECIMAL = 0x0D,
  INTEGER = 0x0F,
  LONG = 0x10,
  UNSIGNED = 0x11,
  LONG_UNSIGNED = 0x12,
  LONG64 = 0x14,
  LONG64_UNSIGNED = 0x15,
  ENUM = 0x16,
  FLOAT32 = 0x17,
  FLOAT64 = 0x18,
  DATE_TIME = 0x19,
  DATE = 0x1A,
  TIME = 0x1B,

  ARRAY = 0x01,
  STRUCTURE = 0x02,
  COMPACT_ARRAY = 0x13
};

enum Medium {
  ABSTRACT = 0x00,
  ELECTRICITY = 0x01,
  HEAT_COST_ALLOCATOR = 0x04,
  COOLING = 0x05,
  HEAT = 0x06,
  GAS = 0x07,
  COLD_WATER = 0x08,
  HOT_WATER = 0x09,
  OIL = 0x10,
  COMPRESSED_AIR = 0x11,
  NITROGEN = 0x12
};

// Data structure
static constexpr uint8_t DECODER_START_OFFSET = 20;  // Skip header, timestamp and break block
static constexpr uint8_t OBIS_TYPE_OFFSET = 0;
static constexpr uint8_t OBIS_LENGTH_OFFSET = 1;
static constexpr uint8_t OBIS_CODE_OFFSET = 2;
static constexpr uint8_t OBIS_CODE_LENGTH_STANDARD = 0x06;  // 6-byte OBIS code (A.B.C.D.E.F)
static constexpr uint8_t OBIS_CODE_LENGTH_EXTENDED = 0x0C;  // 12-byte extended OBIS code
static constexpr uint8_t OBIS_A = 0;
static constexpr uint8_t OBIS_B = 1;
static constexpr uint8_t OBIS_C = 2;
static constexpr uint8_t OBIS_D = 3;
static constexpr uint8_t OBIS_E = 4;
static constexpr uint8_t OBIS_F = 5;

// Metadata
static constexpr uint16_t OBIS_TIMESTAMP = 0x0100;
static constexpr uint16_t OBIS_SERIAL_NUMBER = 0x6001;
static constexpr uint16_t OBIS_DEVICE_NAME = 0x2A00;

// Voltage
static constexpr uint16_t OBIS_VOLTAGE_L1 = 0x2007;
static constexpr uint16_t OBIS_VOLTAGE_L2 = 0x3407;
static constexpr uint16_t OBIS_VOLTAGE_L3 = 0x4807;

// Current
static constexpr uint16_t OBIS_CURRENT_L1 = 0x1F07;
static constexpr uint16_t OBIS_CURRENT_L2 = 0x3307;
static constexpr uint16_t OBIS_CURRENT_L3 = 0x4707;

// Power
static constexpr uint16_t OBIS_ACTIVE_POWER_PLUS = 0x0107;
static constexpr uint16_t OBIS_ACTIVE_POWER_MINUS = 0x0207;

// Active energy
static constexpr uint16_t OBIS_ACTIVE_ENERGY_PLUS = 0x0108;
static constexpr uint16_t OBIS_ACTIVE_ENERGY_MINUS = 0x0208;

// Reactive energy
static constexpr uint16_t OBIS_REACTIVE_ENERGY_PLUS = 0x0308;
static constexpr uint16_t OBIS_REACTIVE_ENERGY_MINUS = 0x0408;

// Netz NOE specific
static constexpr uint16_t OBIS_POWER_FACTOR = 0x0D07;

}  // namespace esphome::dlms_meter
