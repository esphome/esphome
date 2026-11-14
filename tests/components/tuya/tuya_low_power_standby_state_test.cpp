#include <gtest/gtest.h>
#include "esphome/components/tuya/tuya_low_power_standby_state.h"

namespace esphome::tuya::testing {

TEST(TuyaLowPowerStandbyState, ResumeActions) {
  using esphome::tuya::TuyaLowPowerStandbyState;
  using State = TuyaLowPowerStandbyState;
  using Action = State::ResumeAction;

  State s;
  EXPECT_FALSE(s.accept_request(false, true, true, true));  // sm=0 => hibernation, not retained standby
  EXPECT_FALSE(s.accept_request(true, false, true, true));  // unconfirmed DP write/status
  EXPECT_FALSE(s.accept_request(true, true, false, true));  // UART transport fault
  EXPECT_FALSE(s.accept_request(true, true, true, false));  // WLAN/API/OTA veto
  EXPECT_TRUE(s.accept_request(true, true, true, true));
  EXPECT_FALSE(s.accept_request(true, true, true, true));  // repeated CMD38 cannot restart request
  EXPECT_TRUE(s.needs_notification());
  EXPECT_FALSE(s.ready_for_pm());
  EXPECT_FALSE(s.cancelled());  // request without CMD12 needs no compensation
  EXPECT_FALSE(s.requested());

  EXPECT_TRUE(s.accept_request(true, true, true, true));
  s.notification_sent();
  EXPECT_FALSE(s.ready_for_pm());
  EXPECT_FALSE(s.cancelled());  // intentional retry after physical UART drain
  EXPECT_TRUE(s.ready_for_pm());
  EXPECT_TRUE(s.notified());
  EXPECT_TRUE(s.cancelled());  // veto after CMD12: must send compensating CMD12/00
  EXPECT_TRUE(!s.requested() && !s.notified());

  EXPECT_TRUE(s.accept_request(true, true, true, true));
  s.notification_sent();
  (void) s.cancelled();
  EXPECT_TRUE(s.resumed(true, true) == Action::SEND_CMD12_WAKE);
  EXPECT_FALSE(s.recovery_required());
  EXPECT_TRUE(s.accept_request(true, true, true, true));  // new handshake after GPIO wake

  s.notification_sent();
  (void) s.cancelled();
  EXPECT_TRUE(s.resumed(true, false) == Action::REQUIRE_RESYNC);  // timer alone never fakes GPIO wake
  EXPECT_TRUE(s.recovery_required());
  EXPECT_FALSE(s.accept_request(true, true, true, true));
  s.reset_on_mcu_boot();
  EXPECT_FALSE(s.recovery_required());
  EXPECT_TRUE(s.accept_request(true, true, true, true));

  s.notification_sent();
  (void) s.cancelled();
  EXPECT_TRUE(s.resumed(false, true) == Action::REQUIRE_RESYNC);
  s.reset_on_mcu_boot();
  EXPECT_TRUE(s.accept_request(true, true, true, true));
  s.transport_fault();
  EXPECT_TRUE(s.recovery_required() && !s.requested());
  EXPECT_FALSE(s.accept_request(true, true, true, true));

  State empty;
  EXPECT_TRUE(empty.resumed(true, true) == Action::NONE);
}

TEST(TuyaLowPowerStandbyState, KeepConsentWhileNetworkStaysConnected) {
  using State = esphome::tuya::TuyaLowPowerStandbyState;
  using Action = State::ResumeAction;

  State s;
  EXPECT_TRUE(s.accept_request(true, true, true, true));
  s.notification_sent();
  (void) s.cancelled();
  EXPECT_TRUE(s.ready_for_pm());

  // Timer or received network data: the MCU still sleeps, sleep again without a new CMD38.
  EXPECT_TRUE(s.resumed(true, false, true) == Action::KEEP_CONSENT);
  EXPECT_TRUE(s.requested() && s.notified() && s.ready_for_pm());
  EXPECT_FALSE(s.recovery_required());
  EXPECT_TRUE(s.resumed(true, false, true) == Action::KEEP_CONSENT);

  // The MCU's own wake still ends the consent with CMD12/00.
  EXPECT_TRUE(s.resumed(true, true, true) == Action::SEND_CMD12_WAKE);
  EXPECT_FALSE(s.requested());

  // An unexpected wake still requires a full resync, even with the network kept up.
  EXPECT_TRUE(s.accept_request(true, true, true, true));
  s.notification_sent();
  (void) s.cancelled();
  EXPECT_TRUE(s.resumed(false, false, true) == Action::REQUIRE_RESYNC);
  EXPECT_TRUE(s.recovery_required());

  // A write from the network ends the consent and needs CMD12/00 as compensation.
  s.reset_on_mcu_boot();
  EXPECT_TRUE(s.accept_request(true, true, true, true));
  s.notification_sent();
  (void) s.cancelled();
  EXPECT_TRUE(s.resumed(true, false, true) == Action::KEEP_CONSENT);
  EXPECT_TRUE(s.force_cancel());
  EXPECT_FALSE(s.requested());
}

TEST(TuyaLowPowerStandbyState, Version3OfferIsNotifiedConsent) {
  using State = esphome::tuya::TuyaLowPowerStandbyState;
  using Action = State::ResumeAction;

  State s;
  EXPECT_FALSE(s.accept_offer(false, true, true));  // MCU transaction still open
  EXPECT_TRUE(s.accept_offer(true, true, true));
  EXPECT_FALSE(s.accept_offer(true, true, true));
  EXPECT_FALSE(s.needs_notification());  // The 0x91 answer is the whole handshake.
  EXPECT_TRUE(s.ready_for_pm());
  EXPECT_EQ(s.resumed(true, true), Action::SEND_CMD12_WAKE);

  EXPECT_TRUE(s.accept_offer(true, true, true));
  EXPECT_TRUE(s.force_cancel());  // Cancelling tells the sleeping MCU that the module is awake.

  s.transport_fault();
  EXPECT_FALSE(s.accept_offer(true, true, true));
  s.recovered();  // A version-3 MCU has no boot heartbeat; a fresh report recovers.
  EXPECT_TRUE(s.accept_offer(true, true, true));
}

}  // namespace esphome::tuya::testing
