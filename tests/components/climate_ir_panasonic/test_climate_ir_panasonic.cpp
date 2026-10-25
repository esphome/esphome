#include <gtest/gtest.h>
#include <cstdint>
#include <cstring>
#include <new>
#include <utility>
#include <vector>

#include "esphome/components/climate_ir_panasonic/climate_ir_panasonic.h"
#include "esphome/components/remote_base/remote_base.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/preferences.h"

namespace esphome::climate_ir_panasonic::testing {

using Frame = std::vector<uint8_t>;
using remote_base::RawTimings;

static constexpr uint32_t TOLERANCE_PERCENT = 25;

// ---- Real frames --------------------------------------------------------------------------------------------------

// From the IRremoteESP8266 unit tests
static const uint8_t JKE_COOL_25_OFF[] = {0x02, 0x20, 0xE0, 0x04, 0x00, 0x00, 0x00, 0x06, 0x02,
                                          0x20, 0xE0, 0x04, 0x00, 0x30, 0x32, 0x80, 0xAF, 0x00,
                                          0x00, 0x06, 0x60, 0x00, 0x00, 0x80, 0x00, 0x06, 0x83};
static const uint8_t CKP_POWERFUL[] = {0x02, 0x20, 0xE0, 0x04, 0x00, 0x00, 0x00, 0x06, 0x02,
                                       0x20, 0xE0, 0x04, 0x00, 0x4E, 0x2E, 0x80, 0xAF, 0x00,
                                       0x00, 0x0E, 0xE0, 0x11, 0x00, 0x01, 0x00, 0x06, 0xB7};
static const uint8_t CKP_QUIET[] = {0x02, 0x20, 0xE0, 0x04, 0x00, 0x00, 0x00, 0x06, 0x02, 0x20, 0xE0, 0x04, 0x00, 0x4E,
                                    0x2E, 0x80, 0xAF, 0x00, 0x00, 0x0E, 0xE0, 0x30, 0x00, 0x01, 0x00, 0x06, 0xD6};
static const uint8_t DKE_HEAT_25[] = {0x02, 0x20, 0xE0, 0x04, 0x00, 0x00, 0x00, 0x06, 0x02,
                                      0x20, 0xE0, 0x04, 0x00, 0x4F, 0x32, 0x80, 0xAF, 0x0D,
                                      0x00, 0x06, 0x60, 0x00, 0x00, 0x01, 0x00, 0x06, 0x30};
// From the "Panasonic HVAC IR Protocol" notes by Mathieu (a CKP style remote)
static const uint8_t NOTES_CKP_HEAT_30[] = {0x02, 0x20, 0xE0, 0x04, 0x00, 0x00, 0x00, 0x06, 0x02,
                                            0x20, 0xE0, 0x04, 0x00, 0x48, 0x3C, 0x80, 0xAF, 0x00,
                                            0x00, 0x0E, 0xE0, 0x10, 0x00, 0x01, 0x00, 0x06, 0xBE};
// From the Instructables article by the same author (a JKE style remote)
static const uint8_t ARTICLE_AUTO_25[] = {0x02, 0x20, 0xE0, 0x04, 0x00, 0x00, 0x00, 0x06, 0x02,
                                          0x20, 0xE0, 0x04, 0x00, 0x01, 0x32, 0x80, 0xAF, 0x00,
                                          0x00, 0x06, 0x60, 0x00, 0x00, 0x80, 0x00, 0x06, 0x54};
// From a CU-NE9PKE remote. All heat, powered on, swing auto
static const uint8_t PKE_8C_FAN_MAX[] = {0x02, 0x20, 0xE0, 0x04, 0x00, 0x00, 0x00, 0x06, 0x02,
                                         0x20, 0xE0, 0x04, 0x00, 0x49, 0x10, 0x80, 0x7F, 0x00,
                                         0x00, 0x0E, 0xE0, 0x00, 0x00, 0x81, 0x00, 0x00, 0xCD};
// The 8/10 C button sends 10 C. The temperature down key then lowers it to 8 C
static const uint8_t PKE_10C_FAN_MAX[] = {0x02, 0x20, 0xE0, 0x04, 0x00, 0x00, 0x00, 0x06, 0x02,
                                          0x20, 0xE0, 0x04, 0x00, 0x49, 0x14, 0x80, 0x7F, 0x00,
                                          0x00, 0x0E, 0xE0, 0x00, 0x00, 0x81, 0x00, 0x00, 0xD1};
static const uint8_t PKE_16C_FAN_MAX[] = {0x02, 0x20, 0xE0, 0x04, 0x00, 0x00, 0x00, 0x06, 0x02,
                                          0x20, 0xE0, 0x04, 0x00, 0x49, 0x20, 0x80, 0x7F, 0x00,
                                          0x00, 0x0E, 0xE0, 0x00, 0x00, 0x81, 0x00, 0x00, 0xDD};
static const uint8_t PKE_30C_FAN_MAX[] = {0x02, 0x20, 0xE0, 0x04, 0x00, 0x00, 0x00, 0x06, 0x02,
                                          0x20, 0xE0, 0x04, 0x00, 0x49, 0x3C, 0x80, 0x7F, 0x00,
                                          0x00, 0x0E, 0xE0, 0x00, 0x00, 0x81, 0x00, 0x00, 0xF9};
static const uint8_t PKE_19C_AUTO[] = {0x02, 0x20, 0xE0, 0x04, 0x00, 0x00, 0x00, 0x06, 0x02,
                                       0x20, 0xE0, 0x04, 0x00, 0x49, 0x26, 0x80, 0xAF, 0x00,
                                       0x00, 0x0E, 0xE0, 0x00, 0x00, 0x81, 0x00, 0x00, 0x13};
static const uint8_t PKE_20C_AUTO[] = {0x02, 0x20, 0xE0, 0x04, 0x00, 0x00, 0x00, 0x06, 0x02,
                                       0x20, 0xE0, 0x04, 0x00, 0x49, 0x28, 0x80, 0xAF, 0x00,
                                       0x00, 0x0E, 0xE0, 0x00, 0x00, 0x81, 0x00, 0x00, 0x15};
// The powerful/quiet toggle button of a CU-NE9PKE remote: the second section is only 8 bytes
static const uint8_t PKE_TOGGLE[] = {0x02, 0x20, 0xE0, 0x04, 0x00, 0x00, 0x00, 0x06,
                                     0x02, 0x20, 0xE0, 0x04, 0x80, 0x9C, 0x32, 0x54};

static Frame frame(const uint8_t (&bytes)[STATE_LENGTH]) { return Frame(bytes, bytes + STATE_LENGTH); }

// ---- Building captures --------------------------------------------------------------------------------------------

static void add_section(RawTimings &t, const uint8_t *bytes, size_t len) {
  t.push_back(HEADER_MARK);
  t.push_back(-static_cast<int32_t>(HEADER_SPACE));
  for (size_t i = 0; i < len; i++) {
    for (int bit = 0; bit < 8; bit++) {
      t.push_back(BIT_MARK);
      t.push_back(-static_cast<int32_t>((bytes[i] >> bit) & 1 ? BIT_ONE_SPACE : BIT_ZERO_SPACE));
    }
  }
  t.push_back(BIT_MARK);
}

/// What a receiver captures. `idle` is the trailing space the receiver appends, as long as its idle time
static RawTimings first_section(const uint8_t *state, int idle = 0) {
  RawTimings t;
  add_section(t, state, SECTION1_LENGTH);
  if (idle != 0)
    t.push_back(-idle);
  return t;
}
static RawTimings second_section(const uint8_t *state, int idle = 0) {
  RawTimings t;
  add_section(t, state + SECTION1_LENGTH, SECTION2_LENGTH);
  if (idle != 0)
    t.push_back(-idle);
  return t;
}
static RawTimings whole_frame(const uint8_t *state, int idle = 0) {
  RawTimings t;
  add_section(t, state, SECTION1_LENGTH);
  t.push_back(-static_cast<int32_t>(SECTION_GAP));
  add_section(t, state + SECTION1_LENGTH, SECTION2_LENGTH);
  if (idle != 0)
    t.push_back(-idle);
  return t;
}

static bool read(const RawTimings &raw, uint8_t *state) {
  remote_base::RemoteReceiveData data(raw, TOLERANCE_PERCENT, remote_base::TOLERANCE_MODE_PERCENTAGE);
  return read_state(data, state);
}

// ---- Frame encoding and decoding ----------------------------------------------------------------------------------

TEST(PanasonicFrame, ChecksumMatchesEveryRealFrame) {
  for (const auto *f : {JKE_COOL_25_OFF, CKP_POWERFUL, CKP_QUIET, DKE_HEAT_25, NOTES_CKP_HEAT_30, ARTICLE_AUTO_25,
                        PKE_8C_FAN_MAX, PKE_16C_FAN_MAX, PKE_30C_FAN_MAX, PKE_19C_AUTO, PKE_20C_AUTO}) {
    EXPECT_EQ(calc_checksum(f, STATE_LENGTH), f[STATE_LENGTH - 1]);
  }
}

TEST(PanasonicFrame, ChecksumSeedCancelsTheFirstSection) {
  // The first section is constant, and sums to 0x10C. So the 0xF4 seed makes the checksum a plain sum of the second one
  uint8_t tmpl[STATE_LENGTH];
  fill_template(tmpl);
  uint8_t sum = 0;
  for (size_t i = 0; i < SECTION1_LENGTH; i++)
    sum += tmpl[i];
  EXPECT_EQ(static_cast<uint8_t>(sum + CHECKSUM_INIT), 0);
  sum = 0;
  for (size_t i = SECTION1_LENGTH; i < STATE_LENGTH - 1; i++)
    sum += NOTES_CKP_HEAT_30[i];
  EXPECT_EQ(sum, NOTES_CKP_HEAT_30[STATE_LENGTH - 1]);
}

TEST(PanasonicFrame, EncodesTheAuthorsOwnFramesExactly) {
  struct Case {
    const uint8_t (&expected)[STATE_LENGTH];
    uint8_t temperature;
    uint8_t fan;
  };
  const Case cases[] = {{PKE_16C_FAN_MAX, 16, FAN_SPEED_MAX},
                        {PKE_30C_FAN_MAX, 30, FAN_SPEED_MAX},
                        {PKE_19C_AUTO, 19, FAN_SPEED_AUTO},
                        {PKE_20C_AUTO, 20, FAN_SPEED_AUTO}};
  for (const Case &c : cases) {
    SCOPED_TRACE(static_cast<int>(c.temperature));
    FrameParams params;
    params.model = Model::PKE;
    params.power = true;
    params.mode = Mode::HEAT;
    params.temperature = c.temperature;
    params.fan_speed = c.fan;
    uint8_t state[STATE_LENGTH];
    encode_frame(params, state);
    EXPECT_EQ(frame(state), frame(c.expected));
  }
}

TEST(PanasonicFrame, EncodesFrostProtectionAsTheRemoteDoes) {
  FrameParams params;
  params.model = Model::PKE;
  params.power = true;
  params.maintenance = true;
  // Everything else is overridden
  params.mode = Mode::COOL;
  params.temperature = 24;
  params.fan_speed = FAN_SPEED_LOW;
  params.vane_v = VANE_V_LOWEST;
  uint8_t state[STATE_LENGTH];
  encode_frame(params, state);
  EXPECT_EQ(frame(state), frame(PKE_10C_FAN_MAX));

  // Switching the unit off keeps the power bit clear
  params.power = false;
  encode_frame(params, state);
  EXPECT_EQ(state[13] & 0x01, 0);
}

TEST(PanasonicFrame, PkeIsJkeWithBit3SetInByte13) {
  FrameParams params;
  params.power = true;
  params.mode = Mode::HEAT;
  uint8_t jke[STATE_LENGTH], pke[STATE_LENGTH];
  params.model = Model::JKE;
  encode_frame(params, jke);
  params.model = Model::PKE;
  encode_frame(params, pke);
  EXPECT_EQ(jke[13], 0x41);
  EXPECT_EQ(pke[13], 0x49);
}

TEST(PanasonicFrame, EncodesQuietAndPowerfulPerModel) {
  FrameParams params;
  params.mode = Mode::HEAT;
  params.temperature = 23;
  uint8_t state[STATE_LENGTH];

  params.model = Model::CKP;  // Swapped, and byte 21 b4 is always set
  params.powerful = true;
  encode_frame(params, state);
  EXPECT_EQ(state[21], CKP_POWERFUL[21]);
  params.powerful = false;
  params.quiet = true;
  encode_frame(params, state);
  EXPECT_EQ(state[21], CKP_QUIET[21]);

  params.model = Model::JKE;
  encode_frame(params, state);
  EXPECT_EQ(state[21], 0x01);
  params.quiet = false;
  params.powerful = true;
  encode_frame(params, state);
  EXPECT_EQ(state[21], 0x20);
}

TEST(PanasonicFrame, ClampsTemperatureButNotInFrostProtection) {
  FrameParams params;
  uint8_t state[STATE_LENGTH];
  params.temperature = 99;
  encode_frame(params, state);
  EXPECT_EQ((state[14] >> 1) & 0x1F, 30);
  params.temperature = 3;
  encode_frame(params, state);
  EXPECT_EQ((state[14] >> 1) & 0x1F, 16);
  // The remote always sends 27 C in fan only mode
  params.mode = Mode::FAN;
  params.temperature = 18;
  encode_frame(params, state);
  EXPECT_EQ((state[14] >> 1) & 0x1F, 27);
}

TEST(PanasonicFrame, DecodesRealFrames) {
  FrameParams p;
  p.model = Model::JKE;
  ASSERT_TRUE(decode_frame(JKE_COOL_25_OFF, p));
  EXPECT_FALSE(p.power);
  EXPECT_EQ(p.mode, Mode::COOL);
  EXPECT_EQ(p.temperature, 25);
  EXPECT_EQ(p.fan_speed, FAN_SPEED_AUTO);
  EXPECT_EQ(p.vane_v, VANE_V_AUTO);
  EXPECT_FALSE(p.maintenance);

  p = {};
  p.model = Model::JKE;
  ASSERT_TRUE(decode_frame(ARTICLE_AUTO_25, p));
  EXPECT_TRUE(p.power);
  EXPECT_EQ(p.mode, Mode::AUTO);
  EXPECT_EQ(p.temperature, 25);

  p = {};
  p.model = Model::CKP;
  ASSERT_TRUE(decode_frame(NOTES_CKP_HEAT_30, p));
  EXPECT_EQ(p.mode, Mode::HEAT);
  EXPECT_EQ(p.temperature, 30);
  EXPECT_FALSE(p.quiet);
  EXPECT_FALSE(p.powerful);

  p = {};
  p.model = Model::CKP;
  ASSERT_TRUE(decode_frame(CKP_POWERFUL, p));
  EXPECT_TRUE(p.powerful);
  EXPECT_FALSE(p.quiet);
  p = {};
  p.model = Model::CKP;
  ASSERT_TRUE(decode_frame(CKP_QUIET, p));
  EXPECT_TRUE(p.quiet);
  EXPECT_FALSE(p.powerful);

  p = {};
  p.model = Model::DKE;
  ASSERT_TRUE(decode_frame(DKE_HEAT_25, p));
  EXPECT_TRUE(p.power);
  EXPECT_EQ(p.vane_h, 0x0D);
}

TEST(PanasonicFrame, DecodesTheAuthorsFrostProtectionFrames) {
  for (const auto &c : {std::make_pair(PKE_10C_FAN_MAX, 10), std::make_pair(PKE_8C_FAN_MAX, 8)}) {
    FrameParams p;
    p.model = Model::PKE;
    ASSERT_TRUE(decode_frame(c.first, p));
    EXPECT_TRUE(p.power);
    EXPECT_EQ(p.mode, Mode::HEAT);
    EXPECT_EQ(p.temperature, c.second);
    EXPECT_EQ(p.fan_speed, FAN_SPEED_MAX);
    EXPECT_TRUE(p.maintenance);
  }

  // Ordinary frames are never mistaken for it
  for (const auto *f : {PKE_16C_FAN_MAX, PKE_30C_FAN_MAX, PKE_19C_AUTO, PKE_20C_AUTO}) {
    FrameParams n;
    n.model = Model::PKE;
    ASSERT_TRUE(decode_frame(f, n));
    EXPECT_FALSE(n.maintenance);
  }
}

TEST(PanasonicFrame, RejectsInvalidFrames) {
  FrameParams p;
  uint8_t bad[STATE_LENGTH];

  memcpy(bad, JKE_COOL_25_OFF, STATE_LENGTH);
  bad[26] ^= 1;  // Checksum
  EXPECT_FALSE(decode_frame(bad, p));

  memcpy(bad, JKE_COOL_25_OFF, STATE_LENGTH);
  bad[3] = 0;  // Static prefix
  bad[26] = calc_checksum(bad, STATE_LENGTH);
  EXPECT_FALSE(decode_frame(bad, p));

  memcpy(bad, JKE_COOL_25_OFF, STATE_LENGTH);
  bad[13] = 0x10;  // Mode 1 doesn't exist
  bad[26] = calc_checksum(bad, STATE_LENGTH);
  EXPECT_FALSE(decode_frame(bad, p));

  memcpy(bad, PKE_8C_FAN_MAX, STATE_LENGTH);
  bad[14] = 0x0E;  // 7 C, below even the frost protection frame
  bad[26] = calc_checksum(bad, STATE_LENGTH);
  EXPECT_FALSE(decode_frame(bad, p));

  memcpy(bad, JKE_COOL_25_OFF, STATE_LENGTH);
  bad[16] = 0x0F;  // Fan nibble 0
  bad[26] = calc_checksum(bad, STATE_LENGTH);
  EXPECT_FALSE(decode_frame(bad, p));
}

TEST(PanasonicFrame, RoundTripsEverythingTheEncoderCanProduce) {
  for (auto model : {Model::DKE, Model::JKE, Model::LKE, Model::NKE, Model::CKP, Model::RKR, Model::PKE}) {
    for (auto mode : {Mode::AUTO, Mode::DRY, Mode::COOL, Mode::HEAT, Mode::FAN}) {
      for (uint8_t fan :
           {FAN_SPEED_MIN, FAN_SPEED_LOW, FAN_SPEED_MEDIUM, FAN_SPEED_HIGH, FAN_SPEED_MAX, FAN_SPEED_AUTO}) {
        for (uint8_t temp = TEMP_MIN; temp <= TEMP_MAX; temp++) {
          for (uint8_t vane : {VANE_V_HIGHEST, VANE_V_MIDDLE, VANE_V_LOWEST, VANE_V_AUTO}) {
            for (int power = 0; power < 2; power++) {
              for (int option = 0; option < 3; option++) {  // none, quiet, powerful
                FrameParams in;
                in.model = model;
                in.mode = mode;
                in.temperature = temp;
                in.fan_speed = fan;
                in.vane_v = vane;
                in.power = power;
                in.quiet = option == 1;
                in.powerful = option == 2;
                const bool has_horizontal_vane = model == Model::DKE || model == Model::RKR;
                if (has_horizontal_vane)
                  in.vane_h = 0x0B;
                uint8_t state[STATE_LENGTH];
                encode_frame(in, state);

                FrameParams out;
                out.model = model;
                ASSERT_TRUE(decode_frame(state, out));
                EXPECT_EQ(out.power, in.power);
                EXPECT_EQ(out.mode, in.mode);
                EXPECT_EQ(out.temperature, mode == Mode::FAN ? TEMP_FAN_MODE : temp);
                EXPECT_EQ(out.fan_speed, in.fan_speed);
                EXPECT_EQ(out.vane_v, in.vane_v);
                EXPECT_EQ(out.quiet, in.quiet);
                EXPECT_EQ(out.powerful, in.powerful);
                if (has_horizontal_vane)
                  EXPECT_EQ(out.vane_h, in.vane_h);
              }
            }
          }
        }
      }
    }
  }
}

// ---- Reading a capture --------------------------------------------------------------------------------------------

TEST(PanasonicReceive, WholeFrameForAnyIdleTime) {
  for (int idle : {0, 5000, 10000, 20000, 30000}) {
    SCOPED_TRACE(idle);
    uint8_t state[STATE_LENGTH] = {};
    ASSERT_TRUE(read(whole_frame(PKE_20C_AUTO, idle), state));
    EXPECT_EQ(frame(state), frame(PKE_20C_AUTO));
  }
}

TEST(PanasonicReceive, SplitCapturesNeedOnlyTheSecondSection) {
  // The 10 ms gap is as long as the default idle time, and a shorter idle time always splits the frame
  for (int idle : {0, 5000, 10000}) {
    SCOPED_TRACE(idle);
    uint8_t state[STATE_LENGTH] = {};
    EXPECT_FALSE(read(first_section(PKE_20C_AUTO, idle), state));
    ASSERT_TRUE(read(second_section(PKE_20C_AUTO, idle), state));
    EXPECT_EQ(frame(state), frame(PKE_20C_AUTO));
  }
}

TEST(PanasonicReceive, SecondSectionIsEnoughOnItsOwn) {
  uint8_t state[STATE_LENGTH] = {};
  ASSERT_TRUE(read(second_section(PKE_20C_AUTO, 5000), state));
  EXPECT_EQ(frame(state), frame(PKE_20C_AUTO));
}

TEST(PanasonicReceive, IgnoresShortButtonFrames) {
  // Powerful/quiet and nanoe-G have an 8 byte second section of their own
  uint8_t state[STATE_LENGTH] = {};
  RawTimings whole;
  add_section(whole, PKE_TOGGLE, 8);
  whole.push_back(-static_cast<int32_t>(SECTION_GAP));
  add_section(whole, PKE_TOGGLE + 8, 8);
  EXPECT_FALSE(read(whole, state));

  RawTimings lone;
  add_section(lone, PKE_TOGGLE + 8, 8);
  lone.push_back(-5000);
  EXPECT_FALSE(read(lone, state));
}

TEST(PanasonicReceive, RejectsBrokenCaptures) {
  uint8_t state[STATE_LENGTH] = {};
  EXPECT_FALSE(read({}, state));

  RawTimings truncated = whole_frame(PKE_20C_AUTO);
  truncated.resize(truncated.size() - 40);
  EXPECT_FALSE(read(truncated, state));

  RawTimings bad_bit = whole_frame(PKE_20C_AUTO);
  bad_bit[5] = 9000;
  EXPECT_FALSE(read(bad_bit, state));

  // Too many bytes must be rejected without writing past the buffer
  RawTimings too_long;
  const uint8_t zeros[40] = {};
  add_section(too_long, zeros, sizeof(zeros));
  EXPECT_FALSE(read(too_long, state));

  RawTimings junk = first_section(PKE_20C_AUTO);
  junk.push_back(-3000);
  junk.push_back(HEADER_MARK);
  EXPECT_FALSE(read(junk, state));

  uint8_t other[STATE_LENGTH];
  memcpy(other, PKE_20C_AUTO, STATE_LENGTH);
  other[0] = 0x12;  // Not our first section
  EXPECT_FALSE(read(whole_frame(other), state));
}

TEST(PanasonicReceive, ToleratesTimingDrift) {
  for (int percent : {90, 110}) {
    SCOPED_TRACE(percent);
    RawTimings raw = whole_frame(PKE_20C_AUTO);
    for (auto &v : raw)
      v = v * percent / 100;
    uint8_t state[STATE_LENGTH] = {};
    ASSERT_TRUE(read(raw, state));
    EXPECT_EQ(frame(state), frame(PKE_20C_AUTO));
  }
}

// ---- The climate entity -------------------------------------------------------------------------------------------

class CapturingTransmitter : public remote_base::RemoteTransmitterBase {
 public:
  CapturingTransmitter() : RemoteTransmitterBase(nullptr) {}
  std::vector<RawTimings> frames;

