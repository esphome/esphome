#include <gtest/gtest.h>

#include "esphome/components/pioneer_wt/pioneer_wt.h"
#include "esphome/components/pioneer_wt/pioneer_wt_protocol.h"
#include "esphome/core/application.h"
#include "../common.h"

namespace esphome::pioneer_wt::testing {

namespace {

using Frame = std::vector<uint8_t>;

// Frames captured between a Pioneer WT indoor unit and its stock WiFi module.
// Full status the AC sent at boot: on, heat, 77 F, indoor 21.7 C, fan auto, swing stopped, eco off.
const char *const STATUS_DUMP = "a5010121a50000428a930c0c0001010002000009c400030000087a000500000c00000d00000e0800110800"
                                "120400df0000c902001700001e0102270000004d014801";
// The AC's once-a-minute "are you there?" check.
const char *const PRESENCE_CHECK = "a50100210000000cd37d0c0c";
const char *const PRESENCE_REPLY = "a5010023000000124709800c0264ffffffd0";

// payload starts with the two class bytes.
Frame build(uint8_t flags, uint8_t type, uint8_t seq, uint8_t ack, const Frame &payload) {
  uint8_t frame[MAX_FRAME_SIZE];
  const size_t len =
      build_frame(frame, flags, type, seq, ack, payload[0], payload[1], payload.data() + 2, payload.size() - 2);
  return Frame(frame, frame + len);
}

Frame status_request(uint8_t seq) {
  return build(FLAG_WANT_ACK, TYPE_DATA, seq, 0, {CLASS_NETWORK, CLASS_NETWORK, 0xff, 0xff});
}

Frame command(uint8_t seq, const Frame &attrs) {
  Frame payload{CLASS_COMMAND, CLASS_COMMAND};
  payload.insert(payload.end(), attrs.begin(), attrs.end());
  return build(FLAG_WANT_ACK, TYPE_DATA, seq, 0, payload);
}

size_t feed_all(PacketReader &reader, const Frame &bytes) {
  size_t last = 0;
  for (uint8_t b : bytes) {
    if (size_t len = reader.feed(b))
      last = len;
  }
  return last;
}

}  // namespace

// Frames built here must match what the stock module sent, byte for byte.
TEST(PioneerWTProtocol, BuildsCapturedCommands) {
  EXPECT_EQ(command(0x09, {0x00, 0x01, 0x00}), from_hex("a50101210900000f4f990a0a000100"));
  EXPECT_EQ(command(0x0b, {0x00, 0x12, 0x03}), from_hex("a50101210b00000fefbd0a0a001203"));
  EXPECT_EQ(command(0x29, {0x00, 0x1e, 0x00}), from_hex("a50101212900000ffb280a0a001e00"));
  EXPECT_EQ(command(0x2b, {0x00, 0x25, 0x00}), from_hex("a50101212b00000fe4200a0a002500"));
  EXPECT_EQ(command(0x10, {0x02, 0x27, 0x00, 0x00, 0x00, 0x3f}), from_hex("a50101211000001238620a0a02270000003f"));
  EXPECT_EQ(status_request(0x01), from_hex("a50101210100000e4ae90b0bffff"));
}

TEST(PioneerWTProtocol, BuildsCapturedReplies) {
  EXPECT_EQ(build(FLAG_WANT_ACK, TYPE_REPLY, 0, 0xb7, {0x80, CLASS_STATUS}), from_hex("a501012300b7000ce352800c"));
  EXPECT_EQ(build(FLAG_NO_ACK, TYPE_REPLY, 0, 0, {0x80, CLASS_STATUS, 0x02, 0x64, 0xff, 0xff, 0xff, 0xd8}),
            from_hex("a501002300000012c601800c0264ffffffd8"));
}

TEST(PioneerWTProtocol, ReaderAcceptsCapturedFrame) {
  PacketReader reader;
  const Frame frame = from_hex(STATUS_DUMP);
  EXPECT_EQ(feed_all(reader, frame), frame.size());
}

TEST(PioneerWTProtocol, ReaderSkipsNoiseAndStrayHeaderBytes) {
  PacketReader reader;
  Frame bytes = from_hex("00ffa5a5a502");
  const Frame frame = from_hex(PRESENCE_CHECK);
  bytes.insert(bytes.end(), frame.begin(), frame.end());
  EXPECT_EQ(feed_all(reader, bytes), frame.size());
}

TEST(PioneerWTProtocol, ReaderDropsBadChecksum) {
  PacketReader reader;
  Frame frame = from_hex(PRESENCE_CHECK);
  frame[9] ^= 0x01;
  EXPECT_EQ(feed_all(reader, frame), 0u);
  EXPECT_TRUE(reader.take_checksum_error());
  EXPECT_FALSE(reader.take_checksum_error());
  // The next good frame is still accepted.
  EXPECT_EQ(feed_all(reader, from_hex(PRESENCE_CHECK)), 12u);
}

TEST(PioneerWTProtocol, ReaderRejectsImpossibleLength) {
  PacketReader reader;
  EXPECT_EQ(feed_all(reader, from_hex("a501012100000005")), 0u);
  EXPECT_EQ(feed_all(reader, from_hex(PRESENCE_CHECK)), 12u);
}

TEST(PioneerWTProtocol, AttributesOfStatusDump) {
  const Frame frame = from_hex(STATUS_DUMP);
  AttributeReader attrs(frame.data() + HEADER_SIZE + 2, frame.size() - HEADER_SIZE - 2);
  std::vector<std::pair<uint16_t, uint32_t>> seen;
  while (attrs.next())
    seen.emplace_back(attrs.id(), attrs.value());
  EXPECT_FALSE(attrs.stopped_on_unknown());
  ASSERT_EQ(seen.size(), 15u);
  EXPECT_EQ(seen[0], std::make_pair(ATTR_POWER, 1u));
  EXPECT_EQ(seen[1], std::make_pair(ATTR_SETPOINT_C100, 2500u));
  EXPECT_EQ(seen[2], std::make_pair(ATTR_INDOOR_TEMP_C100, 2170u));
  EXPECT_EQ(seen[8], std::make_pair(ATTR_MODE, 4u));
  EXPECT_EQ(seen[13], std::make_pair(ATTR_SETPOINT_F, 77u));
}

TEST(PioneerWTProtocol, AttributesWithCountPrefix) {
  // 0038 carries a count byte: empty, then one byte of data, then the next attribute.
  const Frame body = from_hex("00380000380122000101");
  AttributeReader attrs(body.data(), body.size());
  ASSERT_TRUE(attrs.next());
  EXPECT_EQ(attrs.id(), 0x0038);
  ASSERT_TRUE(attrs.next());
  EXPECT_EQ(attrs.id(), 0x0038);
  ASSERT_TRUE(attrs.next());
  EXPECT_EQ(attrs.id(), ATTR_POWER);
  EXPECT_EQ(attrs.value(), 1u);
  EXPECT_FALSE(attrs.next());
  EXPECT_FALSE(attrs.stopped_on_unknown());
}

TEST(PioneerWTProtocol, AttributesStopAtUnknownId) {
  const Frame body = from_hex("000101abcd00001200");
  AttributeReader attrs(body.data(), body.size());
  ASSERT_TRUE(attrs.next());
  EXPECT_EQ(attrs.id(), ATTR_POWER);
  EXPECT_FALSE(attrs.next());
  EXPECT_TRUE(attrs.stopped_on_unknown());
  EXPECT_EQ(attrs.id(), 0xabcd);
}

// A cut-off frame, not an unknown ID: a four-byte value with three bytes, and 0147 with one of its two length bytes.
TEST(PioneerWTProtocol, AttributesStopAtTruncatedValue) {
  for (const char *hex : {"0227000000", "014700"}) {
    const Frame body = from_hex(hex);
    AttributeReader attrs(body.data(), body.size());
    EXPECT_FALSE(attrs.next()) << hex;
    EXPECT_FALSE(attrs.stopped_on_unknown()) << hex;
  }
}

class PioneerWTTest : public ::testing::Test {
 protected:
  void SetUp() override { this->sut_.set_uart_parent(&this->uart_); }

