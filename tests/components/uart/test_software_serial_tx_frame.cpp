#include <gtest/gtest.h>

#include <cstdint>

#include "esphome/components/uart/software_serial_tx_frame.h"

namespace esphome::uart::testing {

namespace {

constexpr uint32_t US_CYCLES_80MHZ = 80;
constexpr uint32_t MIN_RUN_80MHZ = US_CYCLES_80MHZ * 30;

// Line level of a slot by reading the bits one at a time: start 0, frame LSB first, then high
bool level_at(uint32_t frame, uint32_t num_bits, uint32_t slot) {
  if (slot == 0)
    return false;
  if (slot <= num_bits)
    return (frame >> (slot - 1)) & 1;
  return true;
}

}  // namespace

// Every set slot opens the lock right after an edge into a run that lasts long enough, every such slot up to
// high_from is set, and high_from is the slot of the last 0
TEST(SoftwareSerialTxFrameTest, UnlockSlotsMatchBitByBitReference) {
  for (uint32_t cpu_mhz : {80u, 160u}) {
    const uint32_t min_run = cpu_mhz * 30;
    for (uint32_t baud : {57600u, 76800u, 115200u, 230400u}) {
      const uint32_t bit_time = cpu_mhz * 1000000 / baud;
      for (uint32_t num_bits = 5; num_bits <= 9; num_bits++) {
        for (uint32_t frame = 0; frame < (1u << num_bits); frame++) {
          const SoftwareSerialTxFrame tx = software_serial_tx_frame(frame, num_bits, bit_time, min_run, true);
          uint32_t last_zero = 0;
          for (uint32_t slot = 1; slot <= num_bits; slot++) {
            if (!level_at(frame, num_bits, slot))
              last_zero = slot;
          }
          ASSERT_EQ(tx.high_from, last_zero) << "frame " << frame << " bits " << num_bits;
          for (uint32_t slot = 0; slot <= num_bits + 1; slot++)
            ASSERT_EQ(((tx.line >> slot) & 1) != 0, level_at(frame, num_bits, slot));
          for (uint32_t slot = 0; slot <= tx.high_from; slot++) {
            const bool level = level_at(frame, num_bits, slot);
            const bool edge = slot == 0 || level != level_at(frame, num_bits, slot - 1);
            uint32_t run = 1;
            while (level_at(frame, num_bits, slot + run) == level)
              run++;
            const bool expected = edge && run * bit_time >= min_run;
            ASSERT_EQ(((tx.unlock_at >> slot) & 1) != 0, expected)
                << "frame " << frame << " bits " << num_bits << " baud " << baud << " slot " << slot;
          }
          ASSERT_EQ(tx.unlock_at >> (tx.high_from + 1), 0u);
        }
      }
    }
  }
}

TEST(SoftwareSerialTxFrameTest, NoUnlocksWhenNotRequested) {
  const SoftwareSerialTxFrame tx =
      software_serial_tx_frame(0x00, 8, US_CYCLES_80MHZ * 1000000 / 57600, MIN_RUN_80MHZ, false);
  EXPECT_EQ(tx.unlock_at, 0u);
  EXPECT_EQ(tx.high_from, 8u);
}

// At 57600 a run of two bits is long enough; 0x55 alternates every bit, so its lock never opens
TEST(SoftwareSerialTxFrameTest, Examples57600) {
  const uint32_t bit_time = US_CYCLES_80MHZ * 1000000 / 57600;
  EXPECT_EQ(software_serial_tx_frame(0x55, 8, bit_time, MIN_RUN_80MHZ, true).unlock_at, 0u);
  // 0x00: the start bit begins a run of nine 0s
  EXPECT_EQ(software_serial_tx_frame(0x00, 8, bit_time, MIN_RUN_80MHZ, true).unlock_at, 0b1u);
  // 0xFF: only the start bit is sent before the trailing high run, and it is a run of one
  const SoftwareSerialTxFrame ff = software_serial_tx_frame(0xFF, 8, bit_time, MIN_RUN_80MHZ, true);
  EXPECT_EQ(ff.high_from, 0u);
  EXPECT_EQ(ff.unlock_at, 0u);
  // 'A' (0x41): slots 0 1 0 0 0 0 0 1 0, so a run of five 0s starts at slot 2
  EXPECT_EQ(software_serial_tx_frame(0x41, 8, bit_time, MIN_RUN_80MHZ, true).unlock_at, 1u << 2);
}

}  // namespace esphome::uart::testing