 protected:
  void send_internal(uint32_t send_times, uint32_t send_wait) override {
    this->frames.push_back(this->temp_.get_data());
  }
};

struct Fixture {
  CapturingTransmitter transmitter;
  PanasonicIrClimate climate;

  explicit Fixture(Model model = Model::PKE) {
    // A second fixture in the same test would otherwise restore what the first saved
    if (global_preferences != nullptr)
      global_preferences->reset();
    // The test main does not construct App as generated code does
    static const bool APP_CONSTRUCTED = (new (&App) Application(), true);
    (void) APP_CONSTRUCTED;
    static char name[] = "test_climate_ir_panasonic";
    static char friendly_name[] = "";
    App.pre_setup(name, sizeof(name) - 1, friendly_name, 0);
    this->climate.set_model(model);
    this->climate.set_transmitter(&this->transmitter);
    static_cast<Component &>(this->climate).setup();
  }

  /// Builds a call, lets `fill` set it up, and performs it
  template<typename F> void command(F &&fill) {
    auto call = this->climate.make_call();
    fill(call);
    call.perform();
  }

  /// The state of the last transmission, read back with the same code that reads a remote
  Frame last_state() const {
    EXPECT_FALSE(this->transmitter.frames.empty());
    if (this->transmitter.frames.empty())
      return {};
    uint8_t state[STATE_LENGTH] = {};
    EXPECT_TRUE(read(this->transmitter.frames.back(), state));
    return frame(state);
  }

