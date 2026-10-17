#include <gtest/gtest.h>

#include "esphome/components/bthome/button_event.h"

namespace esphome::bthome::testing {

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

TEST(BTHomeButton, DropsARepeatedPacketIdUntilTheTransmitterWasSilent) {
  PacketDedup dedup;
  EXPECT_TRUE(dedup.new_packet(1, 1000));
  EXPECT_FALSE(dedup.new_packet(1, 1100));
  EXPECT_TRUE(dedup.new_packet(2, 1200));

  // Each copy is less than PACKET_ID_RESET_MS after the previous one, so all are repeats.
  EXPECT_FALSE(dedup.new_packet(2, 1200 + PACKET_ID_RESET_MS - 1));
  EXPECT_FALSE(dedup.new_packet(2, 1200 + 2 * PACKET_ID_RESET_MS - 2));

  // A restarted transmitter may send the same id again after a pause.
  EXPECT_TRUE(dedup.new_packet(2, 1200 + 3 * PACKET_ID_RESET_MS));

  // uint32 wrap of the clock.
  PacketDedup wrapped;
  ASSERT_TRUE(wrapped.new_packet(9, 0xFFFFFFF0u));
  EXPECT_FALSE(wrapped.new_packet(9, 0x10u));
}

TEST(BTHomeButton, DropsGesturesWithoutPacketIdInsideTheCooldown) {
  PacketDedup bare;
  EXPECT_TRUE(bare.new_gesture(100));
  EXPECT_FALSE(bare.new_gesture(100 + NO_PACKET_COOLDOWN_MS - 1));
  EXPECT_TRUE(bare.new_gesture(100 + NO_PACKET_COOLDOWN_MS));

  // uint32 wrap. 32 ms later is still inside the window.
  PacketDedup wrapped;
  ASSERT_TRUE(wrapped.new_gesture(0xFFFFFFF0u));
  EXPECT_FALSE(wrapped.new_gesture(0x10u));
  EXPECT_TRUE(wrapped.new_gesture(0xFFFFFFF0u + NO_PACKET_COOLDOWN_MS));
}

}  // namespace esphome::bthome::testing
