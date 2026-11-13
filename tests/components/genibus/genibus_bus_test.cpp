#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <span>
#include <vector>

#include "esphome/components/genibus/genibus.h"
#include "esphome/core/application.h"

// The byte sequences are frames recorded from a real ALPHA3 or built to the same layout
namespace esphome::genibus::testing {

namespace {

using Bytes = std::vector<uint8_t>;

const Bytes GET_FLOW_HEAD = {0x0A, 0x03, 0x5D, 0x01, 0x21};
const Bytes GET_POWER = {0x0A, 0x03, 0x57, 0x00, 0x45};
const Bytes FRAME_FLOW_HEAD = {0x27, 0x07, 0xE7, 0xF8, 0x0A, 0x03, 0x5D, 0x01, 0x21, 0x52, 0x1F};
const Bytes FRAME_POWER = {0x27, 0x07, 0xE7, 0xF8, 0x0A, 0x03, 0x57, 0x00, 0x45, 0x8A, 0xCD};
// Flow and head object: header type 0x0130 version 1, 24 bytes as the pump sends it; the values are made up
const Bytes REPLY_FLOW_HEAD = {0x24, 0x23, 0xF8, 0xE7, 0x0A, 0x1F, 0x00, 0x01, 0x30, 0x01, 0x00, 0x00, 0x18,
                               0x3A, 0x83, 0x12, 0x6F, 0x46, 0x1C, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                               0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x17, 0x3B};
const Bytes REPLY_ACK = {0x24, 0x05, 0xF8, 0xE7, 0x0A, 0x01, 0x00, 0xAE, 0xA2};
// Class 2 GET of ID 0x51 and its answer "ID unknown", which carries the first unknown ID (spec 2005, figure 3)
const Bytes GET_UNKNOWN_ID = {0x02, 0x01, 0x51};
const Bytes REPLY_ID_UNKNOWN = {0x24, 0x05, 0xF8, 0xE7, 0x02, 0x81, 0x51, 0x56, 0x4F};
const Bytes REPLY_APDU_TOO_LONG = {0x24, 0x05, 0xF8, 0xE7, 0x0A, 0x03, 0x00, 0xC8, 0xC0};
const Bytes REPLY_EMPTY = {0x24, 0x02, 0xF8, 0xE7, 0x4A, 0xA2};
const Bytes REPLY_OTHER_NODE = {0x24, 0x05, 0xF8, 0x20, 0x0A, 0x01, 0x00, 0x4C, 0x2B};
// Recorded on a real ALPHA3: a GET of 93/289, 87/69, 86/8 and 86/10 in one request
const Bytes FRAME_FOUR_OBJECTS = {0x27, 0x16, 0xE7, 0xF8, 0x0A, 0x03, 0x5D, 0x01, 0x21, 0x0A, 0x03, 0x57, 0x00,
                                  0x45, 0x0A, 0x03, 0x56, 0x00, 0x08, 0x0A, 0x03, 0x56, 0x00, 0x0A, 0x53, 0x37};
constexpr uint32_t TIMEOUT_MS = 1000;
constexpr uint32_t QUIET_MS = 250;

class TestBus : public GenibusBus {
 public:
  bool is_connected() const override { return this->connected; }
  void lose_connection() {
    this->connected = false;
    this->connection_lost_();
  }
  void establish_connection() {
    this->connected = true;
    this->connection_established_();
  }

  bool connected{true};
  bool write_ok{true};
  size_t write_limit{SIZE_MAX};  // the transport fails after this many bytes of a frame
  std::vector<Bytes> written;

