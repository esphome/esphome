#include <gtest/gtest.h>
#include "esphome/components/bthome/binary_sensor/binary_sensor.h"
#include "esphome/core/helpers.h"

namespace esphome::bthome::testing {

class BTHomeBinarySensorTest : public ::testing::Test {
 protected:
  esphome::bthome::BTHomeBinarySensor binary_sensor_;
};

TEST_F(BTHomeBinarySensorTest, ProcessObjectWithMatchingTypeTrue) {
  // Setup: Set the sensor to listen for BATTERY_LOW
  binary_sensor_.set_object_type(BTHomeObjectType::BATTERY_LOW);

  // Create a BTHomeObject with BATTERY_LOW type and value true
  uint8_t data[] = {0x01};
  BTHomeObject obj{
      .type = BTHomeObjectType::BATTERY_LOW,
      .data = data,
      .length = sizeof(data),
  };

  // Process the object
  bool result = binary_sensor_.process_object(obj);

  // Verify: should have processed successfully
  EXPECT_TRUE(result);
  // Verify: state should be set to true
  EXPECT_TRUE(binary_sensor_.state);
}

TEST_F(BTHomeBinarySensorTest, ProcessObjectWithMatchingTypeFalse) {
  // Setup: Set the sensor to listen for MOTION_DETECTED
  binary_sensor_.set_object_type(BTHomeObjectType::MOTION_DETECTED);

  // Create a BTHomeObject with MOTION_DETECTED type and value false
  uint8_t data[] = {0x00};
  BTHomeObject obj{
      .type = BTHomeObjectType::MOTION_DETECTED,
      .data = data,
      .length = sizeof(data),
  };

  // Process the object
  bool result = binary_sensor_.process_object(obj);

  // Verify: should have processed successfully
  EXPECT_TRUE(result);
  // Verify: state should be set to false
  EXPECT_FALSE(binary_sensor_.state);
}

TEST_F(BTHomeBinarySensorTest, ProcessObjectWithNonMatchingType) {
  // Setup: Set the sensor to listen for BATTERY_LOW
  binary_sensor_.set_object_type(BTHomeObjectType::BATTERY_LOW);

  // Create a BTHomeObject with MOTION_DETECTED type
  uint8_t data[] = {0x01};
  BTHomeObject obj{
      .type = BTHomeObjectType::MOTION_DETECTED,
      .data = data,
      .length = sizeof(data),
  };

  // Process the object
  bool result = binary_sensor_.process_object(obj);

  // Verify: should not have processed (type mismatch)
  EXPECT_FALSE(result);
  // State should remain unchanged (default false)
  EXPECT_FALSE(binary_sensor_.state);
}

TEST_F(BTHomeBinarySensorTest, ProcessMultipleObjectsSameType) {
  // Setup: Set the sensor to listen for MOTION_DETECTED
  binary_sensor_.set_object_type(BTHomeObjectType::MOTION_DETECTED);

  // First object: motion detected (true)
  uint8_t data1[] = {0x01};
  BTHomeObject obj1{
      .type = BTHomeObjectType::MOTION_DETECTED,
      .data = data1,
      .length = sizeof(data1),
  };

  bool result1 = binary_sensor_.process_object(obj1);
  EXPECT_TRUE(result1);
  EXPECT_TRUE(binary_sensor_.state);

  // Second object: no motion (false)
  uint8_t data2[] = {0x00};
  BTHomeObject obj2{
      .type = BTHomeObjectType::MOTION_DETECTED,
      .data = data2,
      .length = sizeof(data2),
  };

  bool result2 = binary_sensor_.process_object(obj2);
  EXPECT_TRUE(result2);
  EXPECT_FALSE(binary_sensor_.state);
}

TEST_F(BTHomeBinarySensorTest, ProcessObjectUpdatesStateWithNewValue) {
  // Setup: Set the sensor to listen for DOOR_OPEN
  binary_sensor_.set_object_type(BTHomeObjectType::DOOR_OPEN);

  // First object: door open (true)
  uint8_t data1[] = {0x01};
  BTHomeObject obj1{
      .type = BTHomeObjectType::DOOR_OPEN,
      .data = data1,
      .length = sizeof(data1),
  };

  bool result1 = binary_sensor_.process_object(obj1);
  EXPECT_TRUE(result1);
  EXPECT_TRUE(binary_sensor_.state);

  // Second object: door closed (false)
  uint8_t data2[] = {0x00};
  BTHomeObject obj2{
      .type = BTHomeObjectType::DOOR_OPEN,
      .data = data2,
      .length = sizeof(data2),
  };

  bool result2 = binary_sensor_.process_object(obj2);
  EXPECT_TRUE(result2);
  // Verify: state should be updated to the new value
  EXPECT_FALSE(binary_sensor_.state);
}

}  // namespace esphome::bthome::testing
