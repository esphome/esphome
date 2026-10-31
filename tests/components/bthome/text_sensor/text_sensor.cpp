#include <gtest/gtest.h>
#include "esphome/components/bthome/text_sensor/text_sensor.h"
#include "esphome/core/helpers.h"

#include <span>
#include <string>
#include <vector>

namespace esphome::bthome::testing {
using namespace esphome::bthome::client;

class BTHomeTextSensorTest : public ::testing::Test {
 protected:
  BTHomeTextSensor text_sensor_;
};

TEST_F(BTHomeTextSensorTest, ProcessObjectWithMatchingTypeText) {
  text_sensor_.set_object_type(BTHomeObjectType::TEXT);

  const char *content = "hello";
  BTHomeObject obj{
      .type = BTHomeObjectType::TEXT,
      .data = reinterpret_cast<const uint8_t *>(content),
      .length = 5,
  };

  bool result = text_sensor_.process_object(obj);

  EXPECT_TRUE(result);
  EXPECT_EQ(text_sensor_.state, "hello");
}

TEST_F(BTHomeTextSensorTest, ProcessObjectWithMatchingTypeRaw) {
  text_sensor_.set_object_type(BTHomeObjectType::RAW);

  const uint8_t data[] = {0xDE, 0xAD, 0xBE, 0xEF};
  BTHomeObject obj{
      .type = BTHomeObjectType::RAW,
      .data = data,
      .length = sizeof(data),
  };

  bool result = text_sensor_.process_object(obj);

  // Published as lowercase hex, so the state is always valid text
  EXPECT_TRUE(result);
  EXPECT_EQ(text_sensor_.state, "deadbeef");
}

TEST_F(BTHomeTextSensorTest, RawDataCallbackGetsOriginalBytes) {
  text_sensor_.set_object_type(BTHomeObjectType::RAW);
  std::vector<uint8_t> received;
  text_sensor_.add_on_raw_data_callback(
      [&received](std::span<const uint8_t> bytes) { received.assign(bytes.begin(), bytes.end()); });

  const uint8_t data[] = {0x00, 0xFF, 0x10};
  BTHomeObject obj{
      .type = BTHomeObjectType::RAW,
      .data = data,
      .length = sizeof(data),
  };

  EXPECT_TRUE(text_sensor_.process_object(obj));
  EXPECT_EQ(received, std::vector<uint8_t>(std::begin(data), std::end(data)));
  EXPECT_EQ(text_sensor_.state, "00ff10");
}

static bool process_text(BTHomeTextSensor &sensor, const std::vector<uint8_t> &bytes) {
  BTHomeObject obj{
      .type = BTHomeObjectType::TEXT,
      .data = bytes.data(),
      .length = bytes.size(),
  };
  return sensor.process_object(obj);
}

TEST_F(BTHomeTextSensorTest, MultiByteUtf8TextIsPublished) {
  text_sensor_.set_object_type(BTHomeObjectType::TEXT);
  const std::string text = "Gr\xC3\xBC\xC3\x9F"
                           "e \xE2\x82\xAC \xF0\x9F\x8C\xA1";  // "Grüße € 🌡"

  EXPECT_TRUE(process_text(text_sensor_, std::vector<uint8_t>(text.begin(), text.end())));
  EXPECT_EQ(text_sensor_.state, text);
}

TEST_F(BTHomeTextSensorTest, InvalidUtf8TextIsIgnored) {
  text_sensor_.set_object_type(BTHomeObjectType::TEXT);
  const std::vector<std::vector<uint8_t>> invalid = {
      {0xDE, 0xAD, 0xBE, 0xEF},  // stray continuation and lead bytes
      {0xC3, 0x28},              // lead byte followed by a non-continuation byte
      {0xE2, 0x82},              // truncated sequence
      {0xC0, 0xAF},              // overlong encoding of '/'
      {0xED, 0xA0, 0x80},        // UTF-16 surrogate
      {0xF4, 0x90, 0x80, 0x80},  // above U+10FFFF
  };
  for (const auto &bytes : invalid) {
    EXPECT_TRUE(process_text(text_sensor_, bytes));
    EXPECT_FALSE(text_sensor_.has_state());
  }
}

TEST_F(BTHomeTextSensorTest, ProcessObjectWithNonMatchingType) {
  text_sensor_.set_object_type(BTHomeObjectType::TEXT);

  const char *content = "hi";
  BTHomeObject obj{
      .type = BTHomeObjectType::RAW,
      .data = reinterpret_cast<const uint8_t *>(content),
      .length = 2,
  };

  bool result = text_sensor_.process_object(obj);

  EXPECT_FALSE(result);
  // State should remain empty (uninitialized)
  EXPECT_TRUE(text_sensor_.state.empty());
}

TEST_F(BTHomeTextSensorTest, ProcessObjectEmptyString) {
  text_sensor_.set_object_type(BTHomeObjectType::TEXT);

  BTHomeObject obj{
      .type = BTHomeObjectType::TEXT,
      .data = nullptr,
      .length = 0,
  };

  bool result = text_sensor_.process_object(obj);

  EXPECT_TRUE(result);
  EXPECT_EQ(text_sensor_.state, "");
}

TEST_F(BTHomeTextSensorTest, ProcessObjectUpdatesStateWithNewValue) {
  text_sensor_.set_object_type(BTHomeObjectType::TEXT);

  const char *content1 = "first";
  BTHomeObject obj1{
      .type = BTHomeObjectType::TEXT,
      .data = reinterpret_cast<const uint8_t *>(content1),
      .length = 5,
  };

  bool result1 = text_sensor_.process_object(obj1);
  EXPECT_TRUE(result1);
  EXPECT_EQ(text_sensor_.state, "first");

  const char *content2 = "second";
  BTHomeObject obj2{
      .type = BTHomeObjectType::TEXT,
      .data = reinterpret_cast<const uint8_t *>(content2),
      .length = 6,
  };

  bool result2 = text_sensor_.process_object(obj2);
  EXPECT_TRUE(result2);
  EXPECT_EQ(text_sensor_.state, "second");
}

}  // namespace esphome::bthome::testing
