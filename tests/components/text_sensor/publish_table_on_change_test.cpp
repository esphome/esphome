#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/core/progmem.h"

namespace esphome::text_sensor::testing {

// A two-bit field from index 0, then a three-bit field from index 4 with "Unknown" for undefined codes
PROGMEM_STRING_TABLE(FieldStrings, "Off", "Low", "High", "Full", "Normal", "Warm", "Cool", "Unknown");

class PublishTableOnChangeTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->sensor_.add_on_state_callback([this](const std::string &state) { this->seen_.push_back(state); });
  }

  TextSensor sensor_;
  std::vector<std::string> seen_;
  uint8_t last_{0xFF};
};

TEST_F(PublishTableOnChangeTest, PublishesTheFirstValueThenOnlyChanges) {
  for (uint8_t value : {0, 0, 2, 2, 1}) {
    publish_table_on_change<FieldStrings, 0x03, FieldStrings::LAST_INDEX>(&this->sensor_, this->last_, value, 0);
  }
  EXPECT_EQ(this->seen_, (std::vector<std::string>{"Off", "High", "Low"}));
  EXPECT_EQ(this->last_, 1);
}

TEST_F(PublishTableOnChangeTest, IgnoresBitsOutsideTheMask) {
  for (uint8_t value : {0x03, 0xFF, 0x07, 0x01}) {
    publish_table_on_change<FieldStrings, 0x03, FieldStrings::LAST_INDEX>(&this->sensor_, this->last_, value, 0);
  }
  EXPECT_EQ(this->seen_, (std::vector<std::string>{"Full", "Low"}));
}

TEST_F(PublishTableOnChangeTest, OffsetsByFirstAndFallsBackPastTheEnd) {
  for (uint8_t value : {0, 2, 5, 7, 6}) {
    publish_table_on_change<FieldStrings, 0x07, 7>(&this->sensor_, this->last_, value, 4);
  }
  // Codes 4 and up land past the table and publish the fallback; 7 and 6 differ, so both publish
  EXPECT_EQ(this->seen_, (std::vector<std::string>{"Normal", "Cool", "Unknown", "Unknown", "Unknown"}));
}

}  // namespace esphome::text_sensor::testing
