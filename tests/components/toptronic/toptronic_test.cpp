#include <gtest/gtest.h>

#include <algorithm>
#include <vector>

#include "esphome/components/toptronic/toptronic.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"

namespace esphome::toptronic::testing {

namespace {

constexpr uint32_t START_ID = 0x1Fu << 24;
constexpr uint32_t CONT_ID = 0x10u << 24;
constexpr uint8_t TAG = 0x2A;
// Sender field of the hub under test (0x208)
constexpr uint32_t SENDER_BITS = 0x208u << 11;

class NullCanbus : public canbus::Canbus {
 public:
  // Queues a received frame, handed to the callbacks on loop()
  void push(uint32_t can_id, bool rtr, const std::vector<uint8_t> &data) {
    canbus::CanFrame frame{};
    frame.use_extended_id = true;
    frame.remote_transmission_request = rtr;
    frame.can_id = can_id;
    frame.can_data_length_code = data.size();
    std::copy(data.begin(), data.end(), frame.data);
    this->received_.push_back(frame);
  }

  canbus::CanFrame last_sent{};
  std::vector<canbus::CanFrame> sent;
  bool fail{false};
  // Lets a test count the callbacks a hub registered on this bus
  size_t callback_count() const { return this->callback_manager_.size(); }

 protected:
  bool setup_internal() override { return true; }
  canbus::Error send_message(canbus::CanFrame *frame) override {
    if (this->fail)
      return canbus::ERROR_FAIL;
    this->last_sent = *frame;
    this->sent.push_back(*frame);
    return canbus::ERROR_OK;
  }
  canbus::Error read_message(canbus::CanFrame *frame) override {
    if (this->received_.empty())
      return canbus::ERROR_NOMSG;
    *frame = this->received_.front();
    this->received_.erase(this->received_.begin());
    return canbus::ERROR_OK;
  }

  std::vector<canbus::CanFrame> received_;
};

class TestableTopTronic : public TopTronic {
 public:
  explicit TestableTopTronic(canbus::Canbus *bus, uint16_t device_id = 0x208) : TopTronic(bus, device_id) {}

  // Feeds one frame; returns the completed payload, or an empty vector when nothing completed
  std::vector<uint8_t> feed(uint32_t can_id, const std::vector<uint8_t> &frame) {
    const uint8_t *msg;
    size_t len;
    if (!this->parse_frame_(frame.data(), frame.size(), can_id, &msg, &len))
      return {};
    return std::vector<uint8_t>(msg, msg + len);
  }

  void interpret(const std::vector<uint8_t> &msg) { this->interpret_message_(msg.data(), msg.size()); }
  void drain() { this->drain_refresh_(); }
  bool refreshing() const { return this->refresh_in_progress_; }
  static void forget_hubs() {
    hubs.clear();
    boot_refresh_done = false;
    debug_logs = {};
    last_candump_log_ms = 0;
    candump_dropped = 0;
  }
  static void log_frame(uint32_t can_id, const std::vector<uint8_t> &frame, bool sent) {
    log_debug_frame(can_id, frame.data(), frame.size(), sent);
  }
  static bool single_frame(uint32_t can_id, const std::vector<uint8_t> &frame) {
    return is_single_frame_message(can_id, frame.data(), frame.size());
  }
  static uint32_t dropped() { return candump_dropped; }

  size_t live_slots(uint32_t now) const {
    size_t n = 0;
    for (const auto &s : this->slots_)
      n += s.is_live(now, this->max_pending_age_ms_) ? 1 : 0;
    return n;
  }
};

class TopTronicTest : public ::testing::Test {
 protected:
  void TearDown() override { TestableTopTronic::forget_hubs(); }

  void set_now_(uint32_t now) {
    this->now_ = now;
    LoopBlockingGuard{nullptr, nullptr, now};
  }

  // Splits payload + CRC into a start frame and continuation frames
  static std::vector<std::vector<uint8_t>> split(std::vector<uint8_t> payload, bool good_crc = true) {
    uint16_t crc = crc16(payload.data(), payload.size(), 0x600D, 0x8408);
    if (!good_crc)
      crc ^= 1;
    payload.push_back(crc >> 8);
    payload.push_back(crc & 0xFF);
    std::vector<std::vector<uint8_t>> frames;
    size_t pos = std::min<size_t>(6, payload.size());
    frames.push_back({0, TAG});
    frames[0].insert(frames[0].end(), payload.begin(), payload.begin() + pos);
    while (pos < payload.size()) {
      size_t n = std::min<size_t>(7, payload.size() - pos);
      std::vector<uint8_t> f{TAG};
      f.insert(f.end(), payload.begin() + pos, payload.begin() + pos + n);
      frames.push_back(f);
      pos += n;
    }
    frames[0][0] = frames.size() << 3;
    return frames;
  }

