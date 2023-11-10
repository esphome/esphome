#ifdef PIO_UNIT_TESTING

#include <unity.h>
#include "mopeka_std_check.h"

namespace esphome {
namespace mopeka_std_check {

MopekaStdCheck_Helper obj;

void setUp(void) {
  // set stuff up here
}

void tearDown(void) {
  // clean stuff up here
}

void test_mopeka_std_check_get_lpg_speed_of_sound(void) {
  TEST_ASSERT_EQUAL(1040, obj.get_lpg_speed_of_sound(0.0f));
  TEST_ASSERT_EQUAL(939, obj.get_lpg_speed_of_sound(20.0f));
}

void test_mopeka_std_check_parse_battery_level(void) {
  esphome::mopeka_std_check::mopeka_std_package msg;

  msg.raw_voltage = 0x00;
  TEST_ASSERT_EQUAL(0, obj.parse_battery_level(&msg));

  msg.raw_voltage = 0x5B;
  TEST_ASSERT_EQUAL(1, obj.parse_battery_level(&msg));

  msg.raw_voltage = 0x84;
  TEST_ASSERT_EQUAL(50, obj.parse_battery_level(&msg));

  msg.raw_voltage = 0xAC;
  TEST_ASSERT_EQUAL(99, obj.parse_battery_level(&msg));

  msg.raw_voltage = 0xFF;
  TEST_ASSERT_EQUAL(100, obj.parse_battery_level(&msg));
}

void test_mopeka_std_check_parse_temperature(void) {
  esphome::mopeka_std_check::mopeka_std_package msg;

  msg.raw_temp = 0x00;
  TEST_ASSERT_EQUAL(-40, obj.parse_temperature(&msg));

  msg.raw_temp = 0x01;
  TEST_ASSERT_EQUAL(-42, obj.parse_temperature(&msg));

  msg.raw_temp = 0x25;
  TEST_ASSERT_EQUAL(21, obj.parse_temperature(&msg));

  msg.raw_temp = 0x3F;
  TEST_ASSERT_EQUAL(67, obj.parse_temperature(&msg));
}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_mopeka_std_check_get_lpg_speed_of_sound);
  RUN_TEST(test_mopeka_std_check_parse_battery_level);
  RUN_TEST(test_mopeka_std_check_parse_temperature);
  return UNITY_END();
}

}  // namespace mopeka_std_check
}  // namespace esphome

int main(int argc, char **argv) { esphome::mopeka_std_check::main(argc, argv); }
#endif  // PIO_UNIT_TESTING
