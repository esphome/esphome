#include <gtest/gtest.h>
#include <cstdint>
#include <vector>

#include "esphome/components/remote_base/remote_base.h"
#include "esphome/components/remko_ar715/remko_ar715.h"

namespace esphome::remko_ar715::testing {

static constexpr int32_t BIT_MARK = 511;
static constexpr int32_t HDR_SPACE = 3492;
static constexpr int32_t ONE_SPACE = 1540;
static constexpr int32_t ZERO_SPACE = 548;
static constexpr int BITS = 52;

/// The waveform the component must produce for a code: header, 52 bits MSB first, stop bit
static remote_base::RawTimings waveform(uint64_t code) {
  remote_base::RawTimings raw;
  raw.push_back(BIT_MARK);
  raw.push_back(-HDR_SPACE);
  for (int i = BITS - 1; i >= 0; i--) {
    raw.push_back(BIT_MARK);
    raw.push_back((code >> i) & 1 ? -ONE_SPACE : -ZERO_SPACE);
  }
  raw.push_back(BIT_MARK);
  raw.push_back(-HDR_SPACE);
  return raw;
}

/// Decodes a frame back into its code so a mismatch is readable
static uint64_t code_of(const remote_base::RawTimings &raw) {
  uint64_t code = 0;
  for (int bit = 0; bit < BITS; bit++) {
    const size_t space = 3 + 2 * bit;
    code = (code << 1) | (space < raw.size() && -raw[space] > 1000 ? 1 : 0);
  }
  return code;
}

class CapturingTransmitter : public remote_base::RemoteTransmitterBase {
 public:
  CapturingTransmitter() : RemoteTransmitterBase(nullptr) {}
  std::vector<remote_base::RawTimings> frames;

 protected:
  void send_internal(uint32_t send_times, uint32_t send_wait) override {
    this->frames.push_back(this->temp_.get_data());
  }
};

struct Fixture {
  CapturingTransmitter transmitter;
  RemkoAr715Climate climate;

  Fixture() {
    this->climate.set_transmitter(&this->transmitter);
    this->climate.setup();
  }

  void command(climate::ClimateMode mode, float temperature, climate::ClimateFanMode fan,
               climate::ClimateSwingMode swing) {
    auto call = this->climate.make_call();
    call.set_mode(mode);
    call.set_target_temperature(temperature);
    call.set_fan_mode(fan);
    call.set_swing_mode(swing);
    call.perform();
  }

  const remote_base::RawTimings &last_frame() const { return this->transmitter.frames.back(); }
};

static void expect_code(const remote_base::RawTimings &actual, uint64_t code) {
  EXPECT_EQ(actual, waveform(code)) << std::hex << "actual   0x" << code_of(actual) << "\nexpected 0x" << code;
}

// Codes captured from the AR-715 remote in IRremoteESP8266 issue 1812
TEST(RemkoAr715, EncodesCapturedFrames) {
  Fixture f;

  f.command(climate::CLIMATE_MODE_COOL, 16, climate::CLIMATE_FAN_HIGH, climate::CLIMATE_SWING_VERTICAL);
  expect_code(f.last_frame(), 0x830000005880FULL);

  // Power off keeps the rest of the state in the frame
  f.command(climate::CLIMATE_MODE_OFF, 16, climate::CLIMATE_FAN_HIGH, climate::CLIMATE_SWING_VERTICAL);
  expect_code(f.last_frame(), 0x8000000058802ULL);

  f.command(climate::CLIMATE_MODE_COOL, 25, climate::CLIMATE_FAN_MEDIUM, climate::CLIMATE_SWING_OFF);
  expect_code(f.last_frame(), 0x830000007919AULL);

  f.command(climate::CLIMATE_MODE_COOL, 20, climate::CLIMATE_FAN_MEDIUM, climate::CLIMATE_SWING_OFF);
  expect_code(f.last_frame(), 0x830000007914FULL);
}

// Dry and fan-only replace the fan nibbles; the fan speed is not sent
TEST(RemkoAr715, DryAndFanOnlyIgnoreTheFanSpeed) {
  Fixture f;

  f.command(climate::CLIMATE_MODE_DRY, 22, climate::CLIMATE_FAN_HIGH, climate::CLIMATE_SWING_OFF);
  expect_code(f.last_frame(), 0x830000007A469ULL);

  f.command(climate::CLIMATE_MODE_FAN_ONLY, 22, climate::CLIMATE_FAN_HIGH, climate::CLIMATE_SWING_OFF);
  expect_code(f.last_frame(), 0x830000007A36AULL);
}

}  // namespace esphome::remko_ar715::testing
