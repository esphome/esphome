#include <gtest/gtest.h>
#include <cstdint>
#include <cstring>
#include <string>
#include "esphome/components/tuya/tuya_low_power.h"
#include "esphome/components/tuya/tuya_low_power_product.h"

namespace esphome::tuya::testing {

using Event = TuyaLowPowerSession::Event;

TEST(TuyaLowPowerSession, StatusSeriesEndsWhenTheMcuIsQuiet) {
  TuyaLowPowerSession s;
  EXPECT_FALSE(s.ready());
  s.product_received();
  s.begin_full_status(100);
  EXPECT_EQ(s.update(5000), Event::EVENT_NONE);  // No report yet: keep waiting.
  s.observe_report(5000);
  s.observe_report(5200);
  EXPECT_EQ(s.update(5600), Event::EVENT_NONE);  // Still within the quiet time of the last report.
  EXPECT_TRUE(s.busy() && !s.ready());
  EXPECT_EQ(s.update(5700), Event::EVENT_SYNCHRONIZED);
  EXPECT_TRUE(s.ready());
  EXPECT_EQ(s.update(9000), Event::EVENT_NONE);
}

TEST(TuyaLowPowerSession, StatusSeriesTimesOut) {
  TuyaLowPowerSession s;
  s.product_received();
  s.begin_full_status(0xFFFFF000u);  // Across the millis() wrap
  EXPECT_EQ(s.update(0xFFFFF000u + TuyaLowPowerSession::REPORT_TIMEOUT_MS - 1), Event::EVENT_NONE);
  EXPECT_EQ(s.update(0xFFFFF000u + TuyaLowPowerSession::REPORT_TIMEOUT_MS), Event::EVENT_TIMED_OUT);
  EXPECT_TRUE(s.faulted() && !s.ready());
  s.begin_full_status(20000);
  s.observe_report(20100);
  EXPECT_EQ(s.update(20600), Event::EVENT_SYNCHRONIZED);
  EXPECT_TRUE(s.ready() && !s.faulted());  // A full status clears the fault.
}

TEST(TuyaLowPowerSession, WriteEndsAfterAckAndReports) {
  TuyaLowPowerSession s;
  s.product_received();
  s.mark_synchronized();
  s.begin_write(1000);
  s.observe_report(1100);  // A report may come before the ACK.
  EXPECT_EQ(s.update(2000), Event::EVENT_NONE);
  EXPECT_FALSE(s.ready());  // No ACK yet
  s.write_ack(2000);
  s.observe_report(2300);
  EXPECT_EQ(s.update(2700), Event::EVENT_NONE);
  EXPECT_FALSE(s.ready());
  EXPECT_EQ(s.update(2800), Event::EVENT_NONE);
  EXPECT_TRUE(s.ready());

  s.begin_write(3000);  // Never acknowledged
  EXPECT_EQ(s.update(3000 + TuyaLowPowerSession::REPORT_TIMEOUT_MS), Event::EVENT_TIMED_OUT);
  EXPECT_TRUE(s.faulted() && !s.ready());
  s.write_ack(16000);  // A late ACK does not revive the write.
  EXPECT_FALSE(s.busy());
}

TEST(TuyaLowPowerSession, ResetAndCommandTimeout) {
  TuyaLowPowerSession s;
  s.product_received();
  s.mark_synchronized();
  EXPECT_TRUE(s.ready());
  s.reset_status();
  EXPECT_FALSE(s.ready() || s.busy() || s.faulted());
  s.mark_synchronized();
  s.begin_write(0);
  s.command_timeout();
  EXPECT_TRUE(s.faulted() && !s.busy() && !s.ready());
}

TEST(TuyaLowPowerProductInfo, Parse) {
  // Only p and v are required; mwkpt sets the wake pulse length.
  using esphome::tuya::TuyaLowPowerProductInfo;
  TuyaLowPowerProductInfo info;
  const std::string product_json = R"({"p":"ihedujbuli6hsx9b","v":"1.3.1","wk":20,"mwk":28,"sm":1})";
  EXPECT_TRUE(info.parse(reinterpret_cast<const uint8_t *>(product_json.data()), product_json.size()));
  EXPECT_STREQ(info.pid.data(), "ihedujbuli6hsx9b");
  EXPECT_STREQ(info.version.data(), "1.3.1");
  EXPECT_EQ(info.mcu_wake_pulse_ms, 50u);  // No mwkpt: the default of the original module firmware
  const std::string pulse_json = R"({"p":"x","v":"1","mwkpt":120})";
  EXPECT_TRUE(info.parse(reinterpret_cast<const uint8_t *>(pulse_json.data()), pulse_json.size()));
  EXPECT_EQ(info.mcu_wake_pulse_ms, 120u);
  const std::string short_json = R"({"p":"x","v":"1","mwkpt":2})";
  EXPECT_TRUE(info.parse(reinterpret_cast<const uint8_t *>(short_json.data()), short_json.size()));
  EXPECT_EQ(info.mcu_wake_pulse_ms, 5u);
  const std::string long_json = R"({"p":"x","v":"1","mwkpt":250})";
  EXPECT_TRUE(info.parse(reinterpret_cast<const uint8_t *>(long_json.data()), long_json.size()));
  EXPECT_EQ(info.mcu_wake_pulse_ms, 200u);
  const std::string v3_json = R"({"p":"mdthw0oxat1k9sja","v":"1.0.9","m":1,"n":1,"sm":2,"wk":32,"mwk":16})";
  EXPECT_TRUE(info.parse(reinterpret_cast<const uint8_t *>(v3_json.data()), v3_json.size()));
  EXPECT_STREQ(info.version.data(), "1.0.9");
  const std::string minimal_json = R"({"p":"abc","v":"2.0.0","m":0})";
  EXPECT_TRUE(info.parse(reinterpret_cast<const uint8_t *>(minimal_json.data()), minimal_json.size()));
  const std::string missing_p = R"({"v":"1.3.1","wk":20})";
  const std::string missing_v = R"({"p":"ihedujbuli6hsx9b","wk":20})";
  const std::string typed_p = R"({"p":42,"v":"1.3.1","wk":20})";
  EXPECT_FALSE(info.parse(reinterpret_cast<const uint8_t *>(missing_p.data()), missing_p.size()));
  EXPECT_FALSE(info.parse(reinterpret_cast<const uint8_t *>(missing_v.data()), missing_v.size()));
  EXPECT_FALSE(info.parse(reinterpret_cast<const uint8_t *>(typed_p.data()), typed_p.size()));
  EXPECT_FALSE(info.parse(nullptr, 0));
  EXPECT_FALSE(info.parsed);
  // A product field holds at most 48 characters.
  const std::string max_p = R"({"p":")" + std::string(48, 'a') + R"(","v":"1"})";
  const std::string long_p = R"({"p":")" + std::string(49, 'a') + R"(","v":"1"})";
  EXPECT_TRUE(info.parse(reinterpret_cast<const uint8_t *>(max_p.data()), max_p.size()));
  EXPECT_EQ(std::strlen(info.pid.data()), 48u);
  EXPECT_FALSE(info.parse(reinterpret_cast<const uint8_t *>(long_p.data()), long_p.size()));
  EXPECT_STREQ(info.pid.data(), "");
}

}  // namespace esphome::tuya::testing
