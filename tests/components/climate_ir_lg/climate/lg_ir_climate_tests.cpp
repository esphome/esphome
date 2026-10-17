#include <gtest/gtest.h>
#include "../common.h"

namespace esphome::climate_ir_lg::testing {

// The swing command is a distinct, self-contained IR code: the real remote transmits it
// on its own, independent of the unit's current mode, target temperature, or fan speed.
TEST(LgIrClimateTests, SwingCommandIsIndependentOfClimateState) {
  constexpr uint32_t bit_one_low = 1600;

  LgIrClimate sut;
  MockRemoteTransmitter transmitter;
  sut.set_transmitter(&transmitter);
  sut.set_header_high(8000);
  sut.set_header_low(4000);
  sut.set_bit_high(600);
  sut.set_bit_one_low(bit_one_low);
  sut.set_bit_zero_low(550);

  // Put the unit in a distinctive state (COOL, non-default temperature and fan speed) to prove
  // the swing command below is transmitted independently of it.
  climate::ClimateCall on_call(&sut);
  on_call.set_mode(climate::CLIMATE_MODE_COOL);
  on_call.set_target_temperature(20);
  on_call.set_fan_mode(climate::CLIMATE_FAN_HIGH);
  sut.control(on_call);

  // Toggle swing -- this must be the fixed swing code below, unaffected by the state set above.
  climate::ClimateCall swing_call(&sut);
  swing_call.set_swing_mode(climate::CLIMATE_SWING_VERTICAL);
  sut.control(swing_call);

  // A complete LG frame is header (2) + 28 bit pairs (56) + trailing mark (1).
  // Guard against out-of-bounds reads in decode_lg_frame() if the transmission failed.
  ASSERT_GE(transmitter.last_data.size(), 2u + 28u * 2u + 1u);

  uint32_t transmitted = decode_lg_frame(transmitter.last_data, bit_one_low);

  // The swing command is always this exact fixed code, regardless of climate state.
  EXPECT_EQ(transmitted, 0x8810001u);
}

}  // namespace esphome::climate_ir_lg::testing