 protected:
  bool write_frame(std::span<const uint8_t> frame) override {
    const size_t size = std::min(frame.size(), this->write_limit);
    this->written.emplace_back(frame.begin(), frame.begin() + size);
    return this->write_ok && size == frame.size();
  }
};

struct Received {
  GenibusError error;
  Bytes apdus;
};

class TestDevice : public GenibusDevice {
 public:
  void update() override {}
  void on_response(const GenibusResponse &response) override {
    this->responses.push_back({response.error, Bytes(response.apdus.begin(), response.apdus.end())});
    if (this->send_next) {
      this->send_next = false;
      this->sent_from_response = this->parent_->send(this, GET_POWER);
    }
  }
  void on_connected() override {
    this->connects++;
    if (this->send_on_connect)
      this->sent_on_connect = this->request_flow_head();
  }
  void on_disconnected() override {
    this->disconnects++;
    this->sent_on_disconnect = this->request_flow_head();
  }
  bool request_flow_head() { return this->request_object_(93, 289); }

  std::vector<Received> responses;
  int connects{0};
  int disconnects{0};
  bool send_next{false};
  bool sent_from_response{false};
  bool send_on_connect{false};
  bool sent_on_connect{false};
  bool sent_on_disconnect{false};
};

class GenibusBusTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->bus_.set_master_address(0xF8);
    for (auto *device : {&this->pump_, &this->other_}) {
      device->set_parent(&this->bus_);
      this->bus_.register_device(device);
    }
    this->pump_.set_address(0xE7);
    this->other_.set_address(0x20);
  }

  void run_(uint32_t ms = 16) {
    this->now_ += ms;
    LoopBlockingGuard guard{nullptr, nullptr, this->now_};
    this->bus_.loop();
  }

  void receive_(const Bytes &frame, size_t chunk = 20) {
    for (size_t at = 0; at < frame.size(); at += chunk)
      this->bus_.receive_bytes(std::span<const uint8_t>(frame).subspan(at, std::min(chunk, frame.size() - at)));
  }

  TestBus bus_;
  TestDevice pump_;
  TestDevice other_;
  uint32_t now_{1000};
};

}  // namespace

TEST_F(GenibusBusTest, SendsRequestAndDeliversReply) {
  ASSERT_TRUE(this->pump_.request_flow_head());
  EXPECT_TRUE(this->bus_.written.empty());  // the wire is written from loop()
  this->run_();
  ASSERT_EQ(this->bus_.written.size(), 1u);
  EXPECT_EQ(this->bus_.written[0], FRAME_FLOW_HEAD);
  this->receive_(REPLY_FLOW_HEAD, 3);
  ASSERT_EQ(this->pump_.responses.size(), 1u);
  EXPECT_EQ(this->pump_.responses[0].error, GenibusError::GENIBUS_ERROR_NONE);
  const Bytes apdus(REPLY_FLOW_HEAD.begin() + 4, REPLY_FLOW_HEAD.end() - 2);
  EXPECT_EQ(this->pump_.responses[0].apdus, apdus);
  ApduReader reader(apdus);
  Apdu apdu{};
  ASSERT_TRUE(reader.next(apdu));
  DataObject object{};
  ASSERT_TRUE(parse_data_object(apdu, object));
  EXPECT_EQ(object.type, 0x0130);
  EXPECT_EQ(object.version, 1);
  EXPECT_EQ(object.body.size(), 24u);
  EXPECT_FALSE(this->bus_.status_has_warning());
}

TEST_F(GenibusBusTest, UsesConfiguredAddresses) {
  this->bus_.set_master_address(0x01);
  this->pump_.set_address(0x02);
  this->bus_.send(&this->pump_, GET_FLOW_HEAD);
  this->run_();
  ASSERT_EQ(this->bus_.written.size(), 1u);
  EXPECT_EQ(this->bus_.written[0][2], 0x02);
  EXPECT_EQ(this->bus_.written[0][3], 0x01);
  this->receive_(REPLY_ACK);  // addressed to 0xF8 from 0xE7, so not the answer
  EXPECT_TRUE(this->pump_.responses.empty());
}

TEST_F(GenibusBusTest, OneTransactionOnTheWire) {
  this->bus_.send(&this->pump_, GET_FLOW_HEAD);
  this->bus_.send(&this->other_, GET_POWER);
  this->run_();
  this->run_();
  ASSERT_EQ(this->bus_.written.size(), 1u);
  this->receive_(REPLY_FLOW_HEAD);
  this->run_();
  ASSERT_EQ(this->bus_.written.size(), 2u);
  EXPECT_EQ(this->bus_.written[1][2], 0x20);
  EXPECT_EQ(this->pump_.responses.size(), 1u);
  EXPECT_TRUE(this->other_.responses.empty());
}

