#include <gtest/gtest.h>

#include "esphome/components/light/color_mode.h"

namespace esphome::light::testing {

// The search to_bit() used before it computed the position from the capability bits
static unsigned search_bit(ColorMode mode) {
  for (int i = 0; i < ColorModeBitPolicy::MAX_BITS; ++i) {
    if (COLOR_MODE_LOOKUP[i] == mode)
      return i;
  }
  return 0;
}

// Every byte value, so values that are not a mode (for example sent over the API) still give bit 0
TEST(ColorModeBits, ToBitMatchesTableSearchForEveryValue) {
  for (unsigned value = 0; value < 256; value++) {
    const auto mode = static_cast<ColorMode>(value);
    EXPECT_EQ(ColorModeBitPolicy::to_bit(mode), search_bit(mode)) << "value " << value;
  }
}

TEST(ColorModeBits, FromBitReversesToBit) {
  for (int bit = 0; bit < ColorModeBitPolicy::MAX_BITS; bit++) {
    const ColorMode mode = ColorModeBitPolicy::from_bit(bit);
    EXPECT_EQ(ColorModeBitPolicy::to_bit(mode), static_cast<unsigned>(bit)) << "bit " << bit;
  }
}

static_assert(ColorModeBitPolicy::to_bit(ColorMode::RGB_COLD_WARM_WHITE) == 9);
static_assert(ColorModeBitPolicy::to_bit(ColorMode::COLOR_TEMPERATURE) == 4);
static_assert(ColorModeBitPolicy::to_bit(ColorMode::UNKNOWN) == 0);

}  // namespace esphome::light::testing
