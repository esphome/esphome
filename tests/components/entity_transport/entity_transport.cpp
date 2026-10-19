#include <gtest/gtest.h>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>
#include "esphome/components/entity_transport/entity_transport.h"
#include "esphome/core/application.h"

namespace esphome::entity_transport::testing {

// Copies a packet into a vector, for comparing in tests
static std::vector<uint8_t> to_vector(const esphome::packet_interface::PacketBuffer &data) {
  std::vector<uint8_t> result(data.size());
  data.copy_to(result.data(), result.size());
  return result;
}

using packet_interface::PacketBuffer;
using packet_interface::PacketMetaData;
using Packet = std::vector<uint8_t>;

static constexpr const char *DEVICE_NAME = "this-device";
static constexpr uint8_t PACKET_DICTIONARY = 0x11;
static constexpr uint8_t PACKET_DATA = 0x12;
static constexpr uint8_t PACKET_REQUEST = 0x13;

// A packet interface that records what is sent, can refuse to send, and can act as a session layer
class MockInterface : public packet_interface::PacketInterface {
 public:
  explicit MockInterface(size_t max_packet_size = 64) : packet_interface::PacketInterface(max_packet_size) {}
  bool send_to_interface(const PacketBuffer &data, const PacketMetaData &) override {
    if (!this->accept || this->budget == 0)
      return false;
    this->budget--;
    this->sent.push_back(to_vector(data));
    return true;
  }
  void inject(const Packet &packet) { this->on_receive_from_interface_(PacketBuffer(packet), {}); }
  void set_capabilities(uint8_t capabilities) { this->capabilities_ = capabilities; }
  void report_session(bool connected) { this->session_callback_.call(connected); }
  // Deliver everything sent so far to another interface
  void deliver_to(MockInterface &other) {
    auto packets = std::move(this->sent);
    this->sent.clear();
    for (auto &packet : packets)
      other.inject(packet);
  }

  std::vector<Packet> sent;
  bool accept{true};
  size_t budget{SIZE_MAX};  // packets accepted before refusing, like a full queue
};

class TestTransport : public EntityTransport {
 public:
  using EntityTransport::EntityTransport;
  uint32_t get_dictionary_id() const { return this->dictionary_id_; }
  uint32_t get_update_interval() const { return this->update_interval_ms_; }
  using EntityTransport::clear_remote_states_;
};

static Packet header(uint8_t packet_type, uint32_t id) {
  return {packet_type, static_cast<uint8_t>(id), static_cast<uint8_t>(id >> 8), static_cast<uint8_t>(id >> 16),
          static_cast<uint8_t>(id >> 24)};
}

class EntityTransportTest : public ::testing::Test {
 protected:
  void SetUp() override {
    App.~Application();
    new (&App) Application();
    static char name[] = "this-device";
    static char friendly_name[] = "";
    App.pre_setup(name, sizeof(name) - 1, friendly_name, 0);
  }
  void TearDown() override {
    App.~Application();
    new (&App) Application();  // later tests and the test listener use App
  }

  // The sender, with a sensor and a binary sensor
  std::unique_ptr<TestTransport> make_sender_(MockInterface *link = nullptr) {
    auto transport = std::make_unique<TestTransport>(link != nullptr ? link : &this->link_a_);
    transport->set_entity_count(2);
    transport->add_sensor("temp", &this->temp_);
    transport->add_binary_sensor("door", &this->door_);
    transport->setup();
    return transport;
  }
  // The receiver, showing the sender's entities; both run on this device, so they share its name
  std::unique_ptr<TestTransport> make_receiver_(const char *provider = DEVICE_NAME, uint32_t timeout_ms = 0) {
    auto transport = std::make_unique<TestTransport>(&this->link_b_);
    transport->set_remote_timeout(timeout_ms);
    transport->add_remote_sensor(provider, "temp", &this->remote_temp_);
    transport->add_remote_binary_sensor(provider, "door", &this->remote_door_);
    transport->setup();
    return transport;
  }

