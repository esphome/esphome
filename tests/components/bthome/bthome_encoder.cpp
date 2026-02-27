#include <gtest/gtest.h>
#include <cmath>
#include "esphome/components/bthome/bthome_encoder.h"
#include "esphome/components/bthome/bthome_decoder.h"

namespace esphome::bthome::server::testing {

class BTHomeEncoderTest : public ::testing::Test {
 protected:
  void SetUp() override { encoder_.reset(); }
  BTHomeEncoder encoder_;
};

// --- Basic encoding tests ---

TEST_F(BTHomeEncoderTest, ResetClearsState) {
  encoder_.write_bool(BTHomeObjectType::DOOR_OPEN, true);
  EXPECT_GT(encoder_.get_size(), 0u);
  encoder_.reset();
  EXPECT_EQ(encoder_.get_size(), 0u);
  EXPECT_EQ(encoder_.get_remaining(), BTHOME_SERVER_MAX_PAYLOAD);
}

TEST_F(BTHomeEncoderTest, WriteBoolTrue) {
  EXPECT_TRUE(encoder_.write_bool(BTHomeObjectType::DOOR_OPEN, true));
  EXPECT_EQ(encoder_.get_size(), 2u);  // 1 type + 1 value
  const uint8_t *buf = encoder_.get_buffer();
  EXPECT_EQ(buf[0], static_cast<uint8_t>(BTHomeObjectType::DOOR_OPEN));
  EXPECT_EQ(buf[1], 0x01);
}

TEST_F(BTHomeEncoderTest, WriteBoolFalse) {
  EXPECT_TRUE(encoder_.write_bool(BTHomeObjectType::MOTION_DETECTED, false));
  const uint8_t *buf = encoder_.get_buffer();
  EXPECT_EQ(buf[0], static_cast<uint8_t>(BTHomeObjectType::MOTION_DETECTED));
  EXPECT_EQ(buf[1], 0x00);
}

// --- 1-byte unsigned ---

TEST_F(BTHomeEncoderTest, WriteFloatBatteryPct) {
  // BATTERY_PCT: 1 byte, unsigned, scale 1.0
  EXPECT_TRUE(encoder_.write_float(BTHomeObjectType::BATTERY_PCT, 97.0f));
  EXPECT_EQ(encoder_.get_size(), 2u);
  const uint8_t *buf = encoder_.get_buffer();
  EXPECT_EQ(buf[0], 0x01);  // BATTERY_PCT
  EXPECT_EQ(buf[1], 97);
}

// --- 1-byte signed ---

TEST_F(BTHomeEncoderTest, WriteFloatTemperatureI8) {
  // TEMPERATURE_C_I8: 1 byte, signed, scale 1.0
  EXPECT_TRUE(encoder_.write_float(BTHomeObjectType::TEMPERATURE_C_I8, -10.0f));
  EXPECT_EQ(encoder_.get_size(), 2u);
  const uint8_t *buf = encoder_.get_buffer();
  EXPECT_EQ(buf[0], static_cast<uint8_t>(BTHomeObjectType::TEMPERATURE_C_I8));
  EXPECT_EQ(buf[1], static_cast<uint8_t>(static_cast<int8_t>(-10)));
}

// --- 2-byte signed with scaling ---

TEST_F(BTHomeEncoderTest, WriteFloatTemperatureE2) {
  // TEMPERATURE_C_E2: 2 bytes, signed, scale 0.01
  // 23.45 / 0.01 = 2345 = 0x0929
  EXPECT_TRUE(encoder_.write_float(BTHomeObjectType::TEMPERATURE_C_E2, 23.45f));
  EXPECT_EQ(encoder_.get_size(), 3u);
  const uint8_t *buf = encoder_.get_buffer();
  EXPECT_EQ(buf[0], 0x02);  // TEMPERATURE_C_E2
  EXPECT_EQ(buf[1], 0x29);  // low byte
  EXPECT_EQ(buf[2], 0x09);  // high byte
}

TEST_F(BTHomeEncoderTest, WriteFloatNegativeTemperature) {
  // -10.5 / 0.01 = -1050 = 0xFBE6 as uint16
  EXPECT_TRUE(encoder_.write_float(BTHomeObjectType::TEMPERATURE_C_E2, -10.50f));
  EXPECT_EQ(encoder_.get_size(), 3u);
  const uint8_t *buf = encoder_.get_buffer();
  EXPECT_EQ(buf[0], 0x02);
  EXPECT_EQ(buf[1], 0xE6);  // low byte of -1050
  EXPECT_EQ(buf[2], 0xFB);  // high byte of -1050
}

// --- 2-byte unsigned ---

TEST_F(BTHomeEncoderTest, WriteFloatHumidity) {
  // HUMIDITY_PCT_E2: 2 bytes, unsigned, scale 0.01
  // 55.5 / 0.01 = 5550 = 0x15AE
  EXPECT_TRUE(encoder_.write_float(BTHomeObjectType::HUMIDITY_PCT_E2, 55.50f));
  EXPECT_EQ(encoder_.get_size(), 3u);
  const uint8_t *buf = encoder_.get_buffer();
  EXPECT_EQ(buf[0], 0x03);  // HUMIDITY_PCT_E2
  EXPECT_EQ(buf[1], 0xAE);
  EXPECT_EQ(buf[2], 0x15);
}

// --- 3-byte unsigned ---

TEST_F(BTHomeEncoderTest, WriteFloatPressure) {
  // PRESSURE_HPA_E2: 3 bytes, unsigned, scale 0.01
  // 1013.25 / 0.01 = 101325 = 0x018BCD
  EXPECT_TRUE(encoder_.write_float(BTHomeObjectType::PRESSURE_HPA_E2, 1013.25f));
  EXPECT_EQ(encoder_.get_size(), 4u);
  const uint8_t *buf = encoder_.get_buffer();
  EXPECT_EQ(buf[0], 0x04);  // PRESSURE_HPA_E2
  EXPECT_EQ(buf[1], 0xCD);
  EXPECT_EQ(buf[2], 0x8B);
  EXPECT_EQ(buf[3], 0x01);
}

// --- 4-byte unsigned ---

TEST_F(BTHomeEncoderTest, WriteFloatCountU32) {
  // COUNT_U32: 4 bytes, unsigned, scale 1.0
  EXPECT_TRUE(encoder_.write_float(BTHomeObjectType::COUNT_U32, 100000.0f));
  EXPECT_EQ(encoder_.get_size(), 5u);
  const uint8_t *buf = encoder_.get_buffer();
  EXPECT_EQ(buf[0], static_cast<uint8_t>(BTHomeObjectType::COUNT_U32));
  // 100000 = 0x000186A0
  EXPECT_EQ(buf[1], 0xA0);
  EXPECT_EQ(buf[2], 0x86);
  EXPECT_EQ(buf[3], 0x01);
  EXPECT_EQ(buf[4], 0x00);
}

// --- Buffer overflow ---

TEST_F(BTHomeEncoderTest, WriteOverflowReturnsFalse) {
  // Fill most of the buffer with 2-byte bool writes (2 bytes each)
  for (size_t i = 0; i < BTHOME_SERVER_MAX_PAYLOAD / 2; i++) {
    encoder_.write_bool(BTHomeObjectType::DOOR_OPEN, true);
  }
  // Now try to write something that needs more space than remaining
  size_t remaining = encoder_.get_remaining();
  if (remaining < 3) {  // need 3 bytes for a 2-byte value type
    EXPECT_FALSE(encoder_.write_float(BTHomeObjectType::TEMPERATURE_C_E2, 20.0f));
  }
}

TEST_F(BTHomeEncoderTest, WriteExactlyFillsBuffer) {
  // Fill with single-byte entries (2 bytes each: type + value)
  size_t count = BTHOME_SERVER_MAX_PAYLOAD / 2;
  for (size_t i = 0; i < count; i++) {
    EXPECT_TRUE(encoder_.write_bool(BTHomeObjectType::DOOR_OPEN, true));
  }
  // If payload is odd, one byte remains — not enough for another entry
  if (BTHOME_SERVER_MAX_PAYLOAD % 2 == 1) {
    EXPECT_EQ(encoder_.get_remaining(), 1u);
    EXPECT_FALSE(encoder_.write_bool(BTHomeObjectType::DOOR_OPEN, true));
  } else {
    EXPECT_EQ(encoder_.get_remaining(), 0u);
  }
}

// --- Multiple writes ---

TEST_F(BTHomeEncoderTest, WriteMultipleObjects) {
  EXPECT_TRUE(encoder_.write_float(BTHomeObjectType::BATTERY_PCT, 97.0f));
  EXPECT_TRUE(encoder_.write_float(BTHomeObjectType::TEMPERATURE_C_E2, 23.45f));
  EXPECT_TRUE(encoder_.write_bool(BTHomeObjectType::DOOR_OPEN, false));
  // 2 + 3 + 2 = 7 bytes total
  EXPECT_EQ(encoder_.get_size(), 7u);
}

// --- Variable-length type returns false ---

TEST_F(BTHomeEncoderTest, WriteFloatVariableLengthReturnsFalse) {
  EXPECT_FALSE(encoder_.write_float(BTHomeObjectType::TEXT, 0.0f));
  EXPECT_FALSE(encoder_.write_float(BTHomeObjectType::RAW, 0.0f));
  EXPECT_EQ(encoder_.get_size(), 0u);
}

// --- Round-trip tests: encode then decode ---

TEST_F(BTHomeEncoderTest, RoundTripBattery) {
  float original = 97.0f;
  encoder_.write_float(BTHomeObjectType::BATTERY_PCT, original);
  // Decode: skip type byte, data starts at offset 1
  BTHomeObject obj{
      .type = BTHomeObjectType::BATTERY_PCT,
      .data = encoder_.get_buffer() + 1,
      .length = 1,
  };
  EXPECT_FLOAT_EQ(obj.as_float(), original);
}

TEST_F(BTHomeEncoderTest, RoundTripTemperature) {
  float original = 23.45f;
  encoder_.write_float(BTHomeObjectType::TEMPERATURE_C_E2, original);
  BTHomeObject obj{
      .type = BTHomeObjectType::TEMPERATURE_C_E2,
      .data = encoder_.get_buffer() + 1,
      .length = 2,
  };
  EXPECT_NEAR(obj.as_float(), original, 0.01f);
}

TEST_F(BTHomeEncoderTest, RoundTripNegativeTemperature) {
  float original = -10.50f;
  encoder_.write_float(BTHomeObjectType::TEMPERATURE_C_E2, original);
  BTHomeObject obj{
      .type = BTHomeObjectType::TEMPERATURE_C_E2,
      .data = encoder_.get_buffer() + 1,
      .length = 2,
  };
  EXPECT_NEAR(obj.as_float(), original, 0.01f);
}

TEST_F(BTHomeEncoderTest, RoundTripHumidity) {
  float original = 55.50f;
  encoder_.write_float(BTHomeObjectType::HUMIDITY_PCT_E2, original);
  BTHomeObject obj{
      .type = BTHomeObjectType::HUMIDITY_PCT_E2,
      .data = encoder_.get_buffer() + 1,
      .length = 2,
  };
  EXPECT_NEAR(obj.as_float(), original, 0.01f);
}

TEST_F(BTHomeEncoderTest, RoundTripPressure) {
  float original = 1013.25f;
  encoder_.write_float(BTHomeObjectType::PRESSURE_HPA_E2, original);
  BTHomeObject obj{
      .type = BTHomeObjectType::PRESSURE_HPA_E2,
      .data = encoder_.get_buffer() + 1,
      .length = 3,
  };
  EXPECT_NEAR(obj.as_float(), original, 0.01f);
}

// --- Clamping tests ---

TEST_F(BTHomeEncoderTest, ClampUnsigned1Byte) {
  // Value exceeding uint8 max should be clamped to 255
  encoder_.write_float(BTHomeObjectType::BATTERY_PCT, 300.0f);
  const uint8_t *buf = encoder_.get_buffer();
  EXPECT_EQ(buf[1], 255);
}

TEST_F(BTHomeEncoderTest, ClampSigned1Byte) {
  // Value below int8 min should be clamped to -128
  encoder_.write_float(BTHomeObjectType::TEMPERATURE_C_I8, -200.0f);
  BTHomeObject obj{
      .type = BTHomeObjectType::TEMPERATURE_C_I8,
      .data = encoder_.get_buffer() + 1,
      .length = 1,
  };
  EXPECT_FLOAT_EQ(obj.as_float(), -128.0f);
}

}  // namespace esphome::bthome::server::testing
