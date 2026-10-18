#include <gtest/gtest.h>
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <vector>
#include "esphome/components/packet_interface/basic_addressing.h"
#include "esphome/components/packet_interface/hdlc_reliability.h"
#include "esphome/core/application.h"

namespace esphome::packet_interface::addressing_testing {

using Packet = std::vector<uint8_t>;

static constexpr uint16_t BROADCAST = BasicAddressing::BROADCAST_ADDRESS;

// A link layer that records what is sent, so a test can deliver it to other devices
class MockLink : public PacketInterface {
 public:
  MockLink() { this->set_max_packet_size(64); }
  bool send_to_interface(const PacketBuffer &data, const PacketMetaData &) override {
    this->sent.push_back(data.to_vector());
    return true;
  }
  void inject(const Packet &packet) { this->on_receive_from_interface_(PacketBuffer(packet), {}); }

  std::vector<Packet> sent;
};

// What one port receives
struct Port {
  Port(BasicAddressing *addressing, uint16_t destination, bool only_port) {
    this->port = std::make_unique<AddressingPort>(addressing, destination, only_port);
    this->port->add_packet_interface_listener([this](const PacketBuffer &data, const PacketMetaData &meta_data) {
      this->received.push_back(data.to_vector());
      this->sources.push_back(meta_data.source_address);
    });
  }
  void send(const Packet &data, uint16_t destination = 0) {
    PacketMetaData meta_data;
    meta_data.destination_address = destination;
    this->port->send_to_interface(PacketBuffer(data), meta_data);
  }

  std::unique_ptr<AddressingPort> port;
  std::vector<Packet> received;
  std::vector<uint16_t> sources;
};

// A device on the bus with one port per destination given
struct Device {
  Device(uint16_t address, std::initializer_list<uint16_t> destinations) : addressing(&this->link, address) {
    this->link.set_name("bus");
    for (uint16_t destination : destinations)
      this->ports.push_back(std::make_unique<Port>(&this->addressing, destination, destinations.size() == 1));
    this->addressing.setup();
  }
  explicit Device(uint16_t address, uint16_t destination = BROADCAST) : Device(address, {destination}) {}
  Port &port(size_t index = 0) { return *this->ports[index]; }
  // Every device on the bus hears every packet, including its own
  void deliver(std::initializer_list<Device *> bus) {
    for (auto &packet : this->link.sent) {
      for (auto *device : bus)
        device->link.inject(packet);
    }
    this->link.sent.clear();
  }

