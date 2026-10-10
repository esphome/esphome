#pragma once
#include <cmath>
#include <cstdint>

// BMM150 magnetometer definitions - the chip that can optionally be wired to the BMI270's
// auxiliary (secondary I2C master) interface. Lives in its own header/namespace because this
// is BMM150-specific driver knowledge, not BMI270 knowledge - it just can't be a standalone
// ESPHome component, since the BMM150 has no address of its own on the main I2C bus and is only
// reachable by having the BMI270 proxy register reads/writes through its aux interface.
namespace esphome::bmi270::bmm150 {

static constexpr uint8_t BMM150_DEFAULT_I2C_ADDRESS = 0x10;
static constexpr uint8_t BMM150_REG_CHIP_ID = 0x40;
static constexpr uint8_t BMM150_REG_DATA_X_LSB = 0x42;
static constexpr uint8_t BMM150_REG_POWER_CONTROL = 0x4B;
static constexpr uint8_t BMM150_REG_OP_MODE = 0x4C;
static constexpr uint8_t BMM150_CHIP_ID_VALUE = 0x32;
static constexpr uint8_t BMM150_CMD_POWER_ON = 0x01;              // power control bit: suspend -> sleep (3 ms start-up)
static constexpr uint8_t BMM150_CMD_SOFT_RESET = 0x83;            // soft reset bits 7 and 1, power control bit kept set
static constexpr uint8_t BMM150_CMD_NORMAL_MODE_ODR_30HZ = 0x38;  // data rate 111b (30 Hz), opmode 00b (normal)
static constexpr uint32_t BMM150_START_UP_TIME_MS = 3;
static constexpr uint32_t BMM150_SOFT_RESET_TIME_MS = 1;

// Factory trim registers 0x5D-0x71, read once at setup and used to compensate every sample.
static constexpr uint8_t BMM150_REG_TRIM_START = 0x5D;
static constexpr uint8_t BMM150_TRIM_BLOCK_LEN = 8;  // one aux burst
static constexpr uint8_t BMM150_TRIM_BLOCKS = 3;     // 0x5D, 0x65, 0x6D -> covers 0x5D-0x74
static constexpr uint8_t BMM150_TRIM_X1 = 0x5D;
static constexpr uint8_t BMM150_TRIM_Y1 = 0x5E;
static constexpr uint8_t BMM150_TRIM_Z4 = 0x62;  // 16-bit, LSB first
static constexpr uint8_t BMM150_TRIM_X2 = 0x64;
static constexpr uint8_t BMM150_TRIM_Y2 = 0x65;
static constexpr uint8_t BMM150_TRIM_Z2 = 0x68;    // 16-bit
static constexpr uint8_t BMM150_TRIM_Z1 = 0x6A;    // 16-bit
static constexpr uint8_t BMM150_TRIM_XYZ1 = 0x6C;  // 16-bit, 15 valid bits
static constexpr uint8_t BMM150_TRIM_Z3 = 0x6E;    // 16-bit
static constexpr uint8_t BMM150_TRIM_XY2 = 0x70;
static constexpr uint8_t BMM150_TRIM_XY1 = 0x71;
static constexpr uint16_t BMM150_TRIM_XYZ1_MASK = 0x7FFF;

// Raw ADC values that mean "overflow" per the datasheet.
static constexpr int16_t BMM150_XY_OVERFLOW_ADCVAL = -4096;
static constexpr int16_t BMM150_Z_OVERFLOW_ADCVAL = -16384;

// Result of a BMM150 magnetometer reading, in µT. An axis is NaN when the sensor reported overflow.
struct BMM150Data {
  float x;
  float y;
  float z;
};

// Factory trim values (BMM150 datasheet, compensation section).
struct BMM150Trim {
  int8_t x1, y1, x2, y2, xy2;
  uint8_t xy1;
  int16_t z2, z3, z4;
  uint16_t z1, xyz1;
};

// Decodes the trim registers; `regs` holds the bytes of registers 0x5D onwards (at least 21).
inline BMM150Trim bmm150_parse_trim(const uint8_t *regs) {
  auto u16 = [regs](uint8_t offset) { return (uint16_t) ((regs[offset + 1] << 8) | regs[offset]); };
  BMM150Trim trim;
  trim.x1 = (int8_t) regs[BMM150_TRIM_X1 - BMM150_REG_TRIM_START];
  trim.y1 = (int8_t) regs[BMM150_TRIM_Y1 - BMM150_REG_TRIM_START];
  trim.z4 = (int16_t) u16(BMM150_TRIM_Z4 - BMM150_REG_TRIM_START);
  trim.x2 = (int8_t) regs[BMM150_TRIM_X2 - BMM150_REG_TRIM_START];
  trim.y2 = (int8_t) regs[BMM150_TRIM_Y2 - BMM150_REG_TRIM_START];
  trim.z2 = (int16_t) u16(BMM150_TRIM_Z2 - BMM150_REG_TRIM_START);
  trim.z1 = u16(BMM150_TRIM_Z1 - BMM150_REG_TRIM_START);
  trim.xyz1 = u16(BMM150_TRIM_XYZ1 - BMM150_REG_TRIM_START) & BMM150_TRIM_XYZ1_MASK;
  trim.z3 = (int16_t) u16(BMM150_TRIM_Z3 - BMM150_REG_TRIM_START);
  trim.xy2 = (int8_t) regs[BMM150_TRIM_XY2 - BMM150_REG_TRIM_START];
  trim.xy1 = regs[BMM150_TRIM_XY1 - BMM150_REG_TRIM_START];
  return trim;
}

// Compensates an X or Y sample; returns µT, or NaN on overflow. Port of the Bosch BMM150 API (float variant).
inline float bmm150_compensate_xy(const BMM150Trim &trim, int8_t trim_a, int8_t trim_b, int16_t raw, uint16_t rhall) {
  if (raw == BMM150_XY_OVERFLOW_ADCVAL || rhall == 0 || trim.xyz1 == 0)
    return NAN;
  float retval = (float) trim.xyz1 * 16384.0f / (float) rhall - 16384.0f;
  float comp1 = (float) trim.xy2 * (retval * retval / 268435456.0f);
  float comp2 = comp1 + retval * (float) trim.xy1 / 16384.0f;
  float comp3 = (float) trim_b + 160.0f;
  float comp4 = (float) raw * ((comp2 + 256.0f) * comp3);
  return ((comp4 / 8192.0f) + (float) trim_a * 8.0f) / 16.0f;
}

// Compensates a Z sample; returns µT, or NaN on overflow. Port of the Bosch BMM150 API (float variant).
inline float bmm150_compensate_z(const BMM150Trim &trim, int16_t raw, uint16_t rhall) {
  if (raw == BMM150_Z_OVERFLOW_ADCVAL || trim.z2 == 0 || trim.z1 == 0 || trim.xyz1 == 0 || rhall == 0)
    return NAN;
  float comp0 = (float) raw - (float) trim.z4;
  float comp1 = (float) rhall - (float) trim.xyz1;
  float comp2 = (float) trim.z3 * comp1;
  float comp3 = (float) trim.z1 * (float) rhall / 32768.0f;
  float comp4 = (float) trim.z2 + comp3;
  float comp5 = comp0 * 131072.0f - comp2;
  return (comp5 / (comp4 * 4.0f)) / 16.0f;
}

// Converts 8 raw bytes from the BMM150 data registers (X, Y, Z, RHALL; 0x42-0x49) to compensated µT.
inline BMM150Data bmm150_convert(const BMM150Trim &trim, const uint8_t *raw) {
  // X/Y are 13-bit (LSB byte bits 7:3), Z is 15-bit (bits 7:1), RHALL is 14-bit (bits 7:2).
  int16_t x = (int16_t) (((int16_t) (int8_t) raw[1] * 32) | (raw[0] >> 3));
  int16_t y = (int16_t) (((int16_t) (int8_t) raw[3] * 32) | (raw[2] >> 3));
  int16_t z = (int16_t) (((int16_t) (int8_t) raw[5] * 128) | (raw[4] >> 1));
  uint16_t rhall = (uint16_t) (((uint16_t) raw[7] << 6) | (raw[6] >> 2));
  return BMM150Data{
      .x = bmm150_compensate_xy(trim, trim.x1, trim.x2, x, rhall),
      .y = bmm150_compensate_xy(trim, trim.y1, trim.y2, y, rhall),
      .z = bmm150_compensate_z(trim, z, rhall),
  };
}

}  // namespace esphome::bmi270::bmm150
