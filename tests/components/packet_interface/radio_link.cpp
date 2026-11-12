#include <gtest/gtest.h>
#include <cstdint>
#include <vector>
#include "esphome/components/packet_interface/radio_link.h"
#include "esphome/core/application.h"

namespace esphome::packet_interface::radio_link_testing {

using Packet = std::vector<uint8_t>;

static constexpr uint32_t START = 10000;
static constexpr uint32_t MAX_BUSY = ListenBeforeTalk::DEFAULT_MAX_BUSY_MS;

class FakeListener {
 public:
  virtual void on_packet(const std::vector<uint8_t> &packet, float rssi, float snr) = 0;
};

enum class FakeError { NONE, BUSY };

class FakeRadio : public Component {
 public:
  bool is_receiving() { return this->receiving; }
  void register_listener(FakeListener *listener) { this->listener = listener; }
  void fail() { this->mark_failed(); }
  FakeError transmit_packet(const uint8_t *data, size_t length) {
    this->sent.emplace_back(data, data + length);
    return this->result;
  }

  FakeListener *listener{nullptr};
  std::vector<Packet> sent;
  bool receiving{false};
  FakeError result{FakeError::NONE};
};

// Reads the time that set_time() sets
class TestLink : public RadioLink<FakeRadio, FakeListener, FakeError, 16> {
 public:
  using RadioLink::RadioLink;
  uint8_t not_understood_count() const { return this->not_understood_count_; }

 protected:
  uint32_t current_time_ms() const override { return App.get_loop_component_start_time(); }
};

static void set_time(uint32_t now) { LoopBlockingGuard guard{nullptr, nullptr, now}; }

class RadioLinkTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->radio_.call();
    this->link_.call();
  }
  bool send_() {
    static const Packet PACKET{1, 2, 3};
    return this->link_.send_to_interface(PacketBuffer(PACKET), {});
  }

  FakeRadio radio_;
  TestLink link_{&radio_, 16};
};

TEST_F(RadioLinkTest, LoopRunsOnlyWhileBusy) {
  EXPECT_EQ(this->radio_.listener, &this->link_);
  EXPECT_TRUE(this->link_.is_idle());
  set_time(START);
  this->radio_.receiving = true;
  EXPECT_FALSE(this->send_());
  EXPECT_TRUE(this->link_.is_in_loop_state());
  this->link_.loop();
  EXPECT_TRUE(this->link_.is_in_loop_state());
  this->radio_.receiving = false;
  this->link_.loop();
  EXPECT_TRUE(this->link_.is_idle());
  EXPECT_TRUE(this->radio_.sent.empty());
}

TEST_F(RadioLinkTest, ReceptionAfterStalePeriodIsNotSentOver) {
  set_time(START);
  this->radio_.receiving = true;
  EXPECT_FALSE(this->send_());
  // The reception ends with no send to see it
  this->radio_.receiving = false;
  this->link_.loop();
  // A new reception, longer after the first than the limit, still holds the send back
  set_time(START + MAX_BUSY + 100);
  this->radio_.receiving = true;
  EXPECT_FALSE(this->send_());
  EXPECT_TRUE(this->radio_.sent.empty());
}

TEST_F(RadioLinkTest, BusyFlagThatNeverClearsIsSentOver) {
  set_time(START);
  this->radio_.receiving = true;
  EXPECT_FALSE(this->send_());
  this->link_.loop();
  set_time(START + MAX_BUSY);
  EXPECT_TRUE(this->send_());
  ASSERT_EQ(this->radio_.sent.size(), 1u);
  EXPECT_EQ(this->radio_.sent[0], (Packet{1, 2, 3}));
}

TEST_F(RadioLinkTest, EmptyPacketCountsAsSentWithoutTransmitting) {
  static const Packet EMPTY;
  EXPECT_TRUE(this->link_.send_to_interface(PacketBuffer(EMPTY), {}));
  EXPECT_TRUE(this->radio_.sent.empty());
}

TEST_F(RadioLinkTest, FailedRadioRefusesSends) {
  this->radio_.fail();
  EXPECT_FALSE(this->send_());
  EXPECT_FALSE(this->send_());
  EXPECT_TRUE(this->radio_.sent.empty());
}