  NullCanbus bus_;
  TestableTopTronic hub_{&this->bus_};
  uint32_t now_{1000};
};

#ifdef USE_SWITCH
// What switch.py generates: the switch forwards to the hub, which publishes the state
void apply_candump(switch_::Switch *, bool state) { TopTronic::set_candump_enabled(state); }
void apply_find_can_id(switch_::Switch *, bool state) { TopTronic::set_find_can_id_enabled(state); }
#endif

const std::vector<uint8_t> MESSAGE{0x42, 0x01, 0x02, 0x00, 0x10, 0xAA, 0xBB, 0xCC, 0xDD};

// Wire bytes of a two frame response in the order they appear on the bus. The CRC is a literal computed with a
// standalone CRC-16/CCITT reference implementation (poly 0x1021, init 0xB006, reflected), so a wrong init value,
// polynomial or byte order in the hub's own check cannot match itself here.
const std::vector<uint8_t> WIRE_START{0x10, TAG, 0x42, 0x01, 0x02, 0x9E, 0xCB, 0x0A};
const std::vector<uint8_t> WIRE_CONT{TAG, 0x2B, 0x4A, 0xF0};
const std::vector<uint8_t> WIRE_MESSAGE{0x42, 0x01, 0x02, 0x9E, 0xCB, 0x0A, 0x2B};

}  // namespace

TEST_F(TopTronicTest, SingleFrameMessageIsPassedThrough) {
  this->set_now_(1000);
  EXPECT_EQ(this->hub_.feed(START_ID, {0x00, 0x42, 0x01, 0x02, 0x00, 0x10, 0x07}),
            (std::vector<uint8_t>{0x42, 0x01, 0x02, 0x00, 0x10, 0x07}));
}

TEST_F(TopTronicTest, MultiFrameMessageWithValidCrcCompletes) {
  this->set_now_(1000);
  auto frames = split(MESSAGE);
  ASSERT_EQ(frames.size(), 2u);
  EXPECT_TRUE(this->hub_.feed(START_ID, frames[0]).empty());
  EXPECT_EQ(this->hub_.live_slots(1000), 1u);
  EXPECT_EQ(this->hub_.feed(CONT_ID, frames[1]), MESSAGE);
  EXPECT_EQ(this->hub_.live_slots(1000), 0u);
}

TEST_F(TopTronicTest, BadCrcIsDroppedAndFreesTheSlot) {
  this->set_now_(1000);
  auto frames = split(MESSAGE, false);
  EXPECT_TRUE(this->hub_.feed(START_ID, frames[0]).empty());
  EXPECT_TRUE(this->hub_.feed(CONT_ID, frames[1]).empty());
  EXPECT_EQ(this->hub_.live_slots(1000), 0u);
}

TEST_F(TopTronicTest, FixedWireBytesWithTheirCrcComplete) {
  this->set_now_(1000);
  EXPECT_TRUE(this->hub_.feed(START_ID, WIRE_START).empty());
  EXPECT_EQ(this->hub_.feed(CONT_ID, WIRE_CONT), WIRE_MESSAGE);
}

TEST_F(TopTronicTest, SwappedCrcBytesAreRejected) {
  this->set_now_(1000);
  EXPECT_TRUE(this->hub_.feed(START_ID, WIRE_START).empty());
  // The two CRC bytes in the other order: pins the big endian placement on the wire
  EXPECT_TRUE(this->hub_.feed(CONT_ID, {TAG, 0x2B, 0xF0, 0x4A}).empty());
  EXPECT_EQ(this->hub_.live_slots(1000), 0u);
}

TEST_F(TopTronicTest, StaleSlotIsNotCompletedAndIsReclaimed) {
  this->set_now_(1000);
  auto frames = split(MESSAGE);
  EXPECT_TRUE(this->hub_.feed(START_ID, frames[0]).empty());
  this->set_now_(7000);
  EXPECT_TRUE(this->hub_.feed(CONT_ID, frames[1]).empty());
  EXPECT_EQ(this->hub_.live_slots(7000), 0u);
  // A fresh start with the same tag reuses the reclaimed slot and completes
  EXPECT_TRUE(this->hub_.feed(START_ID, frames[0]).empty());
  EXPECT_EQ(this->hub_.feed(CONT_ID, frames[1]), MESSAGE);
}

TEST_F(TopTronicTest, OldestSlotIsEvictedWhenAllAreBusy) {
  this->set_now_(1000);
  for (uint8_t tag : {1, 2, 3}) {
    EXPECT_TRUE(this->hub_.feed(START_ID, {3 << 3, tag, 0x42}).empty());
    this->set_now_(this->now_ + 10);
  }
  // Tag 1 was the oldest, so its continuation no longer matches a slot; tag 2 still does
  EXPECT_TRUE(this->hub_.feed(CONT_ID, {1, 0x00}).empty());
  EXPECT_EQ(this->hub_.live_slots(this->now_), 2u);
  EXPECT_TRUE(this->hub_.feed(CONT_ID, {2, 0x00}).empty());
  EXPECT_EQ(this->hub_.live_slots(this->now_), 2u);
}

TEST_F(TopTronicTest, FramesBeyondTheBufferAreDropped) {
  this->set_now_(1000);
  // 31 frames announced, but oversized frames overflow the buffer before the count runs out
  EXPECT_TRUE(this->hub_.feed(START_ID, {31 << 3, TAG, 0x42}).empty());
  std::vector<uint8_t> big(100, 0x55);
  big[0] = TAG;
  EXPECT_TRUE(this->hub_.feed(CONT_ID, big).empty());
  EXPECT_TRUE(this->hub_.feed(CONT_ID, big).empty());
  EXPECT_TRUE(this->hub_.feed(CONT_ID, big).empty());
  EXPECT_EQ(this->hub_.live_slots(1000), 0u);
}

TEST_F(TopTronicTest, FrameCountAboveTheLimitIsRejected) {
  this->set_now_(1000);
  this->hub_.set_max_frames_per_message(3);
  EXPECT_TRUE(this->hub_.feed(START_ID, {4 << 3, TAG, 0x42}).empty());
  EXPECT_EQ(this->hub_.live_slots(1000), 0u);
  EXPECT_TRUE(this->hub_.feed(START_ID, {3 << 3, TAG, 0x42}).empty());
  EXPECT_EQ(this->hub_.live_slots(1000), 1u);
}

TEST_F(TopTronicTest, MaxPendingMessagesLimitsTheSlotsInUse) {
  this->set_now_(1000);
  this->hub_.set_max_pending_messages(1);
  auto frames = split(MESSAGE);
  EXPECT_TRUE(this->hub_.feed(START_ID, frames[0]).empty());
  // A second message takes the only slot, so the first can no longer complete
  EXPECT_TRUE(this->hub_.feed(START_ID, {3 << 3, 0x01, 0x42}).empty());
  EXPECT_EQ(this->hub_.live_slots(1000), 1u);
  EXPECT_TRUE(this->hub_.feed(CONT_ID, frames[1]).empty());
}

TEST_F(TopTronicTest, MaxPendingAgeIsConfigurable) {
  this->set_now_(1000);
  this->hub_.set_max_pending_age_ms(10000);
  auto frames = split(MESSAGE);
  EXPECT_TRUE(this->hub_.feed(START_ID, frames[0]).empty());
  // Past the 5 s default but within the configured age
  this->set_now_(9000);
  EXPECT_EQ(this->hub_.feed(CONT_ID, frames[1]), MESSAGE);
  this->hub_.set_max_pending_age_ms(100);
  this->set_now_(10000);
  EXPECT_TRUE(this->hub_.feed(START_ID, frames[0]).empty());
  this->set_now_(10200);
  EXPECT_TRUE(this->hub_.feed(CONT_ID, frames[1]).empty());
}

TEST_F(TopTronicTest, FrameCountOfOneIsRejected) {
  this->set_now_(1000);
  EXPECT_TRUE(this->hub_.feed(START_ID, {1 << 3, TAG, 0x42}).empty());
  EXPECT_EQ(this->hub_.live_slots(1000), 0u);
}

TEST_F(TopTronicTest, ResponsePublishesToTheMatchingSensor) {
  TopTronicSensor sensor;
  sensor.set_function_group(0x01);
  sensor.set_function_number(0x02);
  sensor.set_datapoint(0x9ECB);
  sensor.set_type(U16);
  this->hub_.add_sensor(&sensor);
  // Another datapoint is ignored
  this->hub_.interpret({0x42, 0x01, 0x02, 0x00, 0x10, 0x12, 0x34});
  EXPECT_FALSE(sensor.has_state());
  // A value shorter than the type is ignored
  this->hub_.interpret({0x42, 0x01, 0x02, 0x9E, 0xCB, 0x12});
  EXPECT_FALSE(sensor.has_state());
  this->hub_.interpret({0x42, 0x01, 0x02, 0x9E, 0xCB, 0x12, 0x34});
  EXPECT_FLOAT_EQ(sensor.state, 0x1234);
  // The extended response carries two more header bytes before the value
  this->hub_.interpret({0x56, 0x01, 0x02, 0x9E, 0xCB, 0x00, 0x00, 0x00, 0x2A});
  EXPECT_FLOAT_EQ(sensor.state, 0x2A);
}

TEST_F(TopTronicTest, UpdateSendsGetRequestFromTheGateway) {
  TopTronicSensor sensor;
  sensor.set_function_group(50);
  sensor.set_function_number(0);
  sensor.set_datapoint(40651);
  sensor.set_type(U8);
  this->hub_.add_sensor(&sensor);
  this->hub_.register_sensor_callbacks();
  sensor.update();
  const auto &frame = this->bus_.last_sent;
  EXPECT_TRUE(frame.use_extended_id);
  // Gateway type 1152 with the hub's address 8, sent to device 0x208
  EXPECT_EQ(frame.can_id, (0x7Fu << 22) | ((1152u | 8u) << 11) | 0x208u);
  ASSERT_EQ(frame.can_data_length_code, 6);
  EXPECT_EQ(std::vector<uint8_t>(frame.data, frame.data + 6),
            (std::vector<uint8_t>{0x01, 0x40, 50, 0, 40651 >> 8, 40651 & 0xFF}));
}

TEST_F(TopTronicTest, GatewaySenderIdKeepsTheHubAddress) {
  NullCanbus bus_a;
  NullCanbus bus_b;
  TestableTopTronic hub_a{&bus_a, 0x201};  // HV at address 1
  TestableTopTronic hub_b{&bus_b, 0x202};  // HV at address 2
  TopTronicSensor sensor_a;
  TopTronicSensor sensor_b;
  for (auto *sensor : {&sensor_a, &sensor_b}) {
    sensor->set_function_group(50);
    sensor->set_function_number(0);
    sensor->set_datapoint(40651);
    sensor->set_type(U8);
  }
  hub_a.add_sensor(&sensor_a);
  hub_a.register_sensor_callbacks();
  hub_b.add_sensor(&sensor_b);
  hub_b.register_sensor_callbacks();
  sensor_a.update();
  sensor_b.update();
  // Neighbouring addresses must not share one gateway sender id
  EXPECT_EQ((bus_a.last_sent.can_id >> 11) & 0x7FF, 1152u | 1u);
  EXPECT_EQ((bus_b.last_sent.can_id >> 11) & 0x7FF, 1152u | 2u);
}

#ifdef USE_NUMBER
TEST_F(TopTronicTest, NumberSendsScaledSetRequestInOneFrame) {
  TopTronicNumber number;
  number.set_function_group(50);
  number.set_function_number(0);
  number.set_datapoint(40651);
  number.set_type(S16);
  number.set_multiplier(10.0f);
  this->hub_.add_input(&number);
  this->hub_.register_input_callbacks();
  number.control(21.5f);
  ASSERT_EQ(this->bus_.sent.size(), 1u);
  const auto &frame = this->bus_.sent[0];
  EXPECT_TRUE(frame.use_extended_id);
  EXPECT_EQ(frame.can_id, (0x7Fu << 22) | ((1152u | 8u) << 11) | 0x208u);
  ASSERT_EQ(frame.can_data_length_code, 8);
  EXPECT_EQ(std::vector<uint8_t>(frame.data, frame.data + 8),
            (std::vector<uint8_t>{0x01, 0x46, 50, 0, 40651 >> 8, 40651 & 0xFF, 0x00, 0xD7}));
}

TEST_F(TopTronicTest, LongSetRequestIsSplitAndReassembles) {
  this->set_now_(1000);
  TopTronicNumber number;
  number.set_function_group(50);
  number.set_function_number(0);
  number.set_datapoint(40651);
  number.set_type(S32);
  this->hub_.add_input(&number);
  this->hub_.register_input_callbacks();
  // Out of range values are clamped to the wire type
  number.control(-1e12f);
  ASSERT_EQ(this->bus_.sent.size(), 2u);
  const auto &start = this->bus_.sent[0];
  const auto &cont = this->bus_.sent[1];
  EXPECT_EQ(start.can_id >> 24, 0x1Fu);
  EXPECT_EQ(cont.can_id, start.can_id & 0x003FFFFF);
  EXPECT_TRUE(
      this->hub_.feed(start.can_id, std::vector<uint8_t>(start.data, start.data + start.can_data_length_code)).empty());
  EXPECT_EQ(this->hub_.feed(cont.can_id, std::vector<uint8_t>(cont.data, cont.data + cont.can_data_length_code)),
            (std::vector<uint8_t>{0x46, 50, 0, 40651 >> 8, 40651 & 0xFF, 0x80, 0x00, 0x00, 0x00}));
}

TEST_F(TopTronicTest, RepeatedSetIsRateLimited) {
  this->set_now_(1000);
  TopTronicNumber number;
  number.set_function_group(50);
  number.set_function_number(0);
  number.set_datapoint(40651);
  number.set_type(U8);
  this->hub_.add_input(&number);
  this->hub_.register_input_callbacks();
  number.control(1.0f);
  EXPECT_EQ(this->bus_.sent.size(), 1u);
  this->set_now_(2999);
  number.control(2.0f);
  EXPECT_EQ(this->bus_.sent.size(), 1u);
  this->set_now_(3000);
  number.control(3.0f);
  EXPECT_EQ(this->bus_.sent.size(), 2u);
}

TEST_F(TopTronicTest, ZeroWriteIntervalDisablesTheRateLimit) {
  this->set_now_(1000);
  this->hub_.set_write_min_interval_ms(0);
  TopTronicNumber number;
  number.set_function_group(50);
  number.set_function_number(0);
  number.set_datapoint(40651);
  number.set_type(U8);
  this->hub_.add_input(&number);
  this->hub_.register_input_callbacks();
  number.control(1.0f);
  number.control(2.0f);
  EXPECT_EQ(this->bus_.sent.size(), 2u);
}

TEST_F(TopTronicTest, FailedSetDoesNotStartTheInterval) {
  this->set_now_(1000);
  TopTronicNumber number;
  number.set_function_group(50);
  number.set_function_number(0);
  number.set_datapoint(40651);
  number.set_type(U8);
  this->hub_.add_input(&number);
  this->hub_.register_input_callbacks();
  this->bus_.fail = true;
  number.control(1.0f);
  this->bus_.fail = false;
  number.control(1.0f);
  EXPECT_EQ(this->bus_.sent.size(), 1u);
}

#ifdef USE_SENSOR
TEST_F(TopTronicTest, SetIsRejectedUntilTheDatapointWasRead) {
  this->set_now_(1000);
  TopTronicSensor sensor;
  TopTronicNumber number;
  for (TopTronicBase *entity : {static_cast<TopTronicBase *>(&sensor), static_cast<TopTronicBase *>(&number)}) {
    entity->set_function_group(0x01);
    entity->set_function_number(0x02);
    entity->set_datapoint(0x0010);
  }
  sensor.set_type(U8);
  number.set_type(U8);
  this->hub_.add_sensor(&sensor);
  this->hub_.add_input(&number);
  this->hub_.register_input_callbacks();
  number.control(1.0f);
  EXPECT_TRUE(this->bus_.sent.empty());
  // A truncated response does not count as a read
  this->hub_.interpret({0x42, 0x01, 0x02, 0x00, 0x10});
  number.control(1.0f);
  EXPECT_TRUE(this->bus_.sent.empty());
  this->hub_.interpret({0x42, 0x01, 0x02, 0x00, 0x10, 0x05});
  number.control(1.0f);
  EXPECT_EQ(this->bus_.sent.size(), 1u);
}

TEST_F(TopTronicTest, ReadGuardCanBeDisabled) {
  this->set_now_(1000);
  this->hub_.set_reject_writes_before_read(false);
  TopTronicSensor sensor;
  TopTronicNumber number;
  for (TopTronicBase *entity : {static_cast<TopTronicBase *>(&sensor), static_cast<TopTronicBase *>(&number)}) {
    entity->set_function_group(0x01);
    entity->set_function_number(0x02);
    entity->set_datapoint(0x0010);
  }
  number.set_type(U8);
  this->hub_.add_sensor(&sensor);
  this->hub_.add_input(&number);
  this->hub_.register_input_callbacks();
  number.control(1.0f);
  EXPECT_EQ(this->bus_.sent.size(), 1u);
}

TEST_F(TopTronicTest, PausedHubDoesNotPoll) {
  TopTronicSensor sensor;
  sensor.set_function_group(50);
  sensor.set_function_number(0);
  sensor.set_datapoint(40651);
  sensor.set_type(U8);
  this->hub_.add_sensor(&sensor);
  this->hub_.register_sensor_callbacks();
  this->hub_.pause();
  EXPECT_TRUE(this->hub_.is_paused());
  sensor.update();
  EXPECT_TRUE(this->bus_.sent.empty());
  this->hub_.resume();
  sensor.update();
  EXPECT_EQ(this->bus_.sent.size(), 1u);
}
#endif
#endif

#ifdef USE_SENSOR
class TopTronicRefreshTest : public TopTronicTest {
 protected:
  void SetUp() override {
    this->set_now_(1000);
    uint16_t datapoint = 0x0010;
    for (auto &sensor : this->sensors_) {
      sensor.set_function_group(0x01);
      sensor.set_function_number(0x02);
      sensor.set_datapoint(datapoint++);
      sensor.set_type(U8);
      this->hub_.add_sensor(&sensor);
    }
    this->hub_.set_boot_refresh_delay_ms(0);
    this->hub_.set_refresh_interval_ms(50);
    this->hub_.setup();
  }

