#include <gtest/gtest.h>
#include <cmath>

#include "esphome/components/bthome/encoder.h"
#include "esphome/components/bthome/helpers.h"
#include "esphome/components/bthome/local_sensor.h"

namespace esphome::bthome::server::testing {

// ===========================================================================
// BTHomeLocalSensor — wraps sensor::Sensor for server-side BTHome encoding
//
// Tested behaviors:
//   get_encoded_size()  - returns 0 for NaN state, correct size otherwise
//   write()             - encodes type byte + value into BTHomeEncoder
//   register_immediate_callback() - callback fires on sensor state change
// ===========================================================================

class BTHomeLocalSensorTest : public ::testing::Test {
 protected:
  static constexpr BTHomeObjectType BATTERY_TYPE = BTHomeObjectType::BATTERY_PCT;
  static constexpr size_t BATTERY_ENCODED_SIZE = sizeof(BTHomeHeader) + get_bthome_value_length(BATTERY_TYPE);
  // BATTERY_PCT: scale=1.0, unsigned, 1 byte — encoded byte equals the rounded value
  static constexpr float BATTERY_VALUE = 75.0f;

  sensor::Sensor source_;
  BTHomeLocalSensor local_;

  void SetUp() override {
    local_.set_object_type(BATTERY_TYPE);
    local_.set_source(&source_);
  }
};

// Default state of a newly constructed Sensor is NaN — encoded size must be 0
TEST_F(BTHomeLocalSensorTest, GetEncodedSizeReturnsZeroWhenStateIsNaN) { EXPECT_EQ(local_.get_encoded_size(), 0u); }

// With a valid float state, encoded size = object type byte + value bytes
TEST_F(BTHomeLocalSensorTest, GetEncodedSizeReturnsCorrectSizeForValidState) {
  source_.state = BATTERY_VALUE;
  EXPECT_EQ(local_.get_encoded_size(), BATTERY_ENCODED_SIZE);
}

// write() must succeed and place the object type byte first in the encoder buffer
TEST_F(BTHomeLocalSensorTest, WriteEncodesSensorValue) {
  source_.state = BATTERY_VALUE;
  BTHomeEncoder encoder;
  ASSERT_TRUE(local_.write(encoder));
  EXPECT_EQ(encoder.get_buffer()[0], static_cast<uint8_t>(BATTERY_TYPE));
  EXPECT_EQ(encoder.get_buffer()[1], static_cast<uint8_t>(BATTERY_VALUE));
}

// Bytes written by write() must match what get_encoded_size() promised
TEST_F(BTHomeLocalSensorTest, WriteProducesSizeMatchingGetEncodedSize) {
  source_.state = BATTERY_VALUE;
  BTHomeEncoder encoder;
  local_.write(encoder);
  EXPECT_EQ(encoder.get_size(), local_.get_encoded_size());
}

// Callback registered via register_immediate_callback must fire when publish_state() is called
TEST_F(BTHomeLocalSensorTest, RegisterImmediateCallbackFiresOnPublishState) {
  int call_count = 0;
  local_.register_immediate_callback([&call_count]() { call_count++; });
  source_.publish_state(42.0f);
  EXPECT_EQ(call_count, 1);
}

// Each subsequent publish_state() must fire the callback again
TEST_F(BTHomeLocalSensorTest, RegisterImmediateCallbackFiresOnEachPublishState) {
  int call_count = 0;
  local_.register_immediate_callback([&call_count]() { call_count++; });
  source_.publish_state(42.0f);
  source_.publish_state(50.0f);
  EXPECT_EQ(call_count, 2);
}

// ===========================================================================
// BTHomeLocalBinarySensor — wraps binary_sensor::BinarySensor
//
// Tested behaviors:
//   get_encoded_size()  - returns 0 before any state published, correct size after
//   write()             - encodes type byte + boolean value
//   register_immediate_callback() - callback fires on state change
// ===========================================================================

class BTHomeLocalBinarySensorTest : public ::testing::Test {
 protected:
  static constexpr BTHomeObjectType MOTION_TYPE = BTHomeObjectType::MOTION_DETECTED;
  static constexpr size_t MOTION_ENCODED_SIZE = sizeof(BTHomeHeader) + get_bthome_value_length(MOTION_TYPE);