TEST_F(RadioLinkTest, RadioFailingWhileBusyEndsBusyPeriod) {
  set_time(START);
  this->radio_.receiving = true;
  EXPECT_FALSE(this->send_());
  EXPECT_TRUE(this->link_.is_in_loop_state());
  this->radio_.fail();
  this->link_.loop();
  EXPECT_TRUE(this->link_.is_idle());
}

TEST_F(RadioLinkTest, ReceivedPacketCarriesSignal) {
  Packet received;
  PacketMetaData meta;
  this->link_.add_packet_interface_listener([&](const PacketBuffer &data, const PacketMetaData &meta_data) {
    received.resize(data.size());
    data.copy_to(received.data(), received.size());
    meta = meta_data;
  });
  this->link_.on_packet({4, 5}, -80.0f, 7.5f);
  EXPECT_EQ(received, (Packet{4, 5}));
  EXPECT_FLOAT_EQ(meta.rssi, -80.0f);
  EXPECT_FLOAT_EQ(meta.snr, 7.5f);
}

// A packet in two parts, as a layer adds its header, is joined before it goes to the radio
TEST_F(RadioLinkTest, SplitPacketIsJoined) {
  static const Packet HEADER{9, 8};
  static const Packet BODY{1, 2, 3};
  PacketBuffer body(BODY);
  EXPECT_TRUE(this->link_.send_to_interface(PacketBuffer(HEADER.data(), HEADER.size(), &body, nullptr, 0), {}));
  ASSERT_EQ(this->radio_.sent.size(), 1u);
  EXPECT_EQ(this->radio_.sent[0], (Packet{9, 8, 1, 2, 3}));
}

TEST_F(RadioLinkTest, OversizedPacketRefused) {
  static const Packet LARGEST(16, 0x55);
  static const Packet OVERSIZED(17, 0x55);
  EXPECT_FALSE(this->link_.send_to_interface(PacketBuffer(OVERSIZED), {}));
  EXPECT_TRUE(this->radio_.sent.empty());
  EXPECT_TRUE(this->link_.send_to_interface(PacketBuffer(LARGEST), {}));
  EXPECT_EQ(this->radio_.sent.size(), 1u);
}

TEST_F(RadioLinkTest, TransmitErrorIsReported) {
  this->radio_.result = FakeError::BUSY;
  EXPECT_FALSE(this->send_());
  this->radio_.result = FakeError::NONE;
  EXPECT_TRUE(this->send_());
}

// The radio receives packets up to its own limit, which can be more than the link's
TEST_F(RadioLinkTest, OversizedReceptionDropped) {
  std::vector<size_t> sizes;
  this->link_.add_packet_interface_listener(
      [&](const PacketBuffer &data, const PacketMetaData &) { sizes.push_back(data.size()); });
  this->link_.on_packet(Packet(17, 0x55), -80.0f, 7.5f);
  this->link_.on_packet(Packet(16, 0x55), -80.0f, 7.5f);
  EXPECT_EQ(sizes, std::vector<size_t>{16});
}

// A send made over a busy flag that seemed stuck lets the radio listen again, so a reception that starts before
// loop() runs is waited for in a new busy period
TEST_F(RadioLinkTest, ForcedSendEndsBusyPeriod) {
  set_time(START);
  this->radio_.receiving = true;
  EXPECT_FALSE(this->send_());
  set_time(START + MAX_BUSY);
  EXPECT_TRUE(this->send_());
  set_time(START + MAX_BUSY + 100);
  EXPECT_FALSE(this->send_());
  EXPECT_EQ(this->radio_.sent.size(), 1u);
}

TEST_F(RadioLinkTest, AcceptedPacketEndsRunOfOversizedOnes) {
  this->link_.on_packet(Packet(17, 0x55), -80.0f, 7.5f);
  EXPECT_EQ(this->link_.not_understood_count(), 1);
  this->link_.on_packet(Packet(16, 0x55), -80.0f, 7.5f);
  EXPECT_EQ(this->link_.not_understood_count(), 0);
}

}  // namespace esphome::packet_interface::radio_link_testing