TEST_F(GenibusBusTest, TimeoutEndsTransaction) {
  this->bus_.send(&this->pump_, GET_FLOW_HEAD);
  this->bus_.send(&this->other_, GET_POWER);
  this->run_();
  this->receive_({0x24, 0x23, 0xF8});  // a partial answer is dropped with the transaction
  this->run_(TIMEOUT_MS - 1);
  EXPECT_TRUE(this->pump_.responses.empty());
  this->run_(1);
  ASSERT_EQ(this->pump_.responses.size(), 1u);
  EXPECT_EQ(this->pump_.responses[0].error, GenibusError::GENIBUS_ERROR_TIMEOUT);
  EXPECT_TRUE(this->pump_.responses[0].apdus.empty());
  EXPECT_TRUE(this->bus_.status_has_warning());
  EXPECT_FALSE(this->bus_.is_busy(&this->pump_));
  this->run_(QUIET_MS);
  ASSERT_EQ(this->bus_.written.size(), 2u);
  this->receive_(REPLY_OTHER_NODE);
  ASSERT_EQ(this->other_.responses.size(), 1u);
  EXPECT_EQ(this->other_.responses[0].error, GenibusError::GENIBUS_ERROR_NONE);
  EXPECT_FALSE(this->bus_.status_has_warning());
}

TEST_F(GenibusBusTest, TimeoutAcrossMillisWraparound) {
  this->now_ = 0xFFFFFF00;
  this->bus_.send(&this->pump_, GET_FLOW_HEAD);
  this->run_();
  this->run_(TIMEOUT_MS - 1);  // the clock wrapped past 0
  EXPECT_TRUE(this->pump_.responses.empty());
  this->run_(1);
  ASSERT_EQ(this->pump_.responses.size(), 1u);
  EXPECT_EQ(this->pump_.responses[0].error, GenibusError::GENIBUS_ERROR_TIMEOUT);
}

TEST_F(GenibusBusTest, QuietPeriodAfterTimeout) {
  this->bus_.send(&this->pump_, GET_FLOW_HEAD);
  this->run_();
  this->run_(TIMEOUT_MS);
  ASSERT_EQ(this->pump_.responses.size(), 1u);
  ASSERT_TRUE(this->bus_.send(&this->pump_, GET_POWER));
  this->run_(QUIET_MS - 1);
  EXPECT_EQ(this->bus_.written.size(), 1u);  // nothing sent while quiet
  this->receive_(REPLY_FLOW_HEAD);           // the late answer to the first request
  EXPECT_EQ(this->pump_.responses.size(), 1u);
  this->run_(1);
  ASSERT_EQ(this->bus_.written.size(), 2u);
  EXPECT_EQ(this->bus_.written[1], FRAME_POWER);
  this->receive_(REPLY_ACK);
  ASSERT_EQ(this->pump_.responses.size(), 2u);
  EXPECT_EQ(this->pump_.responses[1].apdus, Bytes(REPLY_ACK.begin() + 4, REPLY_ACK.end() - 2));
}

TEST_F(GenibusBusTest, ReplyWithoutApduIsSuccess) {
  this->bus_.send(&this->pump_, GET_FLOW_HEAD);
  this->run_();
  this->receive_(REPLY_EMPTY);
  ASSERT_EQ(this->pump_.responses.size(), 1u);
  EXPECT_EQ(this->pump_.responses[0].error, GenibusError::GENIBUS_ERROR_NONE);
  EXPECT_TRUE(this->pump_.responses[0].apdus.empty());
}

