#include <gtest/gtest.h>
#include <cstddef>
#include <cstdint>
#include "esphome/components/tuya/tuya_low_power.h"
#include "esphome/components/tuya/tuya_low_power_autobaud.h"
#include "esphome/components/tuya/tuya_low_power_product.h"
#include <vector>
#include <string>

namespace esphome::tuya::testing {

TEST(TuyaLowPowerSession, FullStatusAndWrites) {
  using esphome::tuya::TuyaLowPowerSession;
  TuyaLowPowerSession state;
  EXPECT_FALSE(state.ready());
  state.product_received();
  state.begin_full_status(100);

  for (size_t i = 0; i + 1 < TuyaLowPowerSession::FULL_STATUS_DPS.size(); ++i) {
    uint8_t id = TuyaLowPowerSession::FULL_STATUS_DPS[i];
    state.observe_dp(id, 1, &id, 1);
  }
  EXPECT_TRUE(state.busy() && !state.ready());
  uint8_t last = TuyaLowPowerSession::FULL_STATUS_DPS.back();
  state.observe_dp(last, 1, &last, 1);
  EXPECT_TRUE(state.ready());

  const uint8_t wanted[] = {0, 0, 0, 240};
  const uint8_t wrong[] = {0, 0, 0, 210};
  state.begin_write(3, 2, wanted, sizeof(wanted), 200);
  state.write_ack();
  EXPECT_FALSE(state.ready());  // Empty CMD09 ACK is not state confirmation.
  state.observe_dp(3, 2, wrong, sizeof(wrong));
  EXPECT_FALSE(state.ready());
  state.observe_dp(3, 2, wanted, sizeof(wanted));
  EXPECT_TRUE(state.ready());

  state.begin_write(3, 2, wanted, sizeof(wanted), 300);
  state.observe_dp(3, 2, wanted, sizeof(wanted));
  EXPECT_FALSE(state.ready());  // Report may precede the ACK.
  state.write_ack();
  EXPECT_TRUE(state.ready());

  state.begin_write(3, 2, wanted, sizeof(wanted), 400);
  EXPECT_FALSE(state.expire(401, 12000));
  EXPECT_TRUE(state.expire(13000, 12000));
  EXPECT_FALSE(state.ready());  // Failure locks sleep.
  state.begin_full_status(14000);
  for (uint8_t id : TuyaLowPowerSession::FULL_STATUS_DPS)
    state.observe_dp(id, 1, &id, 1);
  EXPECT_TRUE(state.ready());  // Full re-synchronization releases the fault.
  state.reset_status();
  EXPECT_FALSE(state.ready());
  // Autobaud: begin at user-configured speed, alternate every 500 ms.
  using esphome::tuya::TuyaLowPowerAutoBaud;
  TuyaLowPowerAutoBaud scan;
  scan.start(115200);
  EXPECT_TRUE(scan.searching() && scan.probe_due(0) && scan.baud() == 115200);
  scan.probe_sent(10);
  EXPECT_FALSE(scan.probe_due(509));

  auto frame = [](uint8_t command, const std::string &payload) {
    std::vector<uint8_t> b = {
        0x55, 0xAA, 0x00, command, static_cast<uint8_t>(payload.size() >> 8), static_cast<uint8_t>(payload.size())};
    b.insert(b.end(), payload.begin(), payload.end());
    uint8_t sum = 0;
    for (uint8_t x : b)
      sum += x;
    b.push_back(sum);
    return b;
  };
  const std::string product = R"({"p":"ihedujbuli6hsx9b","v":"1.3.1","wk":20,"mwk":28,"sm":1})";
  auto valid = frame(0x01, product);
  auto wrong_command = frame(0x05, product);
  auto bad_checksum = valid;
  bad_checksum.back() ^= 0x01;
  EXPECT_FALSE(scan.accept_frame(wrong_command.data(), wrong_command.size(), 100));
  EXPECT_FALSE(scan.accept_frame(bad_checksum.data(), bad_checksum.size(), 100));
  EXPECT_TRUE(scan.searching());
  EXPECT_TRUE(scan.probe_due(510) && scan.baud() == 9600);
  scan.probe_sent(511);
  EXPECT_FALSE(scan.accept_frame(valid.data(), valid.size(), 1011));  // Expired.
  EXPECT_TRUE(scan.probe_due(1011) && scan.baud() == 115200);
  scan.probe_sent(1012);
  EXPECT_TRUE(scan.accept_frame(valid.data(), valid.size(), 1013));
  EXPECT_TRUE(scan.locked() && !scan.searching() && !scan.probe_due(2000));
  EXPECT_FALSE(scan.accept_frame(valid.data(), valid.size(), 2000));  // No repeat.

  // Other TRV16 MCU firmware answers with protocol version 3; version 1 is still refused.
  auto with_version = [](std::vector<uint8_t> b, uint8_t version) {
    b[2] = version;
    uint8_t sum = 0;
    for (size_t i = 0; i + 1 < b.size(); ++i)
      sum += b[i];
    b.back() = sum;
    return b;
  };
  auto v3 = with_version(valid, 0x03);
  auto v1 = with_version(valid, 0x01);
  TuyaLowPowerAutoBaud scan_v3;
  scan_v3.start(115200);
  scan_v3.probe_sent(10);
  EXPECT_FALSE(scan_v3.accept_frame(v1.data(), v1.size(), 20));
  EXPECT_TRUE(scan_v3.accept_frame(v3.data(), v3.size(), 20));

  // Default to 9600 unless configured to one of the supported candidates.
  scan.start(57600);
  EXPECT_TRUE(scan.baud() == 9600 && scan.probe_due(0));
  scan.probe_sent(0xFFFFFFF0u);
  EXPECT_FALSE(scan.probe_due(0x00000010u));  // millis() wraparound safe.
  EXPECT_TRUE(scan.probe_due(static_cast<uint32_t>(0xFFFFFFF0u + 500u)));
  EXPECT_TRUE(scan.baud() == 115200);
  // The refresh request DP106 never echoes DP106; it returns exactly
  // eight other DPs. An empty CMD09 ACK alone must not release the write.
  state.begin_full_status(15000);
  for (uint8_t id : TuyaLowPowerSession::FULL_STATUS_DPS)
    state.observe_dp(id, 1, &id, 1);
  EXPECT_TRUE(state.ready());
  const uint8_t one = 1;
  state.begin_write(106, 1, &one, 1, 16000);
  state.write_ack();
  EXPECT_TRUE(state.refresh_pending() && !state.ready());
  for (size_t i = 0; i + 1 < TuyaLowPowerSession::REFRESH_DPS.size(); ++i) {
    uint8_t dp = TuyaLowPowerSession::REFRESH_DPS[i];
    state.observe_dp(dp, 1, &dp, 1);
  }
  EXPECT_TRUE(state.write_pending());
  uint8_t end_dp = TuyaLowPowerSession::REFRESH_DPS.back();
  state.observe_dp(end_dp, 1, &end_dp, 1);
  EXPECT_TRUE(state.ready());
  EXPECT_TRUE(state.last_write_result() == TuyaLowPowerSession::WriteResult::CONFIRMED);

  // Report-before-ACK also works for the eight-DP refresh.
  state.begin_write(106, 1, &one, 1, 17000);
  for (uint8_t dp : TuyaLowPowerSession::REFRESH_DPS)
    state.observe_dp(dp, 1, &dp, 1);
  EXPECT_TRUE(state.write_pending());
  state.write_ack();
  EXPECT_TRUE(state.ready());

  // Missing reports are a fault, not an implicit ACK success.
  state.begin_write(106, 1, &one, 1, 18000);
  state.write_ack();
  state.observe_dp(1, 1, &one, 1);
  EXPECT_TRUE(state.expire(30000));
  EXPECT_TRUE(state.last_write_result() == TuyaLowPowerSession::WriteResult::TIMED_OUT);
  EXPECT_FALSE(state.ready());
  state.begin_full_status(31000);
  for (uint8_t dp : TuyaLowPowerSession::FULL_STATUS_DPS)
    state.observe_dp(dp, 1, &dp, 1);
  EXPECT_TRUE(state.ready());

  // The product info names the wake pins and standby-vs-hibernation; product_id restricts the PID.
  using esphome::tuya::TuyaLowPowerProductInfo;
  TuyaLowPowerProductInfo info;
  const std::string product_json = R"({"p":"ihedujbuli6hsx9b","v":"1.3.1","wk":20,"mwk":28,"sm":1})";
  EXPECT_TRUE(info.parse(reinterpret_cast<const uint8_t *>(product_json.data()), product_json.size()));
  EXPECT_TRUE(info.matches("ihedujbuli6hsx9b"));
  EXPECT_TRUE(info.matches(""));
  EXPECT_TRUE(info.wake_gpio == 20 && info.mcu_wake_gpio == 28 && info.sleep_mode == 1);
  EXPECT_EQ(info.mcu_wake_pulse_ms, 50u);  // TRV16 reports no mwkpt: the original WXU default
  const std::string pulse_json = R"({"p":"x","v":"1","wk":20,"mwk":28,"sm":1,"mwkpt":120})";
  EXPECT_TRUE(info.parse(reinterpret_cast<const uint8_t *>(pulse_json.data()), pulse_json.size()));
  EXPECT_EQ(info.mcu_wake_pulse_ms, 120u);
  const std::string short_json = R"({"p":"x","v":"1","wk":20,"mwk":28,"sm":1,"mwkpt":2})";
  EXPECT_TRUE(info.parse(reinterpret_cast<const uint8_t *>(short_json.data()), short_json.size()));
  EXPECT_EQ(info.mcu_wake_pulse_ms, 5u);
  const std::string long_json = R"({"p":"x","v":"1","wk":20,"mwk":28,"sm":1,"mwkpt":250})";
  EXPECT_TRUE(info.parse(reinterpret_cast<const uint8_t *>(long_json.data()), long_json.size()));
  EXPECT_EQ(info.mcu_wake_pulse_ms, 200u);
  // MCU 1.0.9 in the TRV16 (written for a T3-U): the module offers sleep, the wake line idles high.
  const std::string v3_json = R"({"p":"mdthw0oxat1k9sja","v":"1.0.9","m":1,"n":1,"sm":2,"wk":32,"mwk":16})";
  EXPECT_TRUE(info.parse(reinterpret_cast<const uint8_t *>(v3_json.data()), v3_json.size()));
  EXPECT_TRUE(info.wake_gpio == 32 && info.mcu_wake_gpio == 16 && info.sleep_mode == 2);
  EXPECT_FALSE(info.mcu_wake_active_high);
  const std::string high_json = R"({"p":"x","v":"1","wk":20,"mwk":28,"sm":3,"mwkp":1})";
  EXPECT_TRUE(info.parse(reinterpret_cast<const uint8_t *>(high_json.data()), high_json.size()));
  EXPECT_TRUE(info.mcu_wake_active_high);
  EXPECT_EQ(info.sleep_mode, -1);  // Unknown sleep modes are not guessed.
  // Only p and v are required; missing low-power fields stay unset.
  const std::string minimal_json = R"({"p":"abc","v":"2.0.0","m":0})";
  EXPECT_TRUE(info.parse(reinterpret_cast<const uint8_t *>(minimal_json.data()), minimal_json.size()));
  EXPECT_TRUE(info.wake_gpio == -1 && info.mcu_wake_gpio == -1 && info.sleep_mode == -1);
  const std::string no_version = R"({"p":"abc","wk":20,"mwk":28,"sm":1})";
  EXPECT_FALSE(info.parse(reinterpret_cast<const uint8_t *>(no_version.data()), no_version.size()));
  const std::string hib_json = R"({"p":"ihedujbuli6hsx9b","v":"1.3.1","wk":20,"mwk":28,"sm":0})";
  EXPECT_TRUE(info.parse(reinterpret_cast<const uint8_t *>(hib_json.data()), hib_json.size()));
  EXPECT_TRUE(info.matches("ihedujbuli6hsx9b") && info.sleep_mode == 0);
  const std::string wrong_pid = R"({"p":"another_product","v":"1.3.1","wk":20,"mwk":28,"sm":1})";
  EXPECT_TRUE(info.parse(reinterpret_cast<const uint8_t *>(wrong_pid.data()), wrong_pid.size()));
  EXPECT_FALSE(info.matches("ihedujbuli6hsx9b"));
  EXPECT_TRUE(info.matches(""));
  const std::string missing_pin = R"({"p":"ihedujbuli6hsx9b","v":"1.3.1","wk":20,"sm":1})";
  // A missing wake pin no longer rejects the product; it only rules out standby.
  EXPECT_TRUE(info.parse(reinterpret_cast<const uint8_t *>(missing_pin.data()), missing_pin.size()));
  EXPECT_TRUE(info.mcu_wake_gpio == -1 && info.wake_gpio == 20);
  const std::string missing_p = R"({"v":"1.3.1","wk":20,"mwk":28,"sm":1})";
  const std::string missing_v = R"({"p":"ihedujbuli6hsx9b","wk":20,"mwk":28,"sm":1})";
  const std::string bad_sm = R"({"p":"ihedujbuli6hsx9b","v":"1.3.1","wk":20,"mwk":28,"sm":7})";
  const std::string typed_p = R"({"p":42,"v":"1.3.1","wk":20,"mwk":28,"sm":1})";
  EXPECT_FALSE(info.parse(reinterpret_cast<const uint8_t *>(missing_p.data()), missing_p.size()));
  EXPECT_FALSE(info.parse(reinterpret_cast<const uint8_t *>(missing_v.data()), missing_v.size()));
  EXPECT_TRUE(info.parse(reinterpret_cast<const uint8_t *>(bad_sm.data()), bad_sm.size()));
  EXPECT_EQ(info.sleep_mode, -1);  // Unknown sleep mode: no standby.
  EXPECT_FALSE(info.parse(reinterpret_cast<const uint8_t *>(typed_p.data()), typed_p.size()));
  EXPECT_FALSE(info.parse(nullptr, 0));
  EXPECT_FALSE(info.parsed);
}

TEST(TuyaLowPowerSession, Version3McuSynchronizesOnFirstReport) {
  esphome::tuya::TuyaLowPowerSession s;
  s.product_received();
  EXPECT_FALSE(s.ready());
  s.mark_synchronized();
  EXPECT_TRUE(s.synchronized() && s.ready());
  EXPECT_FALSE(s.expire(100000));  // No full-status window was opened, so nothing can time out.
}

}  // namespace esphome::tuya::testing
