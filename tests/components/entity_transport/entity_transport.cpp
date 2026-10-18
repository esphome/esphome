#include <gtest/gtest.h>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>
#include "esphome/components/entity_transport/entity_transport.h"
#include "esphome/core/application.h"

namespace esphome::entity_transport::testing {

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
  MockInterface() { this->set_max_packet_size(64); }
  bool send_to_interface(const PacketBuffer &data, const PacketMetaData &) override {
    if (!this->accept || this->budget == 0)
      return false;
    this->budget--;
    this->sent.push_back(data.to_vector());
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

static Packet header(uint8_t type, uint32_t dictionary_id) {
  return {type, static_cast<uint8_t>(dictionary_id), static_cast<uint8_t>(dictionary_id >> 8),
          static_cast<uint8_t>(dictionary_id >> 16), static_cast<uint8_t>(dictionary_id >> 24)};
}

class EntityTransportTest : public ::testing::Test {
 protected:
  void SetUp() override {
    new (&App) Application;
    App.pre_setup(DEVICE_NAME, strlen(DEVICE_NAME), "", 0);
  }
  void TearDown() override { App.~Application(); }

  // The sender, with a sensor and a binary sensor
  std::unique_ptr<TestTransport> make_sender() {
    auto transport = std::make_unique<TestTransport>(&this->link_a);
    transport->set_entity_count(2);
    transport->add_sensor("temp", &this->temp);
    transport->add_binary_sensor("door", &this->door);
    transport->setup();
    return transport;
  }
  // The receiver, showing the sender's entities; both run on this device, so they share its name
  std::unique_ptr<TestTransport> make_receiver(const char *provider = DEVICE_NAME, uint32_t timeout_ms = 0) {
    auto transport = std::make_unique<TestTransport>(&this->link_b);
    transport->set_remote_timeout(timeout_ms);
    transport->add_remote_sensor(provider, "temp", &this->remote_temp);
    transport->add_remote_binary_sensor(provider, "door", &this->remote_door);
    transport->setup();
    return transport;
  }

  MockInterface link_a, link_b;
  sensor::Sensor temp, remote_temp;
  binary_sensor::BinarySensor door, remote_door;
};

TEST_F(EntityTransportTest, SendsDictionaryThenStates) {
  this->temp.publish_state(21.5f);
  this->door.publish_state(true);
  auto sender = this->make_sender();
  sender->loop();
  uint32_t id = sender->get_dictionary_id();
  Packet dictionary = header(PACKET_DICTIONARY, id);
  dictionary.push_back(strlen(DEVICE_NAME));
  dictionary.insert(dictionary.end(), DEVICE_NAME, DEVICE_NAME + strlen(DEVICE_NAME));
  dictionary.insert(dictionary.end(), {1, 0, 1, 4, 't', 'e', 'm', 'p', 2, 0, 1, 4, 'd', 'o', 'o', 'r'});
  Packet data = header(PACKET_DATA, id);
  data.insert(data.end(), {1, 0, 0x00, 0x00, 0xAC, 0x41, 0, 1});
  ASSERT_EQ(this->link_a.sent.size(), 2);
  EXPECT_EQ(this->link_a.sent[0], dictionary);
  EXPECT_EQ(this->link_a.sent[1], data);
}

TEST_F(EntityTransportTest, SendsOnlyChangedStates) {
  auto sender = this->make_sender();
  sender->loop();
  this->link_a.sent.clear();
  this->temp.publish_state(3.0f);
  sender->loop();
  Packet data = header(PACKET_DATA, sender->get_dictionary_id());
  data.insert(data.end(), {1, 0, 0x00, 0x00, 0x40, 0x40});
  ASSERT_EQ(this->link_a.sent.size(), 1);
  EXPECT_EQ(this->link_a.sent[0], data);
}

TEST_F(EntityTransportTest, StatesReachRemoteEntities) {
  this->temp.publish_state(12.25f);
  this->door.publish_state(true);
  auto sender = this->make_sender();
  auto receiver = this->make_receiver();
  sender->loop();
  this->link_a.deliver_to(this->link_b);
  EXPECT_EQ(this->remote_temp.get_state(), 12.25f);
  EXPECT_TRUE(this->remote_door.state);
  this->door.invalidate_state();
  sender->loop();
  this->link_a.deliver_to(this->link_b);
  EXPECT_FALSE(this->remote_door.has_state());
}

TEST_F(EntityTransportTest, SplitsAcrossPackets) {
  this->link_a.set_max_packet_size(5 + 1 + strlen(DEVICE_NAME) + 3 + 5);
  this->temp.publish_state(1.0f);
  this->door.publish_state(true);
  auto sender = this->make_sender();
  auto receiver = this->make_receiver();
  sender->loop();
  EXPECT_EQ(this->link_a.sent.size(), 3);  // one dictionary packet for each entity, then one data packet
  this->link_a.deliver_to(this->link_b);
  EXPECT_EQ(this->remote_temp.get_state(), 1.0f);
  EXPECT_TRUE(this->remote_door.state);
}

TEST_F(EntityTransportTest, RefusedPacketSentAgain) {
  this->link_a.accept = false;
  auto sender = this->make_sender();
  sender->loop();
  EXPECT_TRUE(this->link_a.sent.empty());
  this->link_a.accept = true;
  sender->loop();
  EXPECT_EQ(this->link_a.sent.size(), 2);
}

TEST_F(EntityTransportTest, UnknownDictionaryRequested) {
  this->temp.publish_state(5.0f);
  auto sender = this->make_sender();
  auto receiver = this->make_receiver();
  sender->loop();
  this->link_a.sent.erase(this->link_a.sent.begin());  // the dictionary is lost
  this->link_a.deliver_to(this->link_b);
  EXPECT_FALSE(this->remote_temp.has_state());
  ASSERT_EQ(this->link_b.sent.size(), 1);
  EXPECT_EQ(this->link_b.sent[0], header(PACKET_REQUEST, sender->get_dictionary_id()));
  this->link_b.deliver_to(this->link_a);
  sender->loop();
  this->link_a.deliver_to(this->link_b);
  EXPECT_EQ(this->remote_temp.get_state(), 5.0f);
}

TEST_F(EntityTransportTest, RequestsLimited) {
  auto receiver = this->make_receiver();
  Packet data = header(PACKET_DATA, 0x12345678);
  data.push_back(0);
  this->link_b.inject(data);
  this->link_b.inject(data);
  EXPECT_EQ(this->link_b.sent.size(), 1);
}

TEST_F(EntityTransportTest, OtherProviderAndOwnPacketsIgnored) {
  this->temp.publish_state(5.0f);
  auto sender = this->make_sender();
  auto receiver = this->make_receiver("other-device");
  sender->loop();
  auto packets = this->link_a.sent;
  this->link_a.deliver_to(this->link_b);
  EXPECT_FALSE(this->remote_temp.has_state());
  for (auto &packet : packets)
    this->link_a.inject(packet);
  this->link_a.inject(header(PACKET_REQUEST, 0x12345678));
  sender->loop();
  EXPECT_TRUE(this->link_a.sent.empty());
}

TEST_F(EntityTransportTest, MalformedPacketsIgnored) {
  auto sender = this->make_sender();
  auto receiver = this->make_receiver();
  sender->loop();
  Packet dictionary = this->link_a.sent[0];
  Packet data = this->link_a.sent[1];
  this->link_b.inject(Packet(dictionary.begin(), dictionary.end() - 1));
  this->link_b.inject(dictionary);
  this->link_b.inject(Packet(data.begin(), data.end() - 1));
  this->link_b.inject({PACKET_DATA, 1, 2});
  this->link_b.inject({0x21, 0, 0, 0, 0});
  EXPECT_FALSE(this->remote_temp.has_state());
}

TEST_F(EntityTransportTest, UpdateIntervalFollowsCapabilities) {
  EXPECT_EQ(this->make_sender()->get_update_interval(), 15000);
  this->link_a.set_capabilities(packet_interface::CAPABILITY_SESSION);
  EXPECT_EQ(this->make_sender()->get_update_interval(), 15000);
  this->link_a.set_capabilities(packet_interface::CAPABILITY_SESSION | packet_interface::CAPABILITY_RELIABILITY);
  EXPECT_EQ(this->make_sender()->get_update_interval(), SCHEDULER_DONT_RUN);
  auto transport = std::make_unique<TestTransport>(&this->link_a);
  transport->set_update_interval(5000);
  transport->setup();
  EXPECT_EQ(transport->get_update_interval(), 5000);
}

TEST_F(EntityTransportTest, SessionConnectSendsAllAndLossClearsRemote) {
  this->link_a.set_capabilities(packet_interface::CAPABILITY_SESSION);
  this->link_b.set_capabilities(packet_interface::CAPABILITY_SESSION);
  this->temp.publish_state(7.0f);
  auto sender = this->make_sender();
  auto receiver = this->make_receiver();
  sender->loop();
  this->link_a.deliver_to(this->link_b);
  this->link_a.report_session(true);
  sender->loop();
  EXPECT_EQ(this->link_a.sent.size(), 2);
  this->link_b.report_session(false);
  EXPECT_TRUE(std::isnan(this->remote_temp.get_state()));
  EXPECT_FALSE(this->remote_door.has_state());
}

TEST_F(EntityTransportTest, RefusedDictionaryPartResumes) {
  // One entity in each dictionary packet
  this->link_a.set_max_packet_size(5 + 1 + strlen(DEVICE_NAME) + 3 + 5);
  this->temp.publish_state(4.0f);
  this->door.publish_state(true);
  auto sender = this->make_sender();
  auto receiver = this->make_receiver();
  this->link_a.budget = 1;
  sender->loop();
  ASSERT_EQ(this->link_a.sent.size(), 1);
  this->link_a.budget = SIZE_MAX;
  sender->loop();
  int dictionaries = 0;
  for (auto &packet : this->link_a.sent)
    dictionaries += packet[0] == PACKET_DICTIONARY;
  EXPECT_EQ(dictionaries, 2);  // the first part was not sent again
  this->link_a.deliver_to(this->link_b);
  EXPECT_EQ(this->remote_temp.get_state(), 4.0f);
  EXPECT_TRUE(this->remote_door.state);
}

TEST_F(EntityTransportTest, RemoteStatesTimeOut) {
  this->temp.publish_state(6.0f);
  this->door.publish_state(true);
  auto sender = this->make_sender();
  auto receiver = this->make_receiver(DEVICE_NAME, 5000);
  { LoopBlockingGuard guard{nullptr, nullptr, 1000}; }
  sender->loop();
  this->link_a.deliver_to(this->link_b);
  { LoopBlockingGuard guard{nullptr, nullptr, 6000}; }
  receiver->clear_remote_states_(true);
  EXPECT_EQ(this->remote_temp.get_state(), 6.0f);
  EXPECT_TRUE(this->remote_door.has_state());
  { LoopBlockingGuard guard{nullptr, nullptr, 6001}; }
  receiver->clear_remote_states_(true);
  EXPECT_TRUE(std::isnan(this->remote_temp.get_state()));
  EXPECT_FALSE(this->remote_door.has_state());
}

}  // namespace esphome::entity_transport::testing