  MockLink link;
  BasicAddressing addressing;
  std::vector<std::unique_ptr<Port>> ports;
};

TEST(BasicAddressingTest, HeaderAndCapability) {
  Device a(0x1234, 0x5678);
  EXPECT_EQ(a.port().port->get_max_packet_size(), 64 - BasicAddressing::HEADER_SIZE);
  EXPECT_TRUE(a.port().port->has_capability(CAPABILITY_ADDRESSING));
  EXPECT_EQ(a.link.top(), a.port().port.get());
  a.port().send({0xAA});
  a.port().send({0xBB}, 0x0042);
  EXPECT_EQ(a.link.sent, (std::vector<Packet>{{0x78, 0x56, 0x34, 0x12, 0xAA}, {0x42, 0x00, 0x34, 0x12, 0xBB}}));
}

TEST(BasicAddressingTest, BroadcastReachesEveryoneButSender) {
  Device a(1), b(2), c(3);
  a.port().send({0x10});
  a.deliver({&a, &b, &c});
  EXPECT_TRUE(a.port().received.empty());
  EXPECT_EQ(b.port().received, std::vector<Packet>{{0x10}});
  EXPECT_EQ(c.port().received, std::vector<Packet>{{0x10}});
  EXPECT_EQ(b.port().sources, std::vector<uint16_t>{1});
}

TEST(BasicAddressingTest, UnicastReachesOnlyDestination) {
  Device a(1, 2), b(2), c(3);
  a.port().send({0x20});
  a.deliver({&b, &c});
  EXPECT_EQ(b.port().received, std::vector<Packet>{{0x20}});
  EXPECT_TRUE(c.port().received.empty());
}

TEST(BasicAddressingTest, PointToPointAcceptsOnlyPeer) {
  Device a(1, 2), b(2, 1), c(3);
  c.port().send({0x30});     // broadcast from a third device
  c.port().send({0x31}, 2);  // addressed to b, from the wrong device
  a.port().send({0x32});     // from the peer
  c.deliver({&b});
  a.deliver({&b});
  EXPECT_EQ(b.port().received, std::vector<Packet>{{0x32}});
  EXPECT_EQ(b.port().sources, std::vector<uint16_t>{1});
}

TEST(BasicAddressingTest, PeersHaveSeparateStacks) {
  Device hub(1, {2, 3, BROADCAST}), b(2, 1), c(3, 1), d(4);
  EXPECT_EQ(hub.link.top(), &hub.link);  // with several ports, applications use the ports
  EXPECT_EQ(hub.port(0).port->top(), hub.port(0).port.get());
  b.port().send({0x02});
  c.port().send({0x03});
  d.port().send({0x04});
  b.deliver({&hub});
  c.deliver({&hub});
  d.deliver({&hub});
  EXPECT_EQ(hub.port(0).received, std::vector<Packet>{{0x02}});
  EXPECT_EQ(hub.port(1).received, std::vector<Packet>{{0x03}});
  EXPECT_EQ(hub.port(2).received, std::vector<Packet>{{0x04}});  // only devices without a port of their own
  hub.port(1).send({0x13});
  hub.deliver({&b, &c, &d});
  EXPECT_EQ(c.port().received, std::vector<Packet>{{0x13}});
  EXPECT_TRUE(b.port().received.empty());
  EXPECT_TRUE(d.port().received.empty());
}

TEST(BasicAddressingTest, WithoutBroadcastPortUnknownDevicesDropped) {
  Device hub(1, {2, 3}), d(4);
  d.port().send({0x04});
  d.deliver({&hub});
  EXPECT_TRUE(hub.port(0).received.empty());
  EXPECT_TRUE(hub.port(1).received.empty());
}

TEST(BasicAddressingTest, ShortPacketDropped) {
  Device a(1);
  a.link.inject({0xFF, 0xFF, 0x02});
  EXPECT_TRUE(a.port().received.empty());
}

// The test main does not construct App; the reliability layer's timers need its scheduler
class AddressingReliabilityTest : public ::testing::Test {
 protected:
  void SetUp() override {
    new (&App) Application;
    App.pre_setup("addressing-tests", 16, "", 0);
  }
  void TearDown() override { App.~Application(); }
};

// A reliability layer on each port, recording what it delivers
struct ReliablePort {
  explicit ReliablePort(AddressingPort *port) : hdlc(port) {
    this->hdlc.add_packet_interface_listener(
        [this](const PacketBuffer &data, const PacketMetaData &) { this->received.push_back(data.to_vector()); });
    this->hdlc.setup();
  }
  void send(const Packet &data) { this->hdlc.send_to_interface(PacketBuffer(data), {}); }
  HdlcReliability hdlc;
  std::vector<Packet> received;
};

TEST_F(AddressingReliabilityTest, SeveralReliablePeersOnOneBus) {
  Device hub(1, {2, 3}), b(2, 1), c(3, 1);
  ReliablePort hub_b(hub.port(0).port.get()), hub_c(hub.port(1).port.get());
  ReliablePort b_hub(b.port().port.get()), c_hub(c.port().port.get());
  hub_b.send({0x12});
  hub_c.send({0x13});
  b_hub.send({0x21});
  c_hub.send({0x31});
  for (int i = 0; i != 20; i++) {
    for (auto *port : {&hub_b, &hub_c, &b_hub, &c_hub})
      port->hdlc.loop();
    for (auto *device : {&hub, &b, &c})
      device->deliver({&hub, &b, &c});
  }
  EXPECT_EQ(b_hub.received, std::vector<Packet>{{0x12}});
  EXPECT_EQ(c_hub.received, std::vector<Packet>{{0x13}});
  EXPECT_EQ(hub_b.received, std::vector<Packet>{{0x21}});
  EXPECT_EQ(hub_c.received, std::vector<Packet>{{0x31}});
}

}  // namespace esphome::packet_interface::addressing_testing
