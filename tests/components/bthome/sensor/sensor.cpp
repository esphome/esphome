#include <gtest/gtest.h>
#include <cmath>
#include "esphome/components/bthome/sensor/sensor.h"
#include "esphome/core/helpers.h"

namespace esphome::bthome::testing {
using namespace esphome::bthome::client;

class BTHomeSensorTest : public ::testing::Test {
 protected:
  BTHomeSensor sensor_;
};

TEST_F(BTHomeSensorTest, ProcessObjectWithMatchingType) {
  // Setup: Set the sensor to listen for BATTERY_PCT
  sensor_.set_object_type(BTHomeObjectType::BATTERY_PCT);

  // Create a BTHomeObject with BATTERY_PCT type and value 97
  uint8_t data[] = {0x61};
  BTHomeObject obj{
      .type = BTHomeObjectType::BATTERY_PCT,
      .data = data,
      .length = sizeof(data),
  };

  // Process the object
  bool result = sensor_.process_object(obj);

  // Verify: should have processed successfully
  EXPECT_TRUE(result);
  // Verify: state should be set to 97.0f
  EXPECT_NEAR(sensor_.state, 97.0f, 0.001f);
}

TEST_F(BTHomeSensorTest, ProcessObjectWithNonMatchingType) {
  // Setup: Set the sensor to listen for BATTERY_PCT
  sensor_.set_object_type(BTHomeObjectType::BATTERY_PCT);

  // Create a BTHomeObject with TEMPERATURE_C_E2 type
  uint8_t data[] = {0xCA, 0x09};
  BTHomeObject obj{
      .type = BTHomeObjectType::TEMPERATURE_C_E2,
      .data = data,
      .length = sizeof(data),
  };

  // Process the object
  bool result = sensor_.process_object(obj);

  // Verify: should not have processed (type mismatch)
  EXPECT_FALSE(result);
  // State should remain NAN (uninitialized)
  EXPECT_TRUE(std::isnan(sensor_.state));
}

TEST_F(BTHomeSensorTest, ProcessObjectUpdatesStateWithNewValue) {
  // Setup: Set the sensor to listen for BATTERY_PCT
  sensor_.set_object_type(BTHomeObjectType::BATTERY_PCT);

  // First object: battery at 97%
  uint8_t data1[] = {0x61};
  BTHomeObject obj1{
      .type = BTHomeObjectType::BATTERY_PCT,
      .data = data1,
      .length = sizeof(data1),
  };

  bool result1 = sensor_.process_object(obj1);
  EXPECT_TRUE(result1);
  EXPECT_NEAR(sensor_.state, 97.0f, 0.001f);

  // Second object: battery at 50%
  uint8_t data2[] = {0x32};
  BTHomeObject obj2{
      .type = BTHomeObjectType::BATTERY_PCT,
      .data = data2,
      .length = sizeof(data2),
  };

  bool result2 = sensor_.process_object(obj2);
  EXPECT_TRUE(result2);
  // Verify: state should be updated to the new value
  EXPECT_NEAR(sensor_.state, 50.0f, 0.001f);
}

}  // namespace esphome::bthome::testing