TEST_F(GenibusBusTest, FalseStartRecoversAfterTimeout) {
  this->bus_.send(&this->pump_, GET_FLOW_HEAD);
  this->run_();
  // A stray start byte with a long length swallows the real answer; only the timeout ends the wait
  this->receive_({0x24, 0x30});
  this->receive_(REPLY_FLOW_HEAD);
  EXPECT_TRUE(this->pump_.responses.empty());
  this->run_(TIMEOUT_MS);
  ASSERT_EQ(this->pump_.responses.size(), 1u);
  EXPECT_EQ(this->pump_.responses[0].error, GenibusError::GENIBUS_ERROR_TIMEOUT);
  this->bus_.send(&this->pump_, GET_FLOW_HEAD);
  this->run_(QUIET_MS);
  this->receive_(REPLY_FLOW_HEAD);
  ASSERT_EQ(this->pump_.responses.size(), 2u);
  EXPECT_EQ(this->pump_.responses[1].error, GenibusError::GENIBUS_ERROR_NONE);
}

TEST_F(GenibusBusTest, CrcErrorEndsTransaction) {
  this->bus_.send(&this->pump_, GET_FLOW_HEAD);
  this->run_();
  Bytes bad = REPLY_FLOW_HEAD;
  bad[12] ^= 0x10;
  this->receive_(bad);
  ASSERT_EQ(this->pump_.responses.size(), 1u);
  EXPECT_EQ(this->pump_.responses[0].error, GenibusError::GENIBUS_ERROR_CRC);
  EXPECT_TRUE(this->bus_.status_has_warning());
  // The pump may still be sending; the next request waits out the quiet period
  ASSERT_TRUE(this->pump_.request_flow_head());
  this->run_(QUIET_MS - 1);
  EXPECT_EQ(this->bus_.written.size(), 1u);
  this->run_(1);
  EXPECT_EQ(this->bus_.written.size(), 2u);
}

TEST_F(GenibusBusTest, MalformedReplyEndsTransaction) {
  this->bus_.send(&this->pump_, GET_FLOW_HEAD);
  this->run_();
  this->receive_(REPLY_APDU_TOO_LONG);
  ASSERT_EQ(this->pump_.responses.size(), 1u);
  EXPECT_EQ(this->pump_.responses[0].error, GenibusError::GENIBUS_ERROR_MALFORMED);
  ASSERT_TRUE(this->pump_.request_flow_head());
  this->run_(QUIET_MS - 1);
  EXPECT_EQ(this->bus_.written.size(), 1u);
  this->run_(1);
  EXPECT_EQ(this->bus_.written.size(), 2u);
}

TEST_F(GenibusBusTest, AckErrorReachesDevice) {
  this->bus_.send(&this->pump_, GET_FLOW_HEAD);
  this->run_();
  this->run_(TIMEOUT_MS);
  ASSERT_TRUE(this->bus_.status_has_warning());
  this->bus_.send(&this->pump_, GET_UNKNOWN_ID);
  this->run_(QUIET_MS);
  ASSERT_EQ(this->bus_.written.size(), 2u);
  EXPECT_EQ(this->bus_.written[1], (Bytes{0x27, 0x05, 0xE7, 0xF8, 0x02, 0x01, 0x51, 0xE3, 0x3D}));
  this->receive_(REPLY_ID_UNKNOWN);
  ASSERT_EQ(this->pump_.responses.size(), 2u);
  EXPECT_EQ(this->pump_.responses[1].error, GenibusError::GENIBUS_ERROR_NONE);
  EXPECT_FALSE(this->bus_.status_has_warning());  // a negative ACK is a working bus; the device judges it
  ApduReader reader(this->pump_.responses[1].apdus);
  Apdu apdu{};
  ASSERT_TRUE(reader.next(apdu));
  EXPECT_EQ(apdu.ack, 2);
  EXPECT_EQ(apdu.data.size(), 1u);
  EXPECT_EQ(apdu.data[0], 0x51);
}