  void answer_(uint16_t datapoint) {
    this->hub_.interpret({0x42, 0x01, 0x02, static_cast<uint8_t>(datapoint >> 8), static_cast<uint8_t>(datapoint), 1});
  }

  // Answers the datapoint of the last GET sent
  void answer_last_() {
    const auto &frame = this->bus_.last_sent;
    this->answer_(encode_uint16(frame.data[4], frame.data[5]));
  }

  void drain_at_(uint32_t now) {
    this->set_now_(now);
    this->hub_.drain();
  }

  TopTronicSensor sensors_[2];
};

TEST_F(TopTronicRefreshTest, GetsAreSpacedAndAnsweredSensorsLeaveTheQueue) {
  this->hub_.update_all();
  EXPECT_TRUE(this->hub_.refreshing());
  this->drain_at_(1000);
  EXPECT_EQ(this->bus_.sent.size(), 1u);
  this->drain_at_(1040);
  EXPECT_EQ(this->bus_.sent.size(), 1u);
  this->drain_at_(1050);
  EXPECT_EQ(this->bus_.sent.size(), 2u);
  this->answer_(0x0010);
  this->answer_(0x0011);
  this->drain_at_(1100);
  EXPECT_FALSE(this->hub_.refreshing());
  EXPECT_EQ(this->bus_.sent.size(), 2u);
}

TEST_F(TopTronicRefreshTest, UnansweredGetIsRetriedThenDropped) {
  // The setter clamps below the 16 ms loop period, so 16 ms is the shortest spacing a test can use
  this->hub_.set_refresh_interval_ms(16);
  this->hub_.set_max_refresh_retries(1);
  this->hub_.set_refresh_retry_interval_ms(200);
  this->hub_.update_all();
  this->drain_at_(1000);
  this->drain_at_(1016);
  EXPECT_EQ(this->bus_.sent.size(), 2u);
  // The head of the queue waits for its retry interval
  this->drain_at_(1100);
  EXPECT_EQ(this->bus_.sent.size(), 2u);
  this->drain_at_(1200);
  this->drain_at_(1216);
  EXPECT_EQ(this->bus_.sent.size(), 4u);
  // Out of retries: both are dropped without another GET
  this->drain_at_(1400);
  this->drain_at_(1416);
  EXPECT_EQ(this->bus_.sent.size(), 4u);
  EXPECT_FALSE(this->hub_.refreshing());
}

TEST_F(TopTronicRefreshTest, RefreshRequestedDuringARefreshRunsAfterIt) {
  this->hub_.update_all();
  this->drain_at_(1000);
  this->hub_.update_all();
  this->answer_last_();
  this->drain_at_(1050);
  this->answer_last_();
  EXPECT_EQ(this->bus_.sent.size(), 2u);
  // The queue emptied, so the deferred refresh starts and sends again
  this->drain_at_(1100);
  EXPECT_TRUE(this->hub_.refreshing());
  this->drain_at_(1150);
  EXPECT_EQ(this->bus_.sent.size(), 3u);
}

TEST_F(TopTronicRefreshTest, PausedRefreshWaitsAndStalledRefreshIsAbandoned) {
  this->hub_.set_max_refresh_retries(10);
  this->hub_.update_all();
  this->hub_.pause();
  this->drain_at_(1000);
  this->drain_at_(20000);
  EXPECT_TRUE(this->bus_.sent.empty());
  EXPECT_TRUE(this->hub_.refreshing());
  this->hub_.resume();
  this->drain_at_(20000);
  EXPECT_EQ(this->bus_.sent.size(), 1u);
  // Nothing sent or answered for 5 s plus the retry interval abandons the refresh
  this->hub_.set_refresh_interval_ms(1000000);
  this->drain_at_(25199);
  EXPECT_TRUE(this->hub_.refreshing());
  this->drain_at_(25200);
  EXPECT_FALSE(this->hub_.refreshing());
  EXPECT_EQ(this->bus_.sent.size(), 1u);
}

TEST_F(TopTronicRefreshTest, RefreshAllStartsTheFirstHubAndDelaysTheRest) {
  NullCanbus bus2;
  TestableTopTronic hub2(&bus2);
  TopTronicSensor sensor2;
  sensor2.set_function_group(0x01);
  sensor2.set_function_number(0x02);
  sensor2.set_datapoint(0x0010);
  sensor2.set_type(U8);
  hub2.add_sensor(&sensor2);
  hub2.set_boot_refresh_delay_ms(0);
  hub2.setup();
  this->hub_.refresh_all();
  EXPECT_TRUE(this->hub_.refreshing());
  EXPECT_FALSE(hub2.refreshing());
  hub2.drain();
  EXPECT_TRUE(bus2.sent.empty());
}

TEST_F(TopTronicRefreshTest, BootRefreshSkipsHubsThatDisabledIt) {
  NullCanbus bus2;
  TestableTopTronic hub2(&bus2);
  TopTronicSensor sensor2;
  sensor2.set_function_group(0x01);
  sensor2.set_function_number(0x02);
  sensor2.set_datapoint(0x0010);
  sensor2.set_type(U8);
  hub2.add_sensor(&sensor2);
  // This hub opted out of the boot refresh; the other one kept the default delay
  hub2.set_boot_refresh_delay_ms(0);
  hub2.setup();
  this->hub_.set_boot_refresh_delay_ms(30000);
  this->hub_.refresh_all(true);
  EXPECT_TRUE(this->hub_.refreshing());
  EXPECT_FALSE(hub2.refreshing());
}
#endif

#ifdef USE_TEXT_SENSOR
TEST_F(TopTronicTest, ResponsePublishesTheMatchingTextOption) {
  TopTronicTextSensor sensor;
  sensor.set_function_group(0x01);
  sensor.set_function_number(0x02);
  sensor.set_datapoint(0x0010);
  sensor.add_option(0, "Off");
  sensor.add_option(1, "On");
  this->hub_.add_sensor(&sensor);
  this->hub_.interpret({0x42, 0x01, 0x02, 0x00, 0x10, 0x01});
  EXPECT_EQ(sensor.state, "On");
}
#endif

TEST_F(TopTronicTest, CandumpRateLimitsReceivedFramesButNotSentFrames) {
  this->set_now_(1000);
  TestableTopTronic::set_candump_enabled(true);
  TestableTopTronic::log_frame(START_ID, {0x00, 0x42}, false);
  EXPECT_EQ(TestableTopTronic::dropped(), 0u);
  TestableTopTronic::log_frame(START_ID, {0x00, 0x42}, false);
  this->set_now_(1020);
  TestableTopTronic::log_frame(START_ID, {0x00, 0x42}, false);
  EXPECT_EQ(TestableTopTronic::dropped(), 2u);
  // A sent frame is always logged and reports the skipped frames
  TestableTopTronic::log_frame(START_ID, {0x01, 0x40}, true);
  EXPECT_EQ(TestableTopTronic::dropped(), 0u);
  this->set_now_(1033);
  TestableTopTronic::log_frame(START_ID, {0x00, 0x42}, false);
  EXPECT_EQ(TestableTopTronic::dropped(), 0u);
}

TEST_F(TopTronicTest, DebugLogsTurnOffOnTheFramePathAfterTwoMinutes) {
  this->set_now_(1000);
  TestableTopTronic::set_candump_enabled(true);
  TestableTopTronic::set_find_can_id_enabled(true);
  this->set_now_(121000);
  TestableTopTronic::log_frame(START_ID, {0x00, 0x42}, false);
  EXPECT_TRUE(TestableTopTronic::is_candump_enabled());
  EXPECT_TRUE(TestableTopTronic::is_find_can_id_enabled());
  this->set_now_(121001);
  TestableTopTronic::log_frame(START_ID, {0x00, 0x42}, false);
  EXPECT_FALSE(TestableTopTronic::is_candump_enabled());
  EXPECT_FALSE(TestableTopTronic::is_find_can_id_enabled());
}

TEST_F(TopTronicTest, SentFramesAreLoggedWhileCandumpIsOn) {
  this->set_now_(1000);
  TestableTopTronic::set_candump_enabled(true);
  // Two sent frames in the same tick are both logged, so nothing counts as skipped
  TestableTopTronic::log_frame(START_ID, {0x01, 0x40}, true);
  TestableTopTronic::log_frame(START_ID, {0x01, 0x40}, true);
  EXPECT_EQ(TestableTopTronic::dropped(), 0u);
  this->set_now_(130000);
  TestableTopTronic::log_frame(START_ID, {0x01, 0x40}, true);
  EXPECT_FALSE(TestableTopTronic::is_candump_enabled());
}

TEST_F(TopTronicTest, OneDebugFrameLoggerPerCanBus) {
  // The fixture hub added the frame logger and its own receive callback
  EXPECT_EQ(this->bus_.callback_count(), 2u);
  TestableTopTronic same_bus_hub{&this->bus_};
  // A second hub on the same bus adds only its receive callback
  EXPECT_EQ(this->bus_.callback_count(), 3u);
  NullCanbus other_bus;
  TestableTopTronic other_bus_hub{&other_bus};
  // A hub on another bus gets its own logger, or frames there would never be logged
  EXPECT_EQ(other_bus.callback_count(), 2u);
}

TEST_F(TopTronicTest, FindCanIdOnlyMatchesSingleFrameMessages) {
  // Only a single frame message holds a command at [1]
  EXPECT_TRUE(TestableTopTronic::single_frame(START_ID, {0x00, 0x42}));
  EXPECT_TRUE(TestableTopTronic::single_frame(START_ID, {0x00, 0x40}));
  // A multi-frame start frame holds the message tag there, which can be 0x40 or 0x42 by chance
  EXPECT_FALSE(TestableTopTronic::single_frame(START_ID, {0x10, 0x42}));
  // A continuation frame holds an arbitrary payload byte
  EXPECT_FALSE(TestableTopTronic::single_frame(CONT_ID, {TAG, 0x42}));
  // A one byte frame cannot hold a command
  EXPECT_FALSE(TestableTopTronic::single_frame(START_ID, {0x00}));
}

#ifdef USE_SWITCH
TEST_F(TopTronicTest, SetupPublishesTheDebugSwitchState) {
  NullCanbus other_bus;
  TestableTopTronic other_hub{&other_bus};
  switch_::ApplySwitch<apply_candump> candump_a;
  switch_::ApplySwitch<apply_candump> candump_b;
  this->hub_.set_candump_switch(&candump_a);
  other_hub.set_candump_switch(&candump_b);
  EXPECT_FALSE(candump_a.has_state());
  EXPECT_FALSE(candump_b.has_state());

  this->hub_.setup();
  // Every switch of the build reports the boot state instead of staying unknown
  EXPECT_TRUE(candump_a.has_state());
  EXPECT_TRUE(candump_b.has_state());
  EXPECT_FALSE(candump_a.state);
  EXPECT_FALSE(candump_b.state);
}

TEST_F(TopTronicTest, DebugSwitchesOfEveryHubFollowTheirLog) {
  this->set_now_(1000);
  NullCanbus other_bus;
  TestableTopTronic other_hub{&other_bus};
  switch_::ApplySwitch<apply_candump> candump_a, candump_b;
  switch_::ApplySwitch<apply_find_can_id> find_a;
  for (switch_::Switch *sw : {static_cast<switch_::Switch *>(&candump_a), static_cast<switch_::Switch *>(&candump_b),
                              static_cast<switch_::Switch *>(&find_a)})
    sw->restore_mode = switch_::SWITCH_ALWAYS_OFF;
  this->hub_.set_candump_switch(&candump_a);
  this->hub_.set_find_can_id_switch(&find_a);
  other_hub.set_candump_switch(&candump_b);

  candump_a.turn_on();
  EXPECT_TRUE(TestableTopTronic::is_candump_enabled());
  EXPECT_FALSE(TestableTopTronic::is_find_can_id_enabled());
  EXPECT_TRUE(candump_a.state);
  EXPECT_TRUE(candump_b.state);
  EXPECT_FALSE(find_a.state);

  // The automatic turn off reaches every switch of that log
  this->set_now_(122000);
  TestableTopTronic::log_frame(START_ID, {0x00, 0x42}, false);
  EXPECT_FALSE(candump_a.state);
  EXPECT_FALSE(candump_b.state);

  TestableTopTronic::set_find_can_id_enabled(true);
  EXPECT_TRUE(find_a.state);
  EXPECT_FALSE(candump_a.state);
}
#endif

#ifdef USE_BUTTON
TEST_F(TopTronicTest, ButtonSendsItsSetRequestWithoutAReadback) {
  // A write-only datapoint is not held back by the read guard
  TopTronicButton button;
  button.set_function_group(0);
  button.set_function_number(0);
  button.set_datapoint(21054);
  button.set_type(U8);
  button.set_value(7);
  this->hub_.add_input(&button);
  this->hub_.register_input_callbacks();
  button.press();
  ASSERT_EQ(this->bus_.sent.size(), 1u);
  const auto &frame = this->bus_.sent[0];
  ASSERT_EQ(frame.can_data_length_code, 7);
  EXPECT_EQ(std::vector<uint8_t>(frame.data, frame.data + 7),
            (std::vector<uint8_t>{0x01, 0x46, 0, 0, 21054 >> 8, 21054 & 0xFF, 7}));
}

TEST_F(TopTronicTest, ButtonKeepsTheFullRangeOfAnUnsignedType) {
  TopTronicButton button;
  button.set_function_group(0);
  button.set_function_number(0);
  button.set_datapoint(0);
  button.set_type(U32);
  button.set_value(0xFFFFFFFE);
  this->hub_.add_input(&button);
  this->hub_.register_input_callbacks();
  button.press();
  ASSERT_EQ(this->bus_.sent.size(), 2u);
  const auto &start = this->bus_.sent[0];
  const auto &cont = this->bus_.sent[1];
  EXPECT_TRUE(
      this->hub_.feed(start.can_id, std::vector<uint8_t>(start.data, start.data + start.can_data_length_code)).empty());
  EXPECT_EQ(this->hub_.feed(cont.can_id, std::vector<uint8_t>(cont.data, cont.data + cont.can_data_length_code)),
            (std::vector<uint8_t>{0x46, 0, 0, 0, 0, 0xFF, 0xFF, 0xFF, 0xFE}));
}
#endif

#ifdef USE_NUMBER
TEST_F(TopTronicTest, WriteOnlyNumberPublishesTheWrittenValue) {
  TopTronicNumber number;
  number.set_function_group(50);
  number.set_function_number(0);
  number.set_datapoint(40651);
  number.set_type(U16);
  this->hub_.add_input(&number);
  this->hub_.setup();
  number.control(42.0f);
  EXPECT_TRUE(number.has_state());
  EXPECT_FLOAT_EQ(number.state, 42.0f);
}

TEST_F(TopTronicTest, LinkedNumberTakesItsStateFromTheSensor) {
  TopTronicSensor sensor;
  sensor.set_function_group(50);
  sensor.set_function_number(0);
  sensor.set_datapoint(40651);
  sensor.set_type(U16);
  TopTronicNumber number;
  number.set_function_group(50);
  number.set_function_number(0);
  number.set_datapoint(40651);
  number.set_type(U16);
  number.set_multiplier(10.0f);
  this->hub_.add_sensor(&sensor);
  this->hub_.add_input(&number);
  this->hub_.setup();
  // The read entity owns the state, so the write does not publish on its own
  number.control(21.5f);
  EXPECT_FALSE(number.has_state());
  this->hub_.interpret({0x42, 50, 0, 40651 >> 8, 40651 & 0xFF, 0x00, 0xD7});
  EXPECT_FLOAT_EQ(number.state, 21.5f);
}
#endif

#if defined(USE_NUMBER) && defined(USE_SELECT)
TEST_F(TopTronicTest, MismatchedLinkIsSkippedInsteadOfCast) {
  TopTronicSensor sensor;
  sensor.set_function_group(50);
  sensor.set_function_number(0);
  sensor.set_datapoint(40651);
  sensor.set_type(U16);
  TopTronicSelect select;
  select.set_function_group(50);
  select.set_function_number(0);
  select.set_datapoint(40651);
  select.add_option(0, "Off");
  select.add_option(1, "On");
  this->hub_.add_sensor(&sensor);
  this->hub_.add_input(&select);
  // A select paired with a numeric sensor is rejected in config validation; the hub
  // must not downcast it if an external component still sets up that pairing.
  this->hub_.setup();
  EXPECT_FALSE(select.has_state());
}
#endif

TEST_F(TopTronicTest, RemoteFrameDoesNotFeedReassembly) {
  this->set_now_(1000);
  auto frames = split(MESSAGE);
  this->bus_.push(START_ID | SENDER_BITS, false, frames[0]);
  this->bus_.loop();
  EXPECT_EQ(this->hub_.live_slots(1000), 1u);
  // A remote frame from the same sender must not be taken as the continuation
  this->bus_.push(CONT_ID | SENDER_BITS, true, frames[1]);
  this->bus_.loop();
  EXPECT_EQ(this->hub_.live_slots(1000), 1u);
  this->bus_.push(CONT_ID | SENDER_BITS, false, frames[1]);
  this->bus_.loop();
  EXPECT_EQ(this->hub_.live_slots(1000), 0u);
}

}  // namespace esphome::toptronic::testing