  bool receive(const RawTimings &raw) {
    remote_base::RemoteReceiveData data(raw, TOLERANCE_PERCENT, remote_base::TOLERANCE_MODE_PERCENTAGE);
    // The receiver calls through the listener interface, where on_receive is public
    return static_cast<remote_base::RemoteReceiverListener &>(this->climate).on_receive(data);
  }
  bool receive(const uint8_t (&state)[STATE_LENGTH]) { return this->receive(whole_frame(state)); }
};

TEST(PanasonicClimate, TransmitsTheAuthorsFramesExactly) {
  Fixture f;
  f.command([](auto &call) {
    call.set_mode(climate::CLIMATE_MODE_HEAT);
    call.set_target_temperature(20.0f);
    call.set_fan_mode(climate::CLIMATE_FAN_AUTO);
    call.set_swing_mode(climate::CLIMATE_SWING_VERTICAL);  // The remote sends the vane on auto
  });
  EXPECT_EQ(f.last_state(), frame(PKE_20C_AUTO));

  f.command([](auto &call) { call.set_target_temperature(19.0f); });
  EXPECT_EQ(f.last_state(), frame(PKE_19C_AUTO));

  f.command([](auto &call) {
    call.set_target_temperature(30.0f);
    call.set_fan_mode("Maximum");
  });
  EXPECT_EQ(f.last_state(), frame(PKE_30C_FAN_MAX));
}

TEST(PanasonicClimate, TransmitsTwoSectionsWithTheRightTimings) {
  Fixture f;
  f.command([](auto &call) {
    call.set_mode(climate::CLIMATE_MODE_COOL);
    call.set_target_temperature(24.0f);
  });
  ASSERT_EQ(f.transmitter.frames.size(), 1u);
  const RawTimings &t = f.transmitter.frames.back();
  // header + 8 bytes + footer, gap, header + 19 bytes + footer
  ASSERT_EQ(t.size(), static_cast<size_t>(2 + 8 * 16 + 1 + 1 + 2 + 19 * 16 + 1));
  EXPECT_EQ(t[0], static_cast<int32_t>(HEADER_MARK));
  EXPECT_EQ(t[1], -static_cast<int32_t>(HEADER_SPACE));
  EXPECT_EQ(t[2 + 8 * 16], static_cast<int32_t>(BIT_MARK));
  EXPECT_EQ(t[2 + 8 * 16 + 1], -static_cast<int32_t>(SECTION_GAP));
  EXPECT_EQ(t.back(), static_cast<int32_t>(BIT_MARK));
}

TEST(PanasonicClimate, OffKeepsTheLastModeAndClearsThePowerBit) {
  Fixture f;
  f.command([](auto &call) {
    call.set_mode(climate::CLIMATE_MODE_COOL);
    call.set_target_temperature(23.0f);
  });
  f.command([](auto &call) { call.set_mode(climate::CLIMATE_MODE_OFF); });

  FrameParams p;
  p.model = Model::PKE;
  const Frame last = f.last_state();
  ASSERT_TRUE(decode_frame(last.data(), p));
  EXPECT_FALSE(p.power);
  EXPECT_EQ(p.mode, Mode::COOL);
  EXPECT_EQ(p.temperature, 23);
}

TEST(PanasonicClimate, CkpOnlyHasAPowerToggle) {
  Fixture f(Model::CKP);
  auto power_bit = [&]() { return f.last_state()[13] & 0x01; };

  f.command([](auto &call) {
    call.set_mode(climate::CLIMATE_MODE_HEAT);
    call.set_target_temperature(22.0f);
  });
  EXPECT_EQ(power_bit(), 1);  // Off -> on toggles

  f.command([](auto &call) { call.set_target_temperature(23.0f); });
  EXPECT_EQ(power_bit(), 0);  // Already on, so changing a setting must not toggle it off

  f.command([](auto &call) { call.set_mode(climate::CLIMATE_MODE_OFF); });
  EXPECT_EQ(power_bit(), 1);  // On -> off toggles

  f.command([](auto &call) { call.set_mode(climate::CLIMATE_MODE_OFF); });
  EXPECT_EQ(power_bit(), 0);  // Already off
}

TEST(PanasonicClimate, FrostProtectionIsAPreset) {
  Fixture f;
  f.command([](auto &call) {
    call.set_mode(climate::CLIMATE_MODE_COOL);
    call.set_target_temperature(24.0f);
  });

  f.command([](auto &call) { call.set_preset(climate::CLIMATE_PRESET_AWAY); });
  EXPECT_EQ(f.last_state(), frame(PKE_10C_FAN_MAX));
  EXPECT_EQ(f.climate.mode, climate::CLIMATE_MODE_HEAT);
  EXPECT_EQ(f.climate.preset, climate::CLIMATE_PRESET_AWAY);
  EXPECT_FLOAT_EQ(f.climate.target_temperature, 24.0f);  // The set-point is not touched

  // Changing anything else takes the unit out of it
  f.command([](auto &call) { call.set_target_temperature(21.0f); });
  EXPECT_EQ(f.climate.preset, climate::CLIMATE_PRESET_NONE);
  FrameParams p;
  p.model = Model::PKE;
  const Frame last = f.last_state();
  ASSERT_TRUE(decode_frame(last.data(), p));
  EXPECT_FALSE(p.maintenance);
  EXPECT_EQ(p.temperature, 21);
}

TEST(PanasonicClimate, ReceivesTheRemote) {
  Fixture f;
  ASSERT_TRUE(f.receive(PKE_20C_AUTO));
  EXPECT_EQ(f.climate.mode, climate::CLIMATE_MODE_HEAT);
  EXPECT_FLOAT_EQ(f.climate.target_temperature, 20.0f);
  EXPECT_EQ(f.climate.fan_mode, climate::CLIMATE_FAN_AUTO);
  EXPECT_EQ(f.climate.swing_mode, climate::CLIMATE_SWING_VERTICAL);
  EXPECT_EQ(f.climate.preset, climate::CLIMATE_PRESET_NONE);

  ASSERT_TRUE(f.receive(PKE_30C_FAN_MAX));
  EXPECT_FLOAT_EQ(f.climate.target_temperature, 30.0f);
  EXPECT_TRUE(f.climate.has_custom_fan_mode());
  EXPECT_EQ(f.climate.get_custom_fan_mode(), "Maximum");

  ASSERT_TRUE(f.receive(JKE_COOL_25_OFF));  // Power bit clear
  EXPECT_EQ(f.climate.mode, climate::CLIMATE_MODE_OFF);
  EXPECT_FLOAT_EQ(f.climate.target_temperature, 25.0f);
  EXPECT_FALSE(f.climate.has_custom_fan_mode());
  EXPECT_EQ(f.climate.fan_mode, climate::CLIMATE_FAN_AUTO);
}

TEST(PanasonicClimate, ReceivesFrostProtectionWithoutLosingTheSetPoint) {
  Fixture f;
  ASSERT_TRUE(f.receive(PKE_19C_AUTO));
  EXPECT_FLOAT_EQ(f.climate.target_temperature, 19.0f);

  // The button sends 10 C, and the temperature down key then lowers it to 8 C
  for (const auto *frost : {PKE_10C_FAN_MAX, PKE_8C_FAN_MAX}) {
    ASSERT_TRUE(f.receive(whole_frame(frost)));
    EXPECT_EQ(f.climate.preset, climate::CLIMATE_PRESET_AWAY);
    EXPECT_EQ(f.climate.mode, climate::CLIMATE_MODE_HEAT);
    EXPECT_FLOAT_EQ(f.climate.target_temperature, 19.0f);  // Frost protection is a mode, not a set-point
    EXPECT_EQ(f.climate.get_custom_fan_mode(), "Maximum");
  }

  ASSERT_TRUE(f.receive(PKE_20C_AUTO));
  EXPECT_EQ(f.climate.preset, climate::CLIMATE_PRESET_NONE);
  EXPECT_FLOAT_EQ(f.climate.target_temperature, 20.0f);
}

TEST(PanasonicClimate, ReceivesSplitCaptures) {
  Fixture f;
  EXPECT_FALSE(f.receive(first_section(PKE_20C_AUTO, 5000)));
  ASSERT_TRUE(f.receive(second_section(PKE_20C_AUTO, 5000)));
  EXPECT_FLOAT_EQ(f.climate.target_temperature, 20.0f);
}

TEST(PanasonicClimate, IgnoresWhatItDoesNotUnderstand) {
  Fixture f;
  ASSERT_TRUE(f.receive(PKE_20C_AUTO));

  uint8_t bad[STATE_LENGTH];
  memcpy(bad, PKE_30C_FAN_MAX, STATE_LENGTH);
  bad[26] ^= 1;
  EXPECT_FALSE(f.receive(bad));
  EXPECT_FLOAT_EQ(f.climate.target_temperature, 20.0f);  // Unchanged
}

TEST(PanasonicClimate, HearsItsOwnTransmissions) {
  // Whatever the entity sends must read back to the state it was set to, for every model
  for (auto model : {Model::DKE, Model::JKE, Model::LKE, Model::NKE, Model::CKP, Model::RKR, Model::PKE}) {
    SCOPED_TRACE(static_cast<int>(model));
    Fixture f(model);
    f.command([](auto &call) {
      call.set_mode(climate::CLIMATE_MODE_COOL);
      call.set_target_temperature(22.0f);
      call.set_fan_mode("Minimum");
    });

    Fixture g(model);
    ASSERT_TRUE(g.receive(f.transmitter.frames.back()));
    EXPECT_EQ(g.climate.mode, climate::CLIMATE_MODE_COOL);
    EXPECT_FLOAT_EQ(g.climate.target_temperature, 22.0f);
    EXPECT_EQ(g.climate.get_custom_fan_mode(), "Minimum");
  }
}

}  // namespace esphome::climate_ir_panasonic::testing
