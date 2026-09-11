#include <gtest/gtest.h>
#include "esphome/components/bthome/text_sensor/text_sensor.h"
#include "esphome/core/helpers.h"

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

  EXPECT_TRUE(result);
  EXPECT_EQ(text_sensor_.state.size(), sizeof(data));
  EXPECT_EQ(static_cast<uint8_t>(text_sensor_.state[0]), 0xDE);
  EXPECT_EQ(static_cast<uint8_t>(text_sensor_.state[1]), 0xAD);
  EXPECT_EQ(static_cast<uint8_t>(text_sensor_.state[2]), 0xBE);
  EXPECT_EQ(static_cast<uint8_t>(text_sensor_.state[3]), 0xEF);
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
