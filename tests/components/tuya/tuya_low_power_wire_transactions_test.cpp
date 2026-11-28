#include <gtest/gtest.h>
#include <cstdint>
#include "esphome/components/tuya/tuya_low_power_wire_transactions.h"

namespace esphome::tuya::testing {

TEST(TuyaLowPowerWireTransactions, QueuedTransactions) {
  using State = esphome::tuya::TuyaLowPowerWireTransactions;
  State t;
  EXPECT_TRUE(t.idle());
  EXPECT_FALSE(t.accepts(0x09, 0));
  EXPECT_FALSE(t.begin(0x03, 10));  // a network notice has no answer to wait for
  EXPECT_TRUE(t.begin(0x09, 100));
  EXPECT_FALSE(t.begin(0x02, 120));   // no overlapping queued network/write transaction
  EXPECT_FALSE(t.complete(0x05, 6));  // report cannot satisfy write ACK
  EXPECT_FALSE(t.complete(0x09, 2));  // malformed ACK
  EXPECT_FALSE(t.complete(0x02, 0));  // wrong command code
  EXPECT_TRUE(t.active() && !t.expire(1599));
  EXPECT_FALSE(t.overdue(1599));
  EXPECT_TRUE(t.overdue(1600));  // Matching ACK arriving before main-loop sweep is late.
  EXPECT_TRUE(t.accepts(0x09, 0));
  EXPECT_TRUE(t.expire(1600));
  EXPECT_TRUE(t.faulted() && !t.complete(0x09, 0));
  t.recovered();
  EXPECT_TRUE(t.begin(0x09, 1700));
  EXPECT_TRUE(t.complete(0x09, 0));
  EXPECT_TRUE(t.idle());

  EXPECT_TRUE(t.begin(0x02, 2000));
  EXPECT_FALSE(t.complete(0x09, 0));  // late CMD09 ACK may not clear CMD02
  EXPECT_TRUE(t.complete(0x02, 0));
  EXPECT_TRUE(t.begin(0x01, 2500));
  EXPECT_FALSE(t.complete(0x01, 0));
  EXPECT_TRUE(t.complete(0x01, 51));
  EXPECT_TRUE(t.begin(0x09, 3000));
  EXPECT_FALSE(t.expire(4499));
  EXPECT_TRUE(t.expire(4500));
  EXPECT_TRUE(t.faulted() && !t.active());
  EXPECT_FALSE(t.begin(0x09, 5000));  // new write cannot consume late old ACK
  EXPECT_FALSE(t.complete(0x09, 0));
  t.recovered();  // The caller asks for the full status separately.
  EXPECT_TRUE(t.begin(0x09, 6000));
  t.transport_fault();
  EXPECT_FALSE(t.begin(0x02, 7000));
  t.recovered();
  EXPECT_TRUE(t.idle());
  EXPECT_TRUE(t.begin(0x01, 0xfffffff0u));
  EXPECT_FALSE(t.overdue(0x20u));
  EXPECT_FALSE(t.expire(0x20u));
  EXPECT_TRUE(t.overdue(static_cast<uint32_t>(0xfffffff0u + 1500u)));
  EXPECT_TRUE(t.expire(static_cast<uint32_t>(0xfffffff0u + 1500u)));
  EXPECT_TRUE(t.faulted());
}

TEST(TuyaLowPowerWireTransactions, VersionThreeAnswers) {
  TuyaLowPowerWireTransactions t;
  // A version-3 write (CMD06) is confirmed by the MCU's CMD07 report, not by a CMD06 frame.
  ASSERT_TRUE(t.begin(0x06, 0));
  EXPECT_FALSE(t.complete(0x06, 0));
  EXPECT_FALSE(t.complete(0x07, 3));  // too short for a datapoint
  EXPECT_TRUE(t.complete(0x07, 5));
  // The working-mode answer of a version-3 MCU may carry two pin bytes.
  ASSERT_TRUE(t.begin(0x02, 100));
  EXPECT_FALSE(t.complete(0x02, 1));
  EXPECT_TRUE(t.complete(0x02, 2));
  ASSERT_TRUE(t.begin(0x02, 200));
  EXPECT_TRUE(t.complete(0x02, 0));
  // A status query (CMD08) is answered by the CMD07 report too, so it cannot be taken for a write's answer.
  ASSERT_TRUE(t.begin(0x08, 250));
  EXPECT_FALSE(t.begin(0x06, 260));
  EXPECT_TRUE(t.complete(0x07, 5));
  // An unanswered CMD06 or CMD08 is dropped without quarantine: a report carries its own values.
  ASSERT_TRUE(t.begin(0x06, 300));
  EXPECT_FALSE(t.abandon_report(300 + TuyaLowPowerWireTransactions::ACK_TIMEOUT_MS - 1));
  EXPECT_TRUE(t.abandon_report(300 + TuyaLowPowerWireTransactions::ACK_TIMEOUT_MS));
  EXPECT_TRUE(t.idle());
  // Other commands still fault, and only they
  ASSERT_TRUE(t.begin(0x09, 2000));
  EXPECT_FALSE(t.abandon_report(2000 + TuyaLowPowerWireTransactions::ACK_TIMEOUT_MS));
  EXPECT_TRUE(t.expire(2000 + TuyaLowPowerWireTransactions::ACK_TIMEOUT_MS));
  EXPECT_TRUE(t.faulted());
}

}  // namespace esphome::tuya::testing