  binary_sensor::BinarySensor source_;
  BTHomeLocalBinarySensor local_;

  void SetUp() override {
    local_.set_object_type(MOTION_TYPE);
    local_.set_source(&source_);
  }
};

// Before any publish_state(), has_state() returns false → encoded size must be 0
TEST_F(BTHomeLocalBinarySensorTest, GetEncodedSizeReturnsZeroWhenNoState) { EXPECT_EQ(local_.get_encoded_size(), 0u); }

// After publish_state(), encoded size = object type byte + 1 bool byte
TEST_F(BTHomeLocalBinarySensorTest, GetEncodedSizeReturnsCorrectSizeAfterPublish) {
  source_.publish_state(true);
  EXPECT_EQ(local_.get_encoded_size(), MOTION_ENCODED_SIZE);
}

// write() with true state: first byte = type, second byte = 0x01
TEST_F(BTHomeLocalBinarySensorTest, WriteEncodesTrueValue) {
  source_.publish_state(true);
  BTHomeEncoder encoder;
  ASSERT_TRUE(local_.write(encoder));
  EXPECT_EQ(encoder.get_buffer()[0], static_cast<uint8_t>(MOTION_TYPE));
  EXPECT_EQ(encoder.get_buffer()[1], uint8_t{1});
}

// write() with false state: first byte = type, second byte = 0x00
TEST_F(BTHomeLocalBinarySensorTest, WriteEncodesFalseValue) {
  source_.publish_state(false);
  BTHomeEncoder encoder;
  ASSERT_TRUE(local_.write(encoder));
  EXPECT_EQ(encoder.get_buffer()[0], static_cast<uint8_t>(MOTION_TYPE));
  EXPECT_EQ(encoder.get_buffer()[1], uint8_t{0});
}

// Bytes written by write() must match what get_encoded_size() promised
TEST_F(BTHomeLocalBinarySensorTest, WriteProducesSizeMatchingGetEncodedSize) {
  source_.publish_state(true);
  BTHomeEncoder encoder;
  local_.write(encoder);
  EXPECT_EQ(encoder.get_size(), local_.get_encoded_size());
}

// Callback fires on first publish_state() (initial state transition from empty → true)
TEST_F(BTHomeLocalBinarySensorTest, RegisterImmediateCallbackFiresOnPublishState) {
  int call_count = 0;
  local_.register_immediate_callback([&call_count]() { call_count++; });
  source_.publish_state(true);
  EXPECT_EQ(call_count, 1);
}

// Callback fires again when state changes from true → false
TEST_F(BTHomeLocalBinarySensorTest, RegisterImmediateCallbackFiresOnStateChange) {
  int call_count = 0;
  local_.register_immediate_callback([&call_count]() { call_count++; });
  source_.publish_state(true);
  source_.publish_state(false);
  EXPECT_EQ(call_count, 2);
}

// ===========================================================================
// BTHomeLocalTextSensor — wraps text_sensor::TextSensor
//
// Encoding: object_type(1B) + content_length(1B) + content(≤max_length B)
//
// Tested behaviors:
//   get_encoded_size()  - 0 before any state, 1+1+len after publish, truncated at max_length
//   write()             - encodes type + length byte + content bytes
//   register_immediate_callback() - callback fires on every publish_state()
// ===========================================================================

class BTHomeLocalTextSensorTest : public ::testing::Test {
 protected:
  static constexpr BTHomeObjectType TEXT_TYPE = BTHomeObjectType::TEXT;
  static constexpr size_t MAX_LENGTH = 5;

  text_sensor::TextSensor source_;
  BTHomeLocalTextSensor local_;