  MockInterface link_a_, link_b_;
  sensor::Sensor temp_, remote_temp_;
  binary_sensor::BinarySensor door_, remote_door_;
};

TEST_F(EntityTransportTest, SendsDictionaryThenStates) {
  this->temp_.publish_state(21.5f);
  this->door_.publish_state(true);
  auto sender = this->make_sender_();
  sender->loop();
  uint32_t id = sender->get_dictionary_id();
  Packet dictionary = header(PACKET_DICTIONARY, id);
  dictionary.push_back(strlen(DEVICE_NAME));
  dictionary.insert(dictionary.end(), DEVICE_NAME, DEVICE_NAME + strlen(DEVICE_NAME));
  dictionary.insert(dictionary.end(), {1, 0, 1, 4, 't', 'e', 'm', 'p', 2, 0, 1, 4, 'd', 'o', 'o', 'r'});
  Packet data = header(PACKET_DATA, id);
  data.insert(data.end(), {1, 0, 0x00, 0x00, 0xAC, 0x41, 0, 1});
  ASSERT_EQ(this->link_a_.sent.size(), 2);
  EXPECT_EQ(this->link_a_.sent[0], dictionary);
  EXPECT_EQ(this->link_a_.sent[1], data);
}

TEST_F(EntityTransportTest, SendsOnlyChangedStates) {
  auto sender = this->make_sender_();
  sender->loop();
  this->link_a_.sent.clear();
  this->temp_.publish_state(3.0f);
  sender->loop();
  Packet data = header(PACKET_DATA, sender->get_dictionary_id());
  data.insert(data.end(), {1, 0, 0x00, 0x00, 0x40, 0x40});
  ASSERT_EQ(this->link_a_.sent.size(), 1);
  EXPECT_EQ(this->link_a_.sent[0], data);
}

TEST_F(EntityTransportTest, StatesReachRemoteEntities) {
  this->temp_.publish_state(12.25f);
  this->door_.publish_state(true);
  auto sender = this->make_sender_();
  auto receiver = this->make_receiver_();
  sender->loop();
  this->link_a_.deliver_to(this->link_b_);
  EXPECT_EQ(this->remote_temp_.get_state(), 12.25f);
  EXPECT_TRUE(this->remote_door_.state);
  this->door_.invalidate_state();
  sender->loop();
  this->link_a_.deliver_to(this->link_b_);
  EXPECT_FALSE(this->remote_door_.has_state());
}

TEST_F(EntityTransportTest, SplitsAcrossPackets) {
  MockInterface link(5 + 1 + strlen(DEVICE_NAME) + 3 + 5);
  this->temp_.publish_state(1.0f);
  this->door_.publish_state(true);
  auto sender = this->make_sender_(&link);
  auto receiver = this->make_receiver_();
  sender->loop();
  EXPECT_EQ(link.sent.size(), 3);  // one dictionary packet for each entity, then one data packet
  link.deliver_to(this->link_b_);
  EXPECT_EQ(this->remote_temp_.get_state(), 1.0f);
  EXPECT_TRUE(this->remote_door_.state);
}

TEST_F(EntityTransportTest, RefusedPacketSentAgain) {
  this->link_a_.accept = false;
  auto sender = this->make_sender_();
  sender->loop();
  EXPECT_TRUE(this->link_a_.sent.empty());
  this->link_a_.accept = true;
  sender->loop();
  EXPECT_EQ(this->link_a_.sent.size(), 2);
}

TEST_F(EntityTransportTest, UnknownDictionaryRequested) {
  this->temp_.publish_state(5.0f);
  auto sender = this->make_sender_();
  auto receiver = this->make_receiver_();
  sender->loop();
  this->link_a_.sent.erase(this->link_a_.sent.begin());  // the dictionary is lost
  this->link_a_.deliver_to(this->link_b_);
  EXPECT_FALSE(this->remote_temp_.has_state());
  ASSERT_EQ(this->link_b_.sent.size(), 1);
  EXPECT_EQ(this->link_b_.sent[0], header(PACKET_REQUEST, sender->get_dictionary_id()));
  this->link_b_.deliver_to(this->link_a_);
  sender->loop();
  this->link_a_.deliver_to(this->link_b_);
  EXPECT_EQ(this->remote_temp_.get_state(), 5.0f);
}

TEST_F(EntityTransportTest, ChangedDictionaryRequested) {
  this->temp_.publish_state(5.0f);
  auto sender = this->make_sender_();
  auto receiver = this->make_receiver_();
  sender->loop();
  this->link_a_.deliver_to(this->link_b_);
  EXPECT_EQ(this->remote_temp_.get_state(), 5.0f);
  // The provider restarts with other entities, and its new dictionary is lost
  sensor::Sensor temp;
  MockInterface link;
  TestTransport changed(&link);
  changed.set_entity_count(1);
  changed.add_sensor("temp", &temp);
  changed.setup();
  temp.publish_state(6.0f);
  changed.loop();
  link.sent.erase(link.sent.begin());
  link.deliver_to(this->link_b_);
  ASSERT_EQ(this->link_b_.sent.size(), 1);
  EXPECT_EQ(this->link_b_.sent[0], header(PACKET_REQUEST, changed.get_dictionary_id()));
  this->link_b_.deliver_to(link);
  changed.loop();
  link.deliver_to(this->link_b_);
  EXPECT_EQ(this->remote_temp_.get_state(), 6.0f);
}

TEST_F(EntityTransportTest, RequestsLimited) {
  auto receiver = this->make_receiver_();
  Packet data = header(PACKET_DATA, 0x12345678);
  data.push_back(0);
  this->link_b_.inject(data);
  this->link_b_.inject(data);
  EXPECT_EQ(this->link_b_.sent.size(), 1);
}

TEST_F(EntityTransportTest, OtherProviderAndOwnPacketsIgnored) {
  this->temp_.publish_state(5.0f);
  auto sender = this->make_sender_();
  auto receiver = this->make_receiver_("other-device");
  sender->loop();
  auto packets = this->link_a_.sent;
  this->link_a_.deliver_to(this->link_b_);
  EXPECT_FALSE(this->remote_temp_.has_state());
  // The dictionary named none of the receiver's entities, so it is not asked for again
  EXPECT_TRUE(this->link_b_.sent.empty());
  for (auto &packet : packets)
    this->link_a_.inject(packet);
  this->link_a_.inject(header(PACKET_REQUEST, 0x12345678));
  sender->loop();
  EXPECT_TRUE(this->link_a_.sent.empty());
}

TEST_F(EntityTransportTest, MalformedPacketsIgnored) {
  auto sender = this->make_sender_();
  auto receiver = this->make_receiver_();
  sender->loop();
  Packet dictionary = this->link_a_.sent[0];
  Packet data = this->link_a_.sent[1];
  this->link_b_.inject(Packet(dictionary.begin(), dictionary.end() - 1));
  this->link_b_.inject(dictionary);
  this->link_b_.inject(Packet(data.begin(), data.end() - 1));
  this->link_b_.inject({PACKET_DATA, 1, 2});
  this->link_b_.inject({0x21, 0, 0, 0, 0});
  EXPECT_FALSE(this->remote_temp_.has_state());
}

TEST_F(EntityTransportTest, UpdateIntervalFollowsCapabilities) {
  EXPECT_EQ(this->make_sender_()->get_update_interval(), 15000);
  this->link_a_.set_capabilities(packet_interface::CAPABILITY_SESSION);
  EXPECT_EQ(this->make_sender_()->get_update_interval(), 15000);
  this->link_a_.set_capabilities(packet_interface::CAPABILITY_SESSION | packet_interface::CAPABILITY_RELIABILITY);
  EXPECT_EQ(this->make_sender_()->get_update_interval(), SCHEDULER_DONT_RUN);
  auto transport = std::make_unique<TestTransport>(&this->link_a_);
  transport->set_update_interval(5000);
  transport->setup();
  EXPECT_EQ(transport->get_update_interval(), 5000);
}

TEST_F(EntityTransportTest, SessionConnectSendsAllAndLossClearsRemote) {
  this->link_a_.set_capabilities(packet_interface::CAPABILITY_SESSION);
  this->link_b_.set_capabilities(packet_interface::CAPABILITY_SESSION);
  this->temp_.publish_state(7.0f);
  auto sender = this->make_sender_();
  auto receiver = this->make_receiver_();
  sender->loop();
  this->link_a_.deliver_to(this->link_b_);
  this->link_a_.report_session(true);
  sender->loop();
  EXPECT_EQ(this->link_a_.sent.size(), 2);
  this->link_b_.report_session(false);
  EXPECT_TRUE(std::isnan(this->remote_temp_.get_state()));
  EXPECT_FALSE(this->remote_door_.has_state());
}

TEST_F(EntityTransportTest, RefusedDictionaryPartResumes) {
  // One entity in each dictionary packet
  MockInterface link(5 + 1 + strlen(DEVICE_NAME) + 3 + 5);
  this->temp_.publish_state(4.0f);
  this->door_.publish_state(true);
  auto sender = this->make_sender_(&link);
  auto receiver = this->make_receiver_();
  link.budget = 1;
  sender->loop();
  ASSERT_EQ(link.sent.size(), 1);
  link.budget = SIZE_MAX;
  sender->loop();
  int dictionaries = 0;
  for (auto &packet : link.sent)
    dictionaries += packet[0] == PACKET_DICTIONARY;
  EXPECT_EQ(dictionaries, 2);  // the first part was not sent again
  link.deliver_to(this->link_b_);
  EXPECT_EQ(this->remote_temp_.get_state(), 4.0f);
  EXPECT_TRUE(this->remote_door_.state);
}

TEST_F(EntityTransportTest, RemoteStatesTimeOut) {
  this->temp_.publish_state(6.0f);
  this->door_.publish_state(true);
  auto sender = this->make_sender_();
  auto receiver = this->make_receiver_(DEVICE_NAME, 5000);
  { LoopBlockingGuard guard{nullptr, nullptr, 1000}; }
  sender->loop();
  this->link_a_.deliver_to(this->link_b_);
  { LoopBlockingGuard guard{nullptr, nullptr, 6000}; }
  receiver->clear_remote_states_(true);
  EXPECT_EQ(this->remote_temp_.get_state(), 6.0f);
  EXPECT_TRUE(this->remote_door_.has_state());
  { LoopBlockingGuard guard{nullptr, nullptr, 6001}; }
  receiver->clear_remote_states_(true);
  EXPECT_TRUE(std::isnan(this->remote_temp_.get_state()));
  EXPECT_FALSE(this->remote_door_.has_state());
}

TEST_F(EntityTransportTest, IdMatchingOnlyUpToNulNotLearnt) {
  auto receiver = this->make_receiver_();
  static constexpr uint32_t OTHER_ID = 0x0BADF00D;
  Packet dictionary = header(PACKET_DICTIONARY, OTHER_ID);
  dictionary.push_back(strlen(DEVICE_NAME));
  dictionary.insert(dictionary.end(), DEVICE_NAME, DEVICE_NAME + strlen(DEVICE_NAME));
  // The id "temp" followed by a NUL and more bytes, nine in all
  dictionary.insert(dictionary.end(), {1, 0, 1, 9, 't', 'e', 'm', 'p', 0, 'a', 'b', 'c', 'd'});
  this->link_b_.inject(dictionary);
  Packet data = header(PACKET_DATA, OTHER_ID);
  data.insert(data.end(), {1, 0, 0x00, 0x00, 0x80, 0x3F});
  this->link_b_.inject(data);
  EXPECT_FALSE(this->remote_temp_.has_state());
}

TEST_F(EntityTransportTest, PacketTooSmallForIdsFailsSetup) {
  // One byte short of a dictionary packet holding the device name and a four character id
  MockInterface small_link(5 + 1 + strlen(DEVICE_NAME) + 3 + 1 + 4 - 1);
  auto sender = this->make_sender_(&small_link);
  EXPECT_TRUE(sender->is_failed());
  EXPECT_FALSE(this->make_sender_()->is_failed());
}

}  // namespace esphome::entity_transport::testing
