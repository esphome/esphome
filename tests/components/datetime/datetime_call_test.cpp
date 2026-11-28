#include <gtest/gtest.h>

#include "esphome/components/datetime/datetime_entity.h"

namespace esphome::datetime::testing {

class TestDateTime : public DateTimeEntity {
 public:
  void control(const DateTimeCall &call) override {
    this->year = call.get_year();
    this->month = call.get_month();
    this->day = call.get_day();
    this->hour = call.get_hour();
    this->minute = call.get_minute();
    this->second = call.get_second();
  }
  optional<uint16_t> year;
  optional<uint8_t> month, day, hour, minute, second;
};

TEST(DateTimeCall, SetFieldsReachControl) {
  TestDateTime entity;
  entity.make_call().set_datetime(2026, 11, 28, 0, 0, 59).perform();
  EXPECT_EQ(entity.year, optional<uint16_t>(2026));
  EXPECT_EQ(entity.month, optional<uint8_t>(11));
  EXPECT_EQ(entity.day, optional<uint8_t>(28));
  EXPECT_EQ(entity.hour, optional<uint8_t>(0));  // 0 is a value, not "unset"
  EXPECT_EQ(entity.minute, optional<uint8_t>(0));
  EXPECT_EQ(entity.second, optional<uint8_t>(59));
}

TEST(DateTimeCall, UnsetFieldsStayUnset) {
  TestDateTime entity;
  entity.make_call().set_hour(7).perform();
  EXPECT_FALSE(entity.year.has_value());
  EXPECT_FALSE(entity.month.has_value());
  EXPECT_FALSE(entity.day.has_value());
  EXPECT_EQ(entity.hour, optional<uint8_t>(7));
  EXPECT_FALSE(entity.minute.has_value());
  EXPECT_FALSE(entity.second.has_value());
}

TEST(DateTimeCall, OutOfRangeFieldsAreDropped) {
  TestDateTime entity;
  entity.make_call().set_datetime(1969, 13, 1, 24, 60, 60).perform();
  EXPECT_FALSE(entity.year.has_value());
  EXPECT_FALSE(entity.month.has_value());
  EXPECT_FALSE(entity.day.has_value());
  EXPECT_FALSE(entity.hour.has_value());
  EXPECT_FALSE(entity.minute.has_value());
  EXPECT_FALSE(entity.second.has_value());
}

}  // namespace esphome::datetime::testing