TEST_F(GenibusBusTest, IgnoresOtherTraffic) {
  this->receive_(REPLY_ACK);  // no request open
  this->bus_.send(&this->pump_, GET_FLOW_HEAD);
  this->run_();
  this->receive_(FRAME_FLOW_HEAD);   // an echo of the request
  this->receive_(REPLY_OTHER_NODE);  // another node's answer
  EXPECT_TRUE(this->pump_.responses.empty());
  this->receive_(REPLY_FLOW_HEAD);
  ASSERT_EQ(this->pump_.responses.size(), 1u);
  EXPECT_EQ(this->pump_.responses[0].error, GenibusError::GENIBUS_ERROR_NONE);
}

TEST_F(GenibusBusTest, RejectedWhileWaiting) {
  EXPECT_FALSE(this->bus_.is_busy(&this->pump_));
  ASSERT_TRUE(this->bus_.send(&this->pump_, GET_FLOW_HEAD));
  EXPECT_TRUE(this->bus_.is_busy(&this->pump_));
  EXPECT_FALSE(this->bus_.is_busy(&this->other_));
  EXPECT_FALSE(this->bus_.send(&this->pump_, GET_POWER));
  this->run_();
  ASSERT_EQ(this->bus_.written.size(), 1u);
  EXPECT_EQ(this->bus_.written[0], FRAME_FLOW_HEAD);
}

TEST_F(GenibusBusTest, RejectedWhileOnTheWire) {
  this->bus_.send(&this->pump_, GET_FLOW_HEAD);
  this->run_();
  EXPECT_TRUE(this->bus_.is_busy(&this->pump_));
  EXPECT_FALSE(this->bus_.send(&this->pump_, GET_POWER));
  this->receive_(REPLY_FLOW_HEAD);
  ASSERT_EQ(this->pump_.responses.size(), 1u);
  EXPECT_FALSE(this->bus_.is_busy(&this->pump_));
  this->run_();
  EXPECT_EQ(this->bus_.written.size(), 1u);  // the rejected request never goes out
}

TEST_F(GenibusBusTest, DevicesTakeTurns) {
  this->pump_.send_next = true;
  this->bus_.send(&this->pump_, GET_FLOW_HEAD);
  this->bus_.send(&this->other_, GET_POWER);
  this->run_();
  this->receive_(REPLY_FLOW_HEAD);  // the pump sends again from on_response()
  this->run_();
  ASSERT_EQ(this->bus_.written.size(), 2u);
  EXPECT_EQ(this->bus_.written[1][2], 0x20);  // the other device was waiting longer
  this->receive_(REPLY_OTHER_NODE);
  this->run_();
  ASSERT_EQ(this->bus_.written.size(), 3u);
  EXPECT_EQ(this->bus_.written[2], FRAME_POWER);
}

TEST_F(GenibusBusTest, DeviceCanSendFromResponse) {
  this->pump_.send_next = true;
  this->bus_.send(&this->pump_, GET_FLOW_HEAD);
  this->run_();
  this->receive_(REPLY_FLOW_HEAD);
  EXPECT_TRUE(this->pump_.sent_from_response);
  this->run_();
  ASSERT_EQ(this->bus_.written.size(), 2u);
  EXPECT_EQ(this->bus_.written[1], FRAME_POWER);
}

TEST_F(GenibusBusTest, DisconnectEndsEveryRequest) {
  this->bus_.send(&this->pump_, GET_FLOW_HEAD);
  this->run_();
  this->bus_.send(&this->other_, GET_POWER);
  this->receive_({0x24, 0x23, 0xF8, 0xE7});
  this->bus_.lose_connection();
  ASSERT_EQ(this->pump_.responses.size(), 1u);
  EXPECT_EQ(this->pump_.responses[0].error, GenibusError::GENIBUS_ERROR_DISCONNECTED);
  ASSERT_EQ(this->other_.responses.size(), 1u);
  EXPECT_EQ(this->other_.responses[0].error, GenibusError::GENIBUS_ERROR_DISCONNECTED);
  EXPECT_FALSE(this->bus_.status_has_warning());
  EXPECT_FALSE(this->bus_.send(&this->pump_, GET_FLOW_HEAD));
  this->run_();
  EXPECT_EQ(this->bus_.written.size(), 1u);
  this->bus_.connected = true;
  this->bus_.send(&this->pump_, GET_FLOW_HEAD);
  this->run_();
  this->receive_(REPLY_FLOW_HEAD);  // the partial frame from before the drop is gone
  ASSERT_EQ(this->pump_.responses.size(), 2u);
  EXPECT_EQ(this->pump_.responses[1].error, GenibusError::GENIBUS_ERROR_NONE);
}

