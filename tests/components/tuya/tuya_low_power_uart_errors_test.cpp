#include <gtest/gtest.h>
#include <cstdint>
#include "esphome/components/tuya/tuya_low_power_uart_errors.h"
#include "esphome/components/tuya/tuya_low_power.h"
#include "esphome/components/tuya/tuya_low_power_wire_transactions.h"

namespace esphome::tuya::testing {

TEST(TuyaLowPowerUartErrors, IgnoresProbeErrorsButInvalidatesAfterLock) {
  TuyaLowPowerUartErrors guard;
  guard.reset(7);
  EXPECT_TRUE(guard.unchanged(7));
  EXPECT_FALSE(guard.observe(10, true));  // Wrong-baud framing errors.
  EXPECT_TRUE(guard.unchanged(10));
  EXPECT_FALSE(guard.observe(10, false));  // No new error on lock.
  EXPECT_TRUE(guard.observe(11, false));   // New post-lock loss is relevant.
  EXPECT_FALSE(guard.unchanged(12));       // Sleep veto before loop handles loss.
  EXPECT_FALSE(guard.observe(12, true));   // New probing epoch can ignore errors.
  EXPECT_TRUE(guard.unchanged(12));
}

TEST(TuyaLowPowerUartErrors, ErrorForcesFullStatusAndWireQuarantine) {
  TuyaLowPowerUartErrors guard;
  TuyaLowPowerSession session;
  TuyaLowPowerWireTransactions wire;
  guard.reset(0);
  session.product_received();
  session.begin_full_status(100);
  for (uint8_t id : TuyaLowPowerSession::FULL_STATUS_DPS)
    session.observe_dp(id, 1, &id, 1);
  EXPECT_TRUE(session.ready());
  EXPECT_TRUE(wire.begin(0x09, 200));
  EXPECT_TRUE(guard.observe(1, false));
  session.command_timeout();
  wire.transport_fault();
  EXPECT_FALSE(session.ready());
  EXPECT_TRUE(wire.faulted());
  EXPECT_FALSE(wire.begin(0x09, 201));  // Never replay an ambiguous write.
  wire.recovered();                     // Only after verified CMD01 identity.
  EXPECT_FALSE(session.ready());
  session.begin_full_status(300);  // CMD02/04 needs all 17 DP reports.
  for (size_t i = 0; i + 1 < TuyaLowPowerSession::FULL_STATUS_DPS.size(); ++i) {
    uint8_t id = TuyaLowPowerSession::FULL_STATUS_DPS[i];
    session.observe_dp(id, 1, &id, 1);
  }
  EXPECT_FALSE(session.ready());
  uint8_t last = TuyaLowPowerSession::FULL_STATUS_DPS.back();
  session.observe_dp(last, 1, &last, 1);
  EXPECT_TRUE(session.ready());
}

}  // namespace esphome::tuya::testing