  void SetUp() override {
    local_.set_object_type(TEXT_TYPE);
    local_.set_source(&source_);
    local_.set_max_length(MAX_LENGTH);
  }
};

// Before any publish_state(), has_state() returns false → encoded size must be 0
TEST_F(BTHomeLocalTextSensorTest, GetEncodedSizeReturnsZeroWhenNoState) { EXPECT_EQ(local_.get_encoded_size(), 0u); }

// Short string: encoded size = type(1) + length_byte(1) + actual string length
TEST_F(BTHomeLocalTextSensorTest, GetEncodedSizeForShortString) {
  source_.publish_state("hi");  // 2 chars < MAX_LENGTH
  EXPECT_EQ(local_.get_encoded_size(), 1u + 1u + 2u);
}

// String longer than max_length: size is clamped to type(1) + length_byte(1) + max_length
TEST_F(BTHomeLocalTextSensorTest, GetEncodedSizeTruncatesToMaxLength) {
  source_.publish_state("toolong");  // 7 chars > MAX_LENGTH
  EXPECT_EQ(local_.get_encoded_size(), 1u + 1u + MAX_LENGTH);
}

// write() encodes type byte, length byte, and exact content for a short string
TEST_F(BTHomeLocalTextSensorTest, WriteEncodesTypeAndContent) {
  source_.publish_state("hello");  // exactly MAX_LENGTH
  BTHomeEncoder encoder;
  ASSERT_TRUE(local_.write(encoder));
  EXPECT_EQ(encoder.get_buffer()[0], static_cast<uint8_t>(TEXT_TYPE));
  EXPECT_EQ(encoder.get_buffer()[1], uint8_t{5});  // length byte
  EXPECT_EQ(encoder.get_buffer()[2], uint8_t{'h'});
  EXPECT_EQ(encoder.get_buffer()[3], uint8_t{'e'});
  EXPECT_EQ(encoder.get_buffer()[4], uint8_t{'l'});
  EXPECT_EQ(encoder.get_buffer()[5], uint8_t{'l'});
  EXPECT_EQ(encoder.get_buffer()[6], uint8_t{'o'});
}

// write() truncates long strings: length byte = max_length, only first max_length chars written
TEST_F(BTHomeLocalTextSensorTest, WriteTruncatesLongString) {
  source_.publish_state("toolong");  // 7 chars, truncated to MAX_LENGTH=5
  BTHomeEncoder encoder;
  ASSERT_TRUE(local_.write(encoder));
  EXPECT_EQ(encoder.get_buffer()[1], uint8_t{5});  // length byte = max_length
  EXPECT_EQ(encoder.get_buffer()[2], uint8_t{'t'});
  EXPECT_EQ(encoder.get_buffer()[3], uint8_t{'o'});
  EXPECT_EQ(encoder.get_buffer()[4], uint8_t{'o'});
  EXPECT_EQ(encoder.get_buffer()[5], uint8_t{'l'});
  EXPECT_EQ(encoder.get_buffer()[6], uint8_t{'o'});
}

// Bytes written by write() must match what get_encoded_size() promised
TEST_F(BTHomeLocalTextSensorTest, WriteProducesSizeMatchingGetEncodedSize) {
  source_.publish_state("hello");
  BTHomeEncoder encoder;
  local_.write(encoder);
  EXPECT_EQ(encoder.get_size(), local_.get_encoded_size());
}

// Callback fires on first publish_state()
TEST_F(BTHomeLocalTextSensorTest, RegisterImmediateCallbackFiresOnPublishState) {
  int call_count = 0;
  local_.register_immediate_callback([&call_count]() { call_count++; });
  source_.publish_state("hello");
  EXPECT_EQ(call_count, 1);
}

// Callback fires on each subsequent publish_state() call
TEST_F(BTHomeLocalTextSensorTest, RegisterImmediateCallbackFiresOnEachPublishState) {
  int call_count = 0;
  local_.register_immediate_callback([&call_count]() { call_count++; });
  source_.publish_state("first");
  source_.publish_state("second");
  EXPECT_EQ(call_count, 2);
}

}  // namespace esphome::bthome::server::testing