TEST_F(GenibusBusTest, TransportDownWithoutNotice) {
  this->bus_.send(&this->pump_, GET_FLOW_HEAD);
  this->bus_.connected = false;
  this->run_();
  EXPECT_TRUE(this->bus_.written.empty());
  ASSERT_EQ(this->pump_.responses.size(), 1u);
  EXPECT_EQ(this->pump_.responses[0].error, GenibusError::GENIBUS_ERROR_DISCONNECTED);
}

TEST_F(GenibusBusTest, TransportDownWithoutNoticeOnTheWire) {
  this->bus_.send(&this->pump_, GET_FLOW_HEAD);
  this->run_();
  this->bus_.send(&this->other_, GET_POWER);
  this->bus_.connected = false;
  this->run_(TIMEOUT_MS);
  ASSERT_EQ(this->pump_.responses.size(), 1u);
  EXPECT_EQ(this->pump_.responses[0].error, GenibusError::GENIBUS_ERROR_TIMEOUT);
  EXPECT_TRUE(this->other_.responses.empty());  // still waiting out the quiet period
  this->run_(QUIET_MS);
  ASSERT_EQ(this->other_.responses.size(), 1u);
  EXPECT_EQ(this->other_.responses[0].error, GenibusError::GENIBUS_ERROR_DISCONNECTED);
  EXPECT_EQ(this->bus_.written.size(), 1u);
  this->bus_.connected = true;
  this->bus_.send(&this->pump_, GET_FLOW_HEAD);
  this->run_();
  this->receive_(REPLY_FLOW_HEAD);
  ASSERT_EQ(this->pump_.responses.size(), 2u);
  EXPECT_EQ(this->pump_.responses[1].error, GenibusError::GENIBUS_ERROR_NONE);
}

TEST_F(GenibusBusTest, EveryDeviceHearsOfConnectionChanges) {
  this->bus_.send(&this->pump_, GET_FLOW_HEAD);
  this->bus_.lose_connection();
  // The request ends before the device hears of the loss, and it cannot send from there
  ASSERT_EQ(this->pump_.responses.size(), 1u);
  EXPECT_EQ(this->pump_.disconnects, 1);
  EXPECT_EQ(this->other_.disconnects, 1);
  EXPECT_FALSE(this->pump_.sent_on_disconnect);
  // A second notice of the same loss, here from the transport and the bus's own check, is not repeated
  this->bus_.lose_connection();
  this->run_();
  EXPECT_EQ(this->pump_.disconnects, 1);
  this->bus_.establish_connection();
  EXPECT_EQ(this->pump_.connects, 1);
  EXPECT_EQ(this->other_.connects, 1);
  this->bus_.lose_connection();
  EXPECT_EQ(this->pump_.disconnects, 2);
  EXPECT_EQ(this->other_.disconnects, 2);
}

TEST_F(GenibusBusTest, DeviceCanSendFromOnConnected) {
  this->bus_.connected = false;
  this->pump_.send_on_connect = true;
  this->bus_.establish_connection();
  EXPECT_TRUE(this->pump_.sent_on_connect);
  this->run_();
  ASSERT_EQ(this->bus_.written.size(), 1u);
  EXPECT_EQ(this->bus_.written[0], FRAME_FLOW_HEAD);
}

TEST_F(GenibusBusTest, WriteFailureEndsTransaction) {
  this->bus_.write_ok = false;
  this->bus_.send(&this->pump_, GET_FLOW_HEAD);
  this->run_();
  ASSERT_EQ(this->pump_.responses.size(), 1u);
  EXPECT_EQ(this->pump_.responses[0].error, GenibusError::GENIBUS_ERROR_WRITE_FAILED);
  EXPECT_TRUE(this->bus_.status_has_warning());
}

