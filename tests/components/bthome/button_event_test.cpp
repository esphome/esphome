#include <gtest/gtest.h>

#include "esphome/components/bthome/button_event.h"

namespace esphome::bthome {

TEST(BTHomeButton, ReadsTheButtonAtIndexAndMapsTheHoldAlias) {
  codec::Parsed parsed{};
  parsed.button_count = 2;
  parsed.buttons[0] = 0x00;
  parsed.buttons[1] = BUTTON_HOLD_ALIAS;

  EXPECT_EQ(button_event_at(parsed, 1), 0x00);
  EXPECT_EQ(button_event_at(parsed, 2), BUTTON_HOLD);
  EXPECT_EQ(button_event_at(parsed, 3), 0x00);
  EXPECT_EQ(button_event_at(parsed, 0), 0x00);

  parsed.buttons[0] = 0x01;
  EXPECT_EQ(button_event_at(parsed, 1), 0x01);

  parsed.button_count = codec::MAX_BUTTONS;
  parsed.buttons[codec::MAX_BUTTONS - 1] = BUTTON_HOLD;
  EXPECT_EQ(button_event_at(parsed, codec::MAX_BUTTONS), BUTTON_HOLD);
  EXPECT_EQ(button_event_at(parsed, codec::MAX_BUTTONS + 1), 0x00);
}

TEST(BTHomeButton, DedupsByPacketIdAndByCooldown) {
  PacketDedup dedup;
  EXPECT_TRUE(dedup.accept(true, 1, 1000));
  EXPECT_FALSE(dedup.accept(true, 1, 1100));
  EXPECT_TRUE(dedup.accept(true, 2, 1100));

  PacketDedup bare;
  EXPECT_TRUE(bare.accept(false, 0, 100));
  EXPECT_FALSE(bare.accept(false, 0, 100 + NO_PACKET_COOLDOWN_MS - 1));
  EXPECT_TRUE(bare.accept(false, 0, 100 + NO_PACKET_COOLDOWN_MS));

  // uint32 wrap. 32 ms later is still inside the window.
  PacketDedup wrapped;
  ASSERT_TRUE(wrapped.accept(false, 0, 0xFFFFFFF0u));
  EXPECT_FALSE(wrapped.accept(false, 0, 0x10u));
  EXPECT_TRUE(wrapped.accept(false, 0, 0xFFFFFFF0u + NO_PACKET_COOLDOWN_MS));
}

}  // namespace esphome::bthome
