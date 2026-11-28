#include <gtest/gtest.h>

#include "esphome/components/datetime/datetime_entity.h"

namespace esphome::datetime::testing {

struct Fields {
  optional<uint16_t> year;
  optional<uint8_t> month, day, hour, minute, second;
  bool operator==(const Fields &) const = default;
};

static Fields fields_of(const DateTimeCall &call) {
  return {call.get_year(), call.get_month(), call.get_day(), call.get_hour(), call.get_minute(), call.get_second()};
}

class TestDateTime : public DateTimeEntity {
 public:
  explicit TestDateTime(uint16_t year = 0, uint8_t month = 0) {
    this->year_ = year;
    this->month_ = month;
    this->day_ = 0;
  }
  void control(const DateTimeCall &call) override { this->got = fields_of(call); }
  Fields got;
};

TEST(DateTimeCall, SetFieldsReachControl) {
  TestDateTime entity;
  entity.make_call().set_datetime(2026, 11, 28, 0, 0, 59).perform();
  EXPECT_EQ(entity.got, (Fields{2026, 11, 28, 0, 0, 59}));  // 0 is a value, not "unset"
}

TEST(DateTimeCall, UnsetFieldsStayUnset) {
  TestDateTime entity;
  entity.make_call().set_hour(7).perform();
  EXPECT_EQ(entity.got, (Fields{{}, {}, {}, 7, {}, {}}));
}

struct SingleSetter {
  void (*set)(DateTimeCall &call);
  Fields want;
};

class DateTimeCallSingleSetter : public ::testing::TestWithParam<SingleSetter> {};

// Each setter marks only its own field, for zero and for the type's maximum (checked before validation)
TEST_P(DateTimeCallSingleSetter, SetsOnlyItsField) {
  TestDateTime entity;
  auto call = entity.make_call();
  GetParam().set(call);
  EXPECT_EQ(fields_of(call), GetParam().want);
}

INSTANTIATE_TEST_SUITE_P(
    Cases, DateTimeCallSingleSetter,
    ::testing::Values(SingleSetter{[](DateTimeCall &c) { c.set_year(0); }, {0, {}, {}, {}, {}, {}}},
                      SingleSetter{[](DateTimeCall &c) { c.set_year(65535); }, {65535, {}, {}, {}, {}, {}}},
                      SingleSetter{[](DateTimeCall &c) { c.set_month(0); }, {{}, 0, {}, {}, {}, {}}},
                      SingleSetter{[](DateTimeCall &c) { c.set_month(255); }, {{}, 255, {}, {}, {}, {}}},
                      SingleSetter{[](DateTimeCall &c) { c.set_day(0); }, {{}, {}, 0, {}, {}, {}}},
                      SingleSetter{[](DateTimeCall &c) { c.set_day(255); }, {{}, {}, 255, {}, {}, {}}},
                      SingleSetter{[](DateTimeCall &c) { c.set_hour(0); }, {{}, {}, {}, 0, {}, {}}},
                      SingleSetter{[](DateTimeCall &c) { c.set_hour(255); }, {{}, {}, {}, 255, {}, {}}},
                      SingleSetter{[](DateTimeCall &c) { c.set_minute(0); }, {{}, {}, {}, {}, 0, {}}},
                      SingleSetter{[](DateTimeCall &c) { c.set_minute(255); }, {{}, {}, {}, {}, 255, {}}},
                      SingleSetter{[](DateTimeCall &c) { c.set_second(0); }, {{}, {}, {}, {}, {}, 0}},
                      SingleSetter{[](DateTimeCall &c) { c.set_second(255); }, {{}, {}, {}, {}, {}, 255}}));

// 255 and 65535 are ordinary values: set, then rejected by validation like any other out-of-range value
TEST(DateTimeCall, MaxValuesAreSet) {
  TestDateTime entity;
  auto call = entity.make_call();
  call.set_datetime(65535, 255, 255, 255, 255, 255);
  EXPECT_EQ(fields_of(call), (Fields{65535, 255, 255, 255, 255, 255}));
}

struct Rejection {
  uint16_t year;
  uint8_t month, day, hour, minute, second;
  Fields want;
};

class DateTimeCallRejection : public ::testing::TestWithParam<Rejection> {};

TEST_P(DateTimeCallRejection, DropsTheFieldAndWhatDependsOnIt) {
  const auto &p = GetParam();
  TestDateTime entity;
  entity.make_call().set_datetime(p.year, p.month, p.day, p.hour, p.minute, p.second).perform();
  EXPECT_EQ(entity.got, p.want);
}

INSTANTIATE_TEST_SUITE_P(Cases, DateTimeCallRejection,
                         ::testing::Values(
                             // A bad year drops year, month and day
                             Rejection{1969, 11, 28, 1, 2, 3, {{}, {}, {}, 1, 2, 3}},
                             Rejection{65535, 11, 28, 1, 2, 3, {{}, {}, {}, 1, 2, 3}},
                             // A bad month drops month and day and keeps the year
                             Rejection{2026, 13, 1, 1, 2, 3, {2026, {}, {}, 1, 2, 3}},
                             Rejection{2026, 255, 28, 1, 2, 3, {2026, {}, {}, 1, 2, 3}},
                             // A bad day drops only the day
                             Rejection{2026, 2, 30, 1, 2, 3, {2026, 2, {}, 1, 2, 3}},
                             // A bad time field drops only that field
                             Rejection{2026, 2, 1, 24, 60, 60, {2026, 2, 1, {}, {}, {}}}));

struct PartialDate {
  uint16_t parent_year;
  uint8_t parent_month;
  optional<uint16_t> year;
  optional<uint8_t> month;
  uint8_t day;
  optional<uint8_t> want_day;
};

class DateTimeCallPartialDate : public ::testing::TestWithParam<PartialDate> {};

// A day without its year or month is checked against the entity's current date, and dropped when that is unset (0)
TEST_P(DateTimeCallPartialDate, ValidatesDayAgainstTheCurrentDate) {
  const auto &p = GetParam();
  TestDateTime entity(p.parent_year, p.parent_month);
  auto call = entity.make_call();
  if (p.year.has_value())
    call.set_year(*p.year);
  if (p.month.has_value())
    call.set_month(*p.month);
  call.set_day(p.day).perform();
  EXPECT_EQ(entity.got.day, p.want_day);
}

INSTANTIATE_TEST_SUITE_P(Cases, DateTimeCallPartialDate,
                         ::testing::Values(
                             // No month anywhere, or no year anywhere: the day is dropped
                             PartialDate{0, 0, {}, {}, 15, {}}, PartialDate{2026, 0, {}, {}, 15, {}},
                             PartialDate{0, 2, {}, {}, 15, {}}, PartialDate{2026, 0, 2026, {}, 15, {}},
                             PartialDate{0, 2, {}, 2, 15, {}},
                             // The current year and month fill in what the call leaves out
                             PartialDate{2026, 2, {}, {}, 28, 28}, PartialDate{2026, 2, {}, {}, 30, {}},
                             PartialDate{2026, 0, {}, 2, 28, 28}, PartialDate{2026, 0, {}, 2, 29, {}},
                             PartialDate{0, 2, 2024, {}, 29, 29}));

}  // namespace esphome::datetime::testing
