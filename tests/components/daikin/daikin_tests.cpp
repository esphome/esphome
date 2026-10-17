#include <gtest/gtest.h>
#include <array>
#include <cstdint>
#include <cstdio>
#include <new>
#include <string>
#include <vector>

#include "esphome/components/daikin/daikin.h"
#include "esphome/components/remote_base/remote_base.h"
#include "esphome/core/application.h"
#include "esphome/core/preferences.h"

namespace esphome::daikin::testing {

static constexpr size_t MESSAGE_SIZE = 35;
using Message = std::array<uint8_t, MESSAGE_SIZE>;
static constexpr uint32_t BOOST_TIMEOUT_ID = 0;

// The host scheduler reads the real clock, so the 20 minute boost timer cannot elapse in a test.
// Explicit instantiation skips access checks, which lets the test run the timer's callback itself.
template<void (DaikinClimate::*M)()> struct BoostCallback {
  friend void run_boost_callback(DaikinClimate &climate) { (climate.*M)(); }
};
void run_boost_callback(DaikinClimate &climate);
template struct BoostCallback<&DaikinClimate::cancel_boost_mode_>;

static uint8_t sum(const uint8_t *data, size_t len) {
  uint8_t total = 0;
  for (size_t i = 0; i < len; i++)
    total += data[i];
  return total;
}

static std::string hex(const uint8_t *data, size_t len) {
  std::string out;
  for (size_t i = 0; i < len; i++) {
    char buf[4];
    snprintf(buf, sizeof(buf), "%02X ", data[i]);
    out += buf;
  }
  return out;
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

/// Reads `count` LSB-first bytes starting at raw[pos], advancing pos past them
static void read_bytes(const remote_base::RawTimings &raw, size_t &pos, uint8_t *out, size_t count) {
  for (size_t byte = 0; byte < count; byte++) {
    uint8_t value = 0;
    for (int bit = 0; bit < 8; bit++) {
      EXPECT_EQ(raw[pos], static_cast<int32_t>(DAIKIN_BIT_MARK)) << "bit mark at " << pos;
      const int32_t space = -raw[pos + 1];
      if (space == static_cast<int32_t>(DAIKIN_ONE_SPACE)) {
        value |= 1 << bit;
      } else {
        EXPECT_EQ(space, static_cast<int32_t>(DAIKIN_ZERO_SPACE)) << "bit space at " << pos + 1;
      }
      pos += 2;
    }
    out[byte] = value;
  }
}

static void expect_header(const remote_base::RawTimings &raw, size_t &pos) {
  EXPECT_EQ(raw[pos], static_cast<int32_t>(DAIKIN_HEADER_MARK)) << "header mark at " << pos;
  EXPECT_EQ(raw[pos + 1], -static_cast<int32_t>(DAIKIN_HEADER_SPACE)) << "header space at " << pos + 1;
  pos += 2;
}

static void expect_trailer(const remote_base::RawTimings &raw, size_t &pos, uint32_t space) {
  EXPECT_EQ(raw[pos], static_cast<int32_t>(DAIKIN_BIT_MARK)) << "trailer mark at " << pos;
  EXPECT_EQ(raw[pos + 1], -static_cast<int32_t>(space)) << "trailer space at " << pos + 1;
  pos += 2;
}

/// Decodes a captured transmission back into its 35 bytes, checking headers and frame gaps
static Message decode(const remote_base::RawTimings &raw) {
  Message msg{};
  // Three frames of 8, 8 and 19 bytes, each a header, two timings per bit and a trailer
  const size_t expected_size = 3 * 4 + 2 * 8 * MESSAGE_SIZE;
  EXPECT_EQ(raw.size(), expected_size);
  if (raw.size() != expected_size)
    return msg;
  size_t pos = 0;
  expect_header(raw, pos);
  read_bytes(raw, pos, msg.data(), 8);
  expect_trailer(raw, pos, DAIKIN_MESSAGE_SPACE);
  expect_header(raw, pos);
  read_bytes(raw, pos, msg.data() + 8, 8);
  expect_trailer(raw, pos, DAIKIN_MESSAGE_SPACE);
  expect_header(raw, pos);
  read_bytes(raw, pos, msg.data() + 16, 19);
  expect_trailer(raw, pos, 0);
  return msg;
}

/// Timings for one received frame (8 or 19 bytes) as the receiver would deliver it
static remote_base::RawTimings frame_timings(const std::vector<uint8_t> &bytes) {
  remote_base::RawTimings raw;
  raw.push_back(DAIKIN_HEADER_MARK);
  raw.push_back(-static_cast<int32_t>(DAIKIN_HEADER_SPACE));
  for (uint8_t byte : bytes) {
    for (int bit = 0; bit < 8; bit++) {
      raw.push_back(DAIKIN_BIT_MARK);
      raw.push_back(-static_cast<int32_t>((byte >> bit) & 1 ? DAIKIN_ONE_SPACE : DAIKIN_ZERO_SPACE));
    }
  }
  raw.push_back(DAIKIN_BIT_MARK);
  raw.push_back(-static_cast<int32_t>(DAIKIN_MESSAGE_SPACE));
  return raw;
}

/// The 0xC5 frame, with a valid checksum unless one is given
static std::vector<uint8_t> comfort_frame(uint8_t comfort) {
  std::vector<uint8_t> f{0x11, 0xDA, 0x27, 0x00, 0xC5, 0x00, comfort, 0x00};
  f[7] = sum(f.data(), 7);
  return f;
}

/// The 19-byte state frame: cool at `temperature` (half degrees), fan auto, no swing
static std::vector<uint8_t> state_frame(uint8_t byte13, uint8_t byte16, uint8_t temperature = 44) {
  std::vector<uint8_t> f{0x11,        0xDA,   0x27, 0x00, 0x00,   DAIKIN_MODE_ON | DAIKIN_MODE_COOL,
                         temperature, 0x00,   0xA0, 0x00, 0x00,   0x06,
                         0x60,        byte13, 0x00, 0xC0, byte16, 0x00,
                         0x00};
  f[18] = sum(f.data(), 18);
  return f;
}

struct Fixture {
  CapturingTransmitter transmitter;
  DaikinClimate climate;

  /// Pass keep_preferences to restore what an earlier fixture saved
  explicit Fixture(bool keep_preferences = false) {
    // A second fixture in the same test would otherwise restore what the first saved
    if (!keep_preferences && global_preferences != nullptr)
      global_preferences->reset();
    // The test main does not construct App as generated code does; the boost timer needs its scheduler
    static const bool APP_CONSTRUCTED = (new (&App) Application(), true);
    (void) APP_CONSTRUCTED;
    static char name[] = "test_daikin";
    static char friendly_name[] = "";
    App.pre_setup(name, sizeof(name) - 1, friendly_name, 0);
    this->climate.set_transmitter(&this->transmitter);
    static_cast<Component &>(this->climate).setup();
  }

  ~Fixture() {
    // Drop a pending boost timer while the climate is alive; App outlives each test
    App.scheduler.cancel_timeout(&this->climate, BOOST_TIMEOUT_ID);
    App.scheduler.call(millis());
  }

  void command(climate::ClimateMode mode, float temperature, climate::ClimatePreset preset) {
    auto call = this->climate.make_call();
    call.set_mode(mode);
    call.set_target_temperature(temperature);
    call.set_preset(preset);
    call.perform();
  }

  Message last_message() const { return decode(this->transmitter.frames.back()); }

  bool receive(const std::vector<uint8_t> &bytes) {
    const remote_base::RawTimings raw = frame_timings(bytes);
    remote_base::RemoteReceiveData data(raw, 25, remote_base::TOLERANCE_MODE_PERCENTAGE);
    // The receiver calls through the listener interface, where on_receive is public
    return static_cast<remote_base::RemoteReceiverListener &>(this->climate).on_receive(data);
  }

  /// Fires the boost timer as the scheduler would if it is pending; returns whether it was
  bool fire_boost_timer() {
    if (!App.scheduler.cancel_timeout(&this->climate, BOOST_TIMEOUT_ID))
      return false;
    run_boost_callback(this->climate);
    return true;
  }
};

static void expect_checksums(const Message &m) {
  EXPECT_EQ(m[7], sum(m.data(), 7)) << hex(m.data(), MESSAGE_SIZE);
  EXPECT_EQ(m[34], sum(m.data() + 16, 18)) << hex(m.data(), MESSAGE_SIZE);
}

TEST(Daikin, EncodesHalfDegree) {
  Fixture f;
  f.command(climate::CLIMATE_MODE_COOL, 22.5f, climate::CLIMATE_PRESET_NONE);
  ASSERT_EQ(f.transmitter.frames.size(), 1u);
  const Message m = f.last_message();
  EXPECT_EQ(m[16 + 5], DAIKIN_MODE_ON | DAIKIN_MODE_COOL);
  EXPECT_EQ(m[22], 45);
  EXPECT_EQ(m[6], 0x00);
  EXPECT_EQ(m[29], 0x00);
  EXPECT_EQ(m[32], 0x00);
  expect_checksums(m);
}

TEST(Daikin, EncodesEachPresetInItsOwnByte) {
  struct Case {
    climate::ClimatePreset preset;
    uint8_t byte6, byte29, byte32;
  };
  const Case cases[] = {
      {climate::CLIMATE_PRESET_NONE, 0x00, 0x00, 0x00},     {climate::CLIMATE_PRESET_BOOST, 0x00, 0x01, 0x00},
      {climate::CLIMATE_PRESET_SLEEP, 0x00, 0x20, 0x00},    {climate::CLIMATE_PRESET_ECO, 0x00, 0x00, 0x04},
      {climate::CLIMATE_PRESET_ACTIVITY, 0x00, 0x00, 0x02}, {climate::CLIMATE_PRESET_COMFORT, 0x10, 0x00, 0x00},
  };
  Fixture f;
  uint8_t plain_first_checksum = 0;
  for (const Case &c : cases) {
    SCOPED_TRACE(static_cast<int>(c.preset));
    f.command(climate::CLIMATE_MODE_COOL, 22.5f, c.preset);
    const Message m = f.last_message();
    EXPECT_EQ(m[6], c.byte6);
    EXPECT_EQ(m[29], c.byte29);
    EXPECT_EQ(m[32], c.byte32);
    EXPECT_EQ(m[22], 45);
    expect_checksums(m);
    if (c.preset == climate::CLIMATE_PRESET_NONE) {
      plain_first_checksum = m[7];
    } else if (c.preset == climate::CLIMATE_PRESET_COMFORT) {
      EXPECT_NE(m[7], plain_first_checksum);
      EXPECT_EQ(static_cast<uint8_t>(m[7] - plain_first_checksum), 0x10);
    } else {
      EXPECT_EQ(m[7], plain_first_checksum);
    }
  }
}

TEST(Daikin, ReceivedComfortFrameSetsComfort) {
  Fixture f;
  EXPECT_TRUE(f.receive(comfort_frame(0x10)));
  EXPECT_TRUE(f.receive(state_frame(0x00, 0x00)));
  EXPECT_EQ(f.climate.preset, climate::CLIMATE_PRESET_COMFORT);
  EXPECT_EQ(f.climate.mode, climate::CLIMATE_MODE_COOL);
}

TEST(Daikin, ReceivedPowerfulGivesBoost) {
  Fixture f;
  EXPECT_TRUE(f.receive(comfort_frame(0x10)));
  EXPECT_TRUE(f.receive(state_frame(0x00, 0x00)));
  EXPECT_TRUE(f.receive(comfort_frame(0x00)));
  EXPECT_TRUE(f.receive(state_frame(0x01, 0x00)));
  EXPECT_EQ(f.climate.preset, climate::CLIMATE_PRESET_BOOST);
}

TEST(Daikin, ReceivedStateWithoutBitsClearsEco) {
  Fixture f;
  EXPECT_TRUE(f.receive(comfort_frame(0x00)));
  EXPECT_TRUE(f.receive(state_frame(0x00, 0x04)));
  EXPECT_EQ(f.climate.preset, climate::CLIMATE_PRESET_ECO);
  EXPECT_TRUE(f.receive(comfort_frame(0x00)));
  EXPECT_TRUE(f.receive(state_frame(0x00, 0x00)));
  EXPECT_EQ(f.climate.preset, climate::CLIMATE_PRESET_NONE);
}

TEST(Daikin, ReceivedOtherPresets) {
  Fixture f;
  EXPECT_TRUE(f.receive(state_frame(0x20, 0x00)));
  EXPECT_EQ(f.climate.preset, climate::CLIMATE_PRESET_SLEEP);
  EXPECT_TRUE(f.receive(state_frame(0x00, 0x02)));
  EXPECT_EQ(f.climate.preset, climate::CLIMATE_PRESET_ACTIVITY);
}

TEST(Daikin, ReceivedHalfDegree) {
  Fixture f;
  EXPECT_TRUE(f.receive(state_frame(0x00, 0x00, 45)));
  EXPECT_EQ(f.climate.mode, climate::CLIMATE_MODE_COOL);
  EXPECT_FLOAT_EQ(f.climate.target_temperature, 22.5f);
}

TEST(Daikin, ReceivedFrameRoundTripsTransmittedState) {
  Fixture f;
  f.command(climate::CLIMATE_MODE_COOL, 22.5f, climate::CLIMATE_PRESET_COMFORT);
  const Message m = f.last_message();
  f.command(climate::CLIMATE_MODE_HEAT, 18.0f, climate::CLIMATE_PRESET_NONE);
  EXPECT_TRUE(f.receive(std::vector<uint8_t>(m.begin(), m.begin() + 8)));
  EXPECT_TRUE(f.receive(std::vector<uint8_t>(m.begin() + 16, m.end())));
  EXPECT_EQ(f.climate.mode, climate::CLIMATE_MODE_COOL);
  EXPECT_FLOAT_EQ(f.climate.target_temperature, 22.5f);
  EXPECT_EQ(f.climate.preset, climate::CLIMATE_PRESET_COMFORT);
}

TEST(Daikin, BoostTimerCancelledWhenRemoteLeavesBoost) {
  Fixture f;
  f.command(climate::CLIMATE_MODE_COOL, 22.0f, climate::CLIMATE_PRESET_BOOST);
  ASSERT_EQ(f.climate.preset, climate::CLIMATE_PRESET_BOOST);
  EXPECT_TRUE(f.receive(comfort_frame(0x00)));
  EXPECT_TRUE(f.receive(state_frame(0x00, 0x04)));
  EXPECT_EQ(f.climate.preset, climate::CLIMATE_PRESET_ECO);
  // No boost timer may remain to end a boost that is already over
  EXPECT_FALSE(f.fire_boost_timer());
  EXPECT_EQ(f.climate.preset, climate::CLIMATE_PRESET_ECO);
}

TEST(Daikin, BoostTimerCancelledWhenEntityLeavesBoost) {
  Fixture f;
  f.command(climate::CLIMATE_MODE_COOL, 22.0f, climate::CLIMATE_PRESET_BOOST);
  f.command(climate::CLIMATE_MODE_COOL, 22.0f, climate::CLIMATE_PRESET_SLEEP);
  EXPECT_FALSE(f.fire_boost_timer());
  EXPECT_EQ(f.climate.preset, climate::CLIMATE_PRESET_SLEEP);
}

TEST(Daikin, ReceivedBoostEndsWhenTimerFires) {
  Fixture f;
  EXPECT_TRUE(f.receive(comfort_frame(0x00)));
  EXPECT_TRUE(f.receive(state_frame(0x01, 0x00)));
  EXPECT_EQ(f.climate.preset, climate::CLIMATE_PRESET_BOOST);
  EXPECT_TRUE(f.fire_boost_timer());
  EXPECT_EQ(f.climate.preset, climate::CLIMATE_PRESET_NONE);
}

TEST(Daikin, RepeatedBoostFrameKeepsOneTimer) {
  Fixture f;
  EXPECT_TRUE(f.receive(state_frame(0x01, 0x00)));
  EXPECT_TRUE(f.receive(state_frame(0x01, 0x00)));
  EXPECT_TRUE(f.fire_boost_timer());
  EXPECT_FALSE(f.fire_boost_timer());
  EXPECT_EQ(f.climate.preset, climate::CLIMATE_PRESET_NONE);
}

TEST(Daikin, BoostToComfortTimerDoesNotChangePreset) {
  Fixture f;
  f.command(climate::CLIMATE_MODE_COOL, 22.0f, climate::CLIMATE_PRESET_BOOST);
  EXPECT_TRUE(f.receive(comfort_frame(0x10)));
  EXPECT_TRUE(f.receive(state_frame(0x00, 0x00)));
  EXPECT_EQ(f.climate.preset, climate::CLIMATE_PRESET_COMFORT);
  // The state frame saw boost as the previous preset and cancelled the timer
  EXPECT_FALSE(f.fire_boost_timer());
  EXPECT_EQ(f.climate.preset, climate::CLIMATE_PRESET_COMFORT);
}

TEST(Daikin, ComfortFrameAloneChangesNothing) {
  Fixture f;
  EXPECT_TRUE(f.receive(state_frame(0x00, 0x04)));
  ASSERT_EQ(f.climate.preset, climate::CLIMATE_PRESET_ECO);

  // The comfort frame is only recorded until a valid state frame follows
  EXPECT_TRUE(f.receive(comfort_frame(0x10)));
  EXPECT_EQ(f.climate.preset, climate::CLIMATE_PRESET_ECO);
  std::vector<uint8_t> bad = state_frame(0x00, 0x00);
  bad[18] ^= 0x01;
  EXPECT_FALSE(f.receive(bad));
  EXPECT_EQ(f.climate.preset, climate::CLIMATE_PRESET_ECO);
  EXPECT_TRUE(f.receive(state_frame(0x00, 0x00)));
  EXPECT_EQ(f.climate.preset, climate::CLIMATE_PRESET_COMFORT);
}

TEST(Daikin, LostStateFrameKeepsComfortUntilNextTransmission) {
  Fixture f;
  EXPECT_TRUE(f.receive(comfort_frame(0x10)));
  EXPECT_TRUE(f.receive(state_frame(0x00, 0x00)));
  ASSERT_EQ(f.climate.preset, climate::CLIMATE_PRESET_COMFORT);

  // Comfort off arrives but its state frame is lost: nothing changes
  EXPECT_TRUE(f.receive(comfort_frame(0x00)));
  EXPECT_EQ(f.climate.preset, climate::CLIMATE_PRESET_COMFORT);

  // The next transmission says comfort off again and its state frame arrives
  EXPECT_TRUE(f.receive(comfort_frame(0x00)));
  EXPECT_TRUE(f.receive(state_frame(0x00, 0x00)));
  EXPECT_EQ(f.climate.preset, climate::CLIMATE_PRESET_NONE);
}

TEST(Daikin, PendingComfortExpiresAfterOneSecond) {
  Fixture f;
  // LoopBlockingGuard publishes the dispatch time the component reads as the loop clock
  { LoopBlockingGuard clock(&f.climate, LOG_STR("test"), 1000); }
  EXPECT_TRUE(f.receive(comfort_frame(0x10)));
  // The state frame of that transmission is lost; the next one arrives after the window
  { LoopBlockingGuard clock(&f.climate, LOG_STR("test"), 1000 + DAIKIN_TRANSMISSION_MS + 1); }
  EXPECT_TRUE(f.receive(state_frame(0x00, 0x00)));
  EXPECT_EQ(f.climate.preset, climate::CLIMATE_PRESET_NONE);
}

TEST(Daikin, StateFrameWithoutComfortFrameKeepsComfort) {
  Fixture f;
  EXPECT_TRUE(f.receive(comfort_frame(0x10)));
  EXPECT_TRUE(f.receive(state_frame(0x00, 0x00)));
  // A state frame whose 0xC5 frame was lost leaves a current comfort preset alone
  EXPECT_TRUE(f.receive(state_frame(0x00, 0x00)));
  EXPECT_EQ(f.climate.preset, climate::CLIMATE_PRESET_COMFORT);
}

TEST(Daikin, RestoredBoostArmsTheTimer) {
  {
    Fixture first;
    first.command(climate::CLIMATE_MODE_COOL, 22.0f, climate::CLIMATE_PRESET_BOOST);
    ASSERT_EQ(first.climate.preset, climate::CLIMATE_PRESET_BOOST);
  }
  Fixture restored(true);
  EXPECT_EQ(restored.climate.preset, climate::CLIMATE_PRESET_BOOST);
  EXPECT_EQ(restored.climate.mode, climate::CLIMATE_MODE_COOL);
  EXPECT_TRUE(App.scheduler.cancel_timeout(&restored.climate, BOOST_TIMEOUT_ID));
}

TEST(Daikin, FreshClimateHasNoBoostTimer) {
  {
    Fixture first;
    first.command(climate::CLIMATE_MODE_COOL, 22.0f, climate::CLIMATE_PRESET_BOOST);
  }
  // A fixture that clears the saved state restores nothing and arms no timer
  Fixture fresh;
  EXPECT_NE(fresh.climate.preset, climate::CLIMATE_PRESET_BOOST);
  EXPECT_FALSE(App.scheduler.cancel_timeout(&fresh.climate, BOOST_TIMEOUT_ID));
}

TEST(Daikin, RejectsBadStateChecksum) {
  Fixture f;
  f.command(climate::CLIMATE_MODE_HEAT, 20.0f, climate::CLIMATE_PRESET_SLEEP);
  std::vector<uint8_t> bad = state_frame(0x01, 0x00, 45);
  bad[18] ^= 0x01;
  EXPECT_FALSE(f.receive(bad));
  EXPECT_EQ(f.climate.mode, climate::CLIMATE_MODE_HEAT);
  EXPECT_FLOAT_EQ(f.climate.target_temperature, 20.0f);
  EXPECT_EQ(f.climate.preset, climate::CLIMATE_PRESET_SLEEP);
  EXPECT_FALSE(f.fire_boost_timer());
}

TEST(Daikin, RejectsBadComfortChecksum) {
  Fixture f;
  std::vector<uint8_t> bad = comfort_frame(0x10);
  bad[7] ^= 0x01;
  EXPECT_FALSE(f.receive(bad));
  EXPECT_TRUE(f.receive(state_frame(0x00, 0x00)));
  EXPECT_EQ(f.climate.preset, climate::CLIMATE_PRESET_NONE);
}

}  // namespace esphome::daikin::testing