  void loop_at_(uint32_t now) {
    LoopBlockingGuard{nullptr, nullptr, now};
    this->sut_.loop();
  }

  // Starts the component and acknowledges its boot request for the full status.
  void start_() {
    this->sut_.setup();
    this->loop_at_(1000);
    this->uart_.tx.clear();
    this->uart_.push_rx(build(FLAG_WANT_ACK, TYPE_REPLY, 0, 0x01, {0x80, CLASS_NETWORK}));
    this->loop_at_(1010);
  }

  // Starts the component, applies the captured full status and clears what was sent so far.
  void start_with_status_() {
    this->start_();
    this->uart_.push_rx_hex(STATUS_DUMP);
    this->loop_at_(1100);
    this->uart_.tx.clear();
  }

  // Lets the boot request for the full status run out of attempts.
  void exhaust_boot_request_() {
    this->sut_.setup();
    for (uint32_t t = 1000; t <= 4000; t += 500)
      this->loop_at_(t);
  }

  MockUARTComponent uart_;
  PioneerWT sut_;
};

TEST_F(PioneerWTTest, ResendsUntilAcknowledgedThenGivesUp) {
  this->sut_.setup();
  this->loop_at_(1000);
  const Frame request = status_request(0x01);
  EXPECT_EQ(this->uart_.tx, request);
  this->loop_at_(1200);  // too soon to resend
  EXPECT_EQ(this->uart_.tx.size(), request.size());
  for (uint32_t t = 1500; t <= 2500; t += 500)
    this->loop_at_(t);
  EXPECT_EQ(this->uart_.tx.size(), request.size() * 4);
  this->loop_at_(3000);  // fourth attempt was the last
  this->loop_at_(3500);
  EXPECT_EQ(this->uart_.tx.size(), request.size() * 4);
}

TEST_F(PioneerWTTest, AppliesStatusDumpAndAcknowledges) {
  this->start_();
  this->uart_.push_rx_hex(STATUS_DUMP);
  this->loop_at_(1100);
  EXPECT_EQ(this->uart_.tx, from_hex("a501012300a5000ca38b800c"));
  EXPECT_EQ(this->sut_.mode, climate::CLIMATE_MODE_HEAT);
  EXPECT_NEAR(this->sut_.target_temperature, 25.0f, 0.01f);
  EXPECT_NEAR(this->sut_.current_temperature, 21.7f, 0.01f);
  ASSERT_TRUE(this->sut_.fan_mode.has_value());
  EXPECT_EQ(*this->sut_.fan_mode, climate::CLIMATE_FAN_AUTO);
  EXPECT_EQ(this->sut_.swing_mode, climate::CLIMATE_SWING_OFF);
}

TEST_F(PioneerWTTest, AnswersPresenceCheck) {
  this->start_();
  this->uart_.push_rx_hex(PRESENCE_CHECK);
  this->loop_at_(1100);
  EXPECT_EQ(this->uart_.tx, from_hex(PRESENCE_REPLY));
}

TEST_F(PioneerWTTest, PublishesOnlyWhenTheReportChangesSomething) {
  this->start_with_status_();
  int published = 0;
  this->sut_.add_on_state_callback([&published](climate::Climate & /*climate*/) { published++; });
  this->uart_.push_rx_hex(STATUS_DUMP);
  this->loop_at_(1200);
  EXPECT_EQ(published, 0);
  this->uart_.push_rx(build(FLAG_WANT_ACK, TYPE_DATA, 0xb0, 0, {CLASS_STATUS, CLASS_STATUS, 0x00, 0x05, 0x03}));
  this->loop_at_(1300);
  EXPECT_EQ(published, 1);
}

TEST_F(PioneerWTTest, ReportsFanSpeedAndPowerOff) {
  this->start_();
  this->uart_.push_rx_hex(STATUS_DUMP);
  this->loop_at_(1100);
  // Fan speed 3, then power off.
  this->uart_.push_rx(build(FLAG_WANT_ACK, TYPE_DATA, 0xb0, 0, {CLASS_STATUS, CLASS_STATUS, 0x00, 0x05, 0x03}));
  this->uart_.push_rx(build(FLAG_WANT_ACK, TYPE_DATA, 0xb1, 0, {CLASS_STATUS, CLASS_STATUS, 0x00, 0x01, 0x00}));
  this->loop_at_(1200);
  EXPECT_EQ(this->sut_.get_custom_fan_mode(), "Speed 3");
  EXPECT_EQ(this->sut_.mode, climate::CLIMATE_MODE_OFF);
}

TEST_F(PioneerWTTest, ModeChangeWhileOnSendsModeOnly) {
  this->start_with_status_();
  this->sut_.make_call().set_mode(climate::CLIMATE_MODE_COOL).perform();
  this->loop_at_(1200);
  EXPECT_EQ(this->uart_.tx, from_hex("a50101210200000f0d230a0a001201"));
  // State follows the AC's report, not the request.
  EXPECT_EQ(this->sut_.mode, climate::CLIMATE_MODE_HEAT);
}

TEST_F(PioneerWTTest, TurnOnSendsPowerModeAndSetpoint) {
  this->start_();
  this->uart_.tx.clear();
  this->sut_.make_call().set_mode(climate::CLIMATE_MODE_HEAT).set_target_temperature(20.0f).perform();
  this->loop_at_(1200);
  EXPECT_EQ(this->uart_.tx, from_hex("a50101210200001869400a0a000101001204022700000044"));
}

TEST_F(PioneerWTTest, OffSendsPowerOnly) {
  this->start_with_status_();
  this->sut_.make_call().set_mode(climate::CLIMATE_MODE_OFF).perform();
  this->loop_at_(1200);
  EXPECT_EQ(this->uart_.tx, from_hex("a50101210200000f4b220a0a000100"));
}

TEST_F(PioneerWTTest, FanSpeedSentLikeTheApp) {
  this->start_();
  this->uart_.tx.clear();
  this->sut_.make_call().set_fan_mode("Speed 3").perform();
  this->loop_at_(1200);
  EXPECT_EQ(this->uart_.tx, from_hex("a501012102000018ac1f0a0a022400022500000503007300"));
}

TEST_F(PioneerWTTest, DisplayAndBeeperCommands) {
  this->start_();
  this->uart_.tx.clear();
  this->sut_.set_display_on(false);
  this->loop_at_(1200);
  EXPECT_EQ(this->uart_.tx, command(0x02, {0x00, 0x1e, 0x00}));
  this->uart_.tx.clear();
  this->uart_.push_rx(build(FLAG_WANT_ACK, TYPE_REPLY, 0, 0x02, {0x80, CLASS_COMMAND}));
  this->sut_.set_beeper_on(true);
  this->loop_at_(1300);
  EXPECT_EQ(this->uart_.tx, command(0x03, {0x00, 0x25, 0x01}));
}

// The request for the full status at boot can be lost when the AC powers up at the same time.
TEST_F(PioneerWTTest, AsksAgainUntilFullStatusArrives) {
  this->exhaust_boot_request_();
  // The AC is alive: it sends its presence check and a small report, but never the full status.
  this->uart_.push_rx_hex(PRESENCE_CHECK);
  this->uart_.push_rx(build(FLAG_NO_ACK, TYPE_DATA, 0xb0, 0, {CLASS_STATUS, CLASS_STATUS, 0x00, 0x05, 0x03}));
  this->loop_at_(20000);
  this->uart_.tx.clear();
  this->loop_at_(29000);
  EXPECT_TRUE(this->uart_.tx.empty());
  this->loop_at_(30000);
  this->loop_at_(30010);
  EXPECT_EQ(this->uart_.tx, status_request(0x02));
}

TEST_F(PioneerWTTest, PresenceCheckDoesNotCountAsStatus) {
  this->start_();
  this->uart_.push_rx_hex(STATUS_DUMP);
  this->loop_at_(1100);
  // Presence checks keep arriving, but no status for five minutes.
  for (uint32_t t = 60000; t <= 300000; t += 60000) {
    this->uart_.push_rx_hex(PRESENCE_CHECK);
    this->loop_at_(t);
  }
  this->uart_.tx.clear();
  this->loop_at_(301100);
  this->loop_at_(301110);
  EXPECT_EQ(this->uart_.tx, status_request(0x02));
}

TEST_F(PioneerWTTest, WarningStaysUntilAcknowledgement) {
  this->exhaust_boot_request_();
  EXPECT_TRUE(this->sut_.status_has_warning());
  // Sending the next command does not prove the link works.
  this->sut_.make_call().set_mode(climate::CLIMATE_MODE_COOL).perform();
  this->loop_at_(5000);
  EXPECT_TRUE(this->sut_.status_has_warning());
  this->uart_.push_rx(build(FLAG_WANT_ACK, TYPE_REPLY, 0, 0x02, {0x80, CLASS_COMMAND}));
  this->loop_at_(5100);
  EXPECT_FALSE(this->sut_.status_has_warning());
}

TEST_F(PioneerWTTest, DropsPartialFrameAfterPause) {
  this->start_();
  // A cut-off header that claims 255 bytes would otherwise swallow the frames that follow.
  this->uart_.push_rx_hex("a5010121000000ff");
  this->loop_at_(1100);
  this->loop_at_(1300);  // idle pass after the gap
  this->uart_.push_rx_hex(PRESENCE_CHECK);
  this->loop_at_(2000);
  EXPECT_EQ(this->uart_.tx, from_hex(PRESENCE_REPLY));
}

TEST_F(PioneerWTTest, KeepsPartialFrameAfterSlowLoop) {
  this->start_();
  const std::string check = PRESENCE_CHECK;
  this->uart_.push_rx_hex(check.substr(0, 10));
  this->loop_at_(1100);
  // The loop was busy; the rest of the frame is already waiting when it runs again.
  this->uart_.push_rx_hex(check.substr(10));
  this->loop_at_(1500);
  EXPECT_EQ(this->uart_.tx, from_hex(PRESENCE_REPLY));
}

TEST_F(PioneerWTTest, FailedPowerOnIsSentAgainWithTheNextMode) {
  this->start_();  // no status yet: the AC is off
  this->uart_.tx.clear();
  this->sut_.make_call().set_mode(climate::CLIMATE_MODE_HEAT).perform();
  for (uint32_t t = 1100; t <= 2600; t += 500)
    this->loop_at_(t);   // four attempts, no acknowledgement
  this->loop_at_(3100);  // given up
  this->uart_.tx.clear();
  this->sut_.make_call().set_mode(climate::CLIMATE_MODE_COOL).perform();
  this->loop_at_(3200);
  EXPECT_EQ(this->uart_.tx, command(0x03, {0x00, 0x01, 0x01, 0x00, 0x12, 0x01}));
}

TEST_F(PioneerWTTest, ModeAfterQueuedOffTurnsBackOn) {
  this->start_with_status_();
  // Both calls are made before the AC reports that it is off.
  this->sut_.make_call().set_mode(climate::CLIMATE_MODE_OFF).perform();
  this->sut_.make_call().set_mode(climate::CLIMATE_MODE_COOL).perform();
  this->loop_at_(1200);
  EXPECT_EQ(this->uart_.tx, command(0x02, {0x00, 0x01, 0x00}));
  this->uart_.tx.clear();
  this->uart_.push_rx(build(FLAG_WANT_ACK, TYPE_REPLY, 0, 0x02, {0x80, CLASS_COMMAND}));
  this->loop_at_(1300);
  EXPECT_EQ(this->uart_.tx, command(0x03, {0x00, 0x01, 0x01, 0x00, 0x12, 0x01}));
}

TEST_F(PioneerWTTest, ReportOnDuringQueuedOffKeepsTheOff) {
  this->start_with_status_();
  this->sut_.make_call().set_mode(climate::CLIMATE_MODE_OFF).perform();
  // A report sent before the AC got the off must not make the next mode change skip the power on.
  this->uart_.push_rx(build(FLAG_NO_ACK, TYPE_DATA, 0xb0, 0, {CLASS_STATUS, CLASS_STATUS, 0x00, 0x01, 0x01}));
  this->loop_at_(1200);
  this->sut_.make_call().set_mode(climate::CLIMATE_MODE_COOL).perform();
  this->uart_.tx.clear();
  this->uart_.push_rx(build(FLAG_WANT_ACK, TYPE_REPLY, 0, 0x02, {0x80, CLASS_COMMAND}));
  this->loop_at_(1300);
  EXPECT_EQ(this->uart_.tx, command(0x03, {0x00, 0x01, 0x01, 0x00, 0x12, 0x01}));
}

TEST_F(PioneerWTTest, EcoAndAutoFanCommand) {
  this->start_();
  this->uart_.tx.clear();
  this->sut_.make_call().set_fan_mode(climate::CLIMATE_FAN_AUTO).set_preset(climate::CLIMATE_PRESET_ECO).perform();
  this->loop_at_(1200);
  EXPECT_EQ(this->uart_.tx,
            command(0x02, {0x02, 0x24, 0x00, 0x02, 0x25, 0x00, 0x00, 0x05, 0x00, 0x00, 0x73, 0x01, 0x00, 0x13, 0x01}));
}

TEST_F(PioneerWTTest, ReportsEcoAndSwing) {
  this->start_with_status_();
  this->uart_.push_rx(
      build(FLAG_NO_ACK, TYPE_DATA, 0xb0, 0, {CLASS_STATUS, CLASS_STATUS, 0x00, 0x13, 0x01, 0x00, 0x11, SWING_ON}));
  this->loop_at_(1200);
  ASSERT_TRUE(this->sut_.preset.has_value());
  EXPECT_EQ(*this->sut_.preset, climate::CLIMATE_PRESET_ECO);
  EXPECT_EQ(this->sut_.swing_mode, climate::CLIMATE_SWING_VERTICAL);
}

TEST_F(PioneerWTTest, RemoteOffWhileASettingIsQueuedIsFollowed) {
  this->start_with_status_();
  this->sut_.set_display_on(false);
  // The handheld remote turns the AC off while the display command waits for its acknowledgement.
  this->uart_.push_rx(build(FLAG_NO_ACK, TYPE_DATA, 0xb0, 0, {CLASS_STATUS, CLASS_STATUS, 0x00, 0x01, 0x00}));
  this->uart_.push_rx(build(FLAG_WANT_ACK, TYPE_REPLY, 0, 0x02, {0x80, CLASS_COMMAND}));
  this->loop_at_(1200);
  this->uart_.tx.clear();
  this->sut_.make_call().set_mode(climate::CLIMATE_MODE_COOL).perform();
  this->loop_at_(1300);
  EXPECT_EQ(this->uart_.tx, command(0x03, {0x00, 0x01, 0x01, 0x00, 0x12, 0x01}));
}

TEST_F(PioneerWTTest, DroppingAnotherCommandKeepsAQueuedOff) {
  this->start_with_status_();
  this->sut_.set_display_on(false);
  this->sut_.make_call().set_mode(climate::CLIMATE_MODE_OFF).perform();
  // The display command gets no acknowledgement and is dropped; the off is still queued.
  for (uint32_t t = 1200; t <= 2700; t += 500)
    this->loop_at_(t);
  this->loop_at_(3200);
  this->sut_.make_call().set_mode(climate::CLIMATE_MODE_COOL).perform();
  this->uart_.tx.clear();
  this->uart_.push_rx(build(FLAG_WANT_ACK, TYPE_REPLY, 0, 0x03, {0x80, CLASS_COMMAND}));
  this->loop_at_(3300);
  EXPECT_EQ(this->uart_.tx, command(0x04, {0x00, 0x01, 0x01, 0x00, 0x12, 0x01}));
}

TEST_F(PioneerWTTest, PowerOnDroppedForRoomIsSentAgain) {
  this->start_();  // no status yet: the AC is off
  this->sut_.make_call().set_mode(climate::CLIMATE_MODE_HEAT).perform();
  for (uint8_t i = 0; i < 3; i++)
    this->sut_.set_display_on(i % 2);
  this->uart_.tx.clear();
  // The queue is full; the power on at its head makes room for this call.
  this->sut_.make_call().set_mode(climate::CLIMATE_MODE_COOL).perform();
  this->loop_at_(1100);
  EXPECT_EQ(this->uart_.tx, command(0x03, {0x00, 0x1e, 0x00}));
  for (uint8_t seq = 0x03; seq <= 0x05; seq++) {
    this->uart_.push_rx(build(FLAG_WANT_ACK, TYPE_REPLY, 0, seq, {0x80, CLASS_COMMAND}));
    this->loop_at_(1100 + seq * 100);
  }
  EXPECT_EQ(Frame(this->uart_.tx.end() - 18, this->uart_.tx.end()),
            command(0x06, {0x00, 0x01, 0x01, 0x00, 0x12, 0x01}));
}

TEST_F(PioneerWTTest, ClampsTargetTemperature) {
  this->start_();
  this->uart_.tx.clear();
  this->sut_.make_call().set_target_temperature(-20.0f).perform();
  this->loop_at_(1200);
  // 16 C is 60.8 F, sent as 61 F.
  EXPECT_EQ(this->uart_.tx, command(0x02, {0x02, 0x27, 0x00, 0x00, 0x00, 0x3d}));
}

TEST_F(PioneerWTTest, FullQueueDropsOldestCommand) {
  this->start_();
  this->uart_.tx.clear();
  // Five commands with no acknowledgement; the queue holds four.
  for (uint8_t i = 1; i <= 5; i++) {
    this->sut_.make_call()
        .set_swing_mode(i % 2 ? climate::CLIMATE_SWING_VERTICAL : climate::CLIMATE_SWING_OFF)
        .perform();
  }
  this->loop_at_(1200);
  // The first command (seq 2) was dropped, so the second (seq 3, swing off) goes out first.
  EXPECT_EQ(this->uart_.tx, command(0x03, {0x00, 0x11, SWING_OFF}));
}

}  // namespace esphome::pioneer_wt::testing
