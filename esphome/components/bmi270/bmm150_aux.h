#pragma once
#include <cstdint>

// BMM150 magnetometer definitions - the chip that can optionally be wired to the BMI270's
// auxiliary (secondary I2C master) interface. Lives in its own header/namespace because this
// is BMM150-specific driver knowledge, not BMI270 knowledge - it just can't be a standalone
// ESPHome component, since the BMM150 has no address of its own on the main I2C bus and is only
// reachable by having the BMI270 proxy register reads/writes through its aux interface.
namespace esphome::bmi270::bmm150 {

static const uint8_t BMM150_DEFAULT_I2C_ADDRESS = 0x10;
static const uint8_t BMM150_REG_CHIP_ID = 0x40;
static const uint8_t BMM150_REG_DATA_X_LSB = 0x42;
static const uint8_t BMM150_REG_POWER_CONTROL = 0x4B;
static const uint8_t BMM150_REG_OP_MODE = 0x4C;
static const uint8_t BMM150_CHIP_ID_VALUE = 0x32;
static const uint8_t BMM150_CMD_POWER_ON_RESET = 0x83;
static const uint8_t BMM150_CMD_NORMAL_MODE_ODR_30HZ = 0x38;
// µT per LSB, per BMM150 datasheet (13-bit data, ±1300µT full scale in x/y).
static constexpr float BMM150_MICROTESLA_PER_LSB = 10.0f * 4912.0f / 32768.0f;

// Result of a BMM150 magnetometer reading, in µT.
struct BMM150Data {
  float x;
  float y;
  float z;
};

}  // namespace esphome::bmi270::bmm150