TEST_F(GenibusBusTest, WriteFailureInSecondPieceWaitsQuietPeriod) {
  // A transport that writes 20 bytes at a time fails on the second piece: the pump got half a frame
  this->bus_.write_limit = 20;
  const Bytes four_gets(FRAME_FOUR_OBJECTS.begin() + 4, FRAME_FOUR_OBJECTS.end() - 2);
  ASSERT_TRUE(this->bus_.send(&this->pump_, four_gets));
  this->run_();
  ASSERT_EQ(this->bus_.written.size(), 1u);
  EXPECT_EQ(this->bus_.written[0], Bytes(FRAME_FOUR_OBJECTS.begin(), FRAME_FOUR_OBJECTS.begin() + 20));
  ASSERT_EQ(this->pump_.responses.size(), 1u);
  EXPECT_EQ(this->pump_.responses[0].error, GenibusError::GENIBUS_ERROR_WRITE_FAILED);
  EXPECT_TRUE(this->bus_.status_has_warning());
  this->bus_.write_limit = SIZE_MAX;
  ASSERT_TRUE(this->bus_.send(&this->pump_, GET_POWER));
  this->run_(QUIET_MS - 1);
  EXPECT_EQ(this->bus_.written.size(), 1u);  // nothing sent while quiet
  this->receive_(REPLY_FLOW_HEAD);           // whatever the pump answers to the half frame is dropped
  EXPECT_EQ(this->pump_.responses.size(), 1u);
  this->run_(1);
  ASSERT_EQ(this->bus_.written.size(), 2u);
  EXPECT_EQ(this->bus_.written[1], FRAME_POWER);
  this->receive_(REPLY_ACK);
  ASSERT_EQ(this->pump_.responses.size(), 2u);
  EXPECT_EQ(this->pump_.responses[1].error, GenibusError::GENIBUS_ERROR_NONE);
  EXPECT_FALSE(this->bus_.status_has_warning());
}

TEST_F(GenibusBusTest, LoopRunsOnlyWhileThereIsWork) {
  this->run_();
  EXPECT_TRUE(this->bus_.is_idle());  // nothing to send
  ASSERT_TRUE(this->pump_.request_flow_head());
  EXPECT_FALSE(this->bus_.is_idle());
  this->run_();
  EXPECT_FALSE(this->bus_.is_idle());  // waits for the reply or the timeout
  this->receive_(REPLY_FLOW_HEAD);
  EXPECT_FALSE(this->bus_.is_idle());
  this->run_();
  EXPECT_TRUE(this->bus_.is_idle());
  // After a timeout the loop runs through the quiet period, then stops
  ASSERT_TRUE(this->pump_.request_flow_head());
  this->run_();
  this->run_(TIMEOUT_MS);
  EXPECT_FALSE(this->bus_.is_idle());
  this->run_(QUIET_MS);
  EXPECT_TRUE(this->bus_.is_idle());
  // A lost connection ends the requests and leaves the loop off
  ASSERT_TRUE(this->pump_.request_flow_head());
  this->bus_.lose_connection();
  this->run_();
  EXPECT_TRUE(this->bus_.is_idle());
  EXPECT_FALSE(this->pump_.request_flow_head());
  EXPECT_TRUE(this->bus_.is_idle());
}

TEST_F(GenibusBusTest, RefusesBadRequests) {
  EXPECT_FALSE(this->bus_.send(&this->pump_, {}));
  const Bytes too_long(MAX_REQUEST_SIZE + 1, 0);
  EXPECT_FALSE(this->bus_.send(&this->pump_, too_long));
  TestDevice stranger;
  stranger.set_parent(&this->bus_);
  EXPECT_FALSE(this->bus_.send(&stranger, GET_FLOW_HEAD));
  EXPECT_FALSE(this->bus_.is_busy(&stranger));
  this->run_();
  EXPECT_TRUE(this->bus_.written.empty());
}

}  // namespace esphome::genibus::testing
