#include <gtest/gtest.h>
#include "../common.h"

namespace esphome::climate_ir_lg::testing {

namespace {

constexpr uint32_t BIT_ONE_LOW = 1600;
// A complete LG frame is header (2) + 28 bit pairs (56) + trailing mark (1)
constexpr size_t FRAME_SIZE = 2 + 28 * 2 + 1;

// A climate with timings set, so transmit_() produces a frame decode_lg_frame() can read back
void configure(LgIrClimate &sut, MockRemoteTransmitter &transmitter) {
  sut.set_transmitter(&transmitter);
  sut.set_header_high(8000);
  sut.set_header_low(4000);
  sut.set_bit_high(600);
  sut.set_bit_one_low(BIT_ONE_LOW);
  sut.set_bit_zero_low(550);
}

// COOL at 20 °C with the fan on HIGH: a distinctive state the swing tests must not leak into their frame
void turn_on(LgIrClimate &sut) {
  climate::ClimateCall on_call(&sut);
  on_call.set_mode(climate::CLIMATE_MODE_COOL);
  on_call.set_target_temperature(20);
  on_call.set_fan_mode(climate::CLIMATE_FAN_HIGH);
  sut.control(on_call);
}

uint32_t last_frame(const MockRemoteTransmitter &transmitter) {
  // Guard against out-of-bounds reads in decode_lg_frame() if the transmission failed
  if (transmitter.last_data.size() < FRAME_SIZE) {
    ADD_FAILURE() << "no complete frame was transmitted";
    return 0;
  }
  return decode_lg_frame(transmitter.last_data, BIT_ONE_LOW);
}

}  // namespace

// A mode command carries the mode, the fan speed and the temperature, with the checksum nibble
TEST(LgIrClimateTests, ModeCommandCarriesModeFanSpeedAndTemperature) {
  LgIrClimate sut;
  MockRemoteTransmitter transmitter;
  configure(sut, transmitter);

  turn_on(sut);

  // From OFF: COMMAND_ON_COOL, FAN_MAX, 20 °C encoded as (20 - 15) << 8, checksum 9
  EXPECT_EQ(last_frame(transmitter), 0x8800549u);
}

// The swing command is a distinct, self-contained IR code: the real remote transmits it
// on its own, independent of the unit's current mode, target temperature, or fan speed.
TEST(LgIrClimateTests, SwingCommandIsIndependentOfClimateState) {
  LgIrClimate sut;
  MockRemoteTransmitter transmitter;
  configure(sut, transmitter);
  turn_on(sut);

  // Toggle swing -- this must be the fixed swing code below, unaffected by the state set above.
  climate::ClimateCall swing_call(&sut);
  swing_call.set_swing_mode(climate::CLIMATE_SWING_VERTICAL);
  sut.control(swing_call);

  // The swing command is always this exact fixed code, regardless of climate state.
  EXPECT_EQ(last_frame(transmitter), 0x8810001u);
}

// With advanced commands the swing frames are fixed codes too, one to start and one to stop the swing
TEST(LgIrClimateTests, AdvancedSwingCommandsAreIndependentOfClimateState) {
  LgIrClimate sut;
  MockRemoteTransmitter transmitter;
  configure(sut, transmitter);
  sut.set_advanced_commands_support(true);
  turn_on(sut);

  climate::ClimateCall swing_on(&sut);
  swing_on.set_swing_mode(climate::CLIMATE_SWING_VERTICAL);
  sut.control(swing_on);
  EXPECT_EQ(last_frame(transmitter), 0x8813149u);  // HEADER_ADV_SWING | VERT_SWING_ON

  climate::ClimateCall swing_off(&sut);
  swing_off.set_swing_mode(climate::CLIMATE_SWING_OFF);
  sut.control(swing_off);
  EXPECT_EQ(last_frame(transmitter), 0x881315Au);  // HEADER_ADV_SWING | VERT_SWING_OFF
}

}  // namespace esphome::climate_ir_lg::testing
