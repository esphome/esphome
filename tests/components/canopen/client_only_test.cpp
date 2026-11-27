#include <gtest/gtest.h>

#include <array>
#include <string>
#include <vector>

#include "esphome/components/canopen/canopen.h"
#include "esphome/core/hal.h"
#include "fake_canbus.h"

namespace esphome::canopen::testing {

// a hub without node id (SDO client only) and a node on one canbus, linked as codegen does
struct ClientOnly : ::testing::Test {
  FakeCanbus bus;
  TestHub client{&this->bus, CANopenComponent::NO_NODE_ID};
  TestHub node{&this->bus, 0x10};
  void SetUp() override {
    client.set_next_local(&node);
    node.set_next_local(&client);
  }
  void start() {
    client.setup();
    node.setup();
    ASSERT_FALSE(client.is_failed());
    ASSERT_FALSE(node.is_failed());
  }
};

struct Result {
  int done{0};
  SDOReply reply{};
  std::vector<uint8_t> data;
  static void on_reply(void *context, const SDOReply &reply) {
    auto *r = static_cast<Result *>(context);
    r->done++;
    r->reply = reply;
    if (reply.data != nullptr)
      r->data.assign(reply.data, reply.data + reply.len);
  }
};

TEST_F(ClientOnly, IsClientOnly) {
  EXPECT_TRUE(client.is_client_only());
  EXPECT_FALSE(node.is_client_only());
}

TEST_F(ClientOnly, SendsNothingOnItsOwn) {
  start();
  for (int i = 0; i < 5; i++)
    client.loop();
  // no boot-up / heartbeat (0x700 + id), no EMCY: only the node's own boot-up after its loop()
  EXPECT_TRUE(bus.sent.empty());
  node.loop();
  EXPECT_EQ(bus.count(0x710), 1u);
  EXPECT_EQ(bus.sent.size(), 1u);
}

TEST_F(ClientOnly, ReadWriteAndBufferRead) {
  node.set_identity(0xA59A08F5, 1, 2, 3);
  node.set_device_name("test-node");
  uint32_t value = 0;
  node.get_od().add_var(0x2000, OD_READ_WRITE, &value, sizeof(value));
  start();
  Result result;
  auto run = [&](int until) {
    for (int i = 0; i < 50 && result.done < until; i++) {
      client.loop();
      node.loop();
    }
  };

  ASSERT_TRUE(client.read_sdo<uint32_t>(0x10, 0x1018, 1, Result::on_reply, &result));
  run(1);
  ASSERT_EQ(result.done, 1);
  EXPECT_TRUE(result.reply.ok());
  EXPECT_EQ(result.reply.as<uint32_t>(), 0xA59A08F5u);
  EXPECT_EQ(bus.count(0x610), 1u);
  EXPECT_EQ(bus.count(0x590), 1u);

  ASSERT_TRUE(client.write_sdo<uint32_t>(0x10, 0x2000, 0, 0x12345678, Result::on_reply, &result));
  run(2);
  ASSERT_EQ(result.done, 2);
  EXPECT_TRUE(result.reply.ok());
  EXPECT_EQ(value, 0x12345678u);

  std::array<uint8_t, 32> buffer{};
  ASSERT_TRUE(client.read_sdo_data(0x10, 0x1008, 0, buffer.data(), buffer.size(), Result::on_reply, &result));
  run(3);
  ASSERT_EQ(result.done, 3);
  EXPECT_TRUE(result.reply.ok());
  EXPECT_EQ(std::string(result.data.begin(), result.data.end()), "test-node");
}

TEST_F(ClientOnly, RequestToAnAbsentNodeTimesOut) {
  client.set_response_timeout(5);
  start();
  Result result;
  ASSERT_TRUE(client.read_sdo<uint32_t>(0x20, 0x1018, 1, Result::on_reply, &result));
  for (int i = 0; i < 100 && result.done == 0; i++) {
    client.loop();
    advance_loop_clock(1);
  }
  ASSERT_EQ(result.done, 1);
  EXPECT_EQ(result.reply.result, SDOResult::SDO_RESULT_TIMEOUT);
  // the upload request, then the client's abort (SDO protocol timed out)
  ASSERT_EQ(bus.count(0x620), 2u);
  EXPECT_EQ(bus.sent[0].data[0], 0x40);
  EXPECT_EQ(bus.sent[1].data[0], 0x80);
  EXPECT_EQ(bus.sent[1].data[7], 0x05);
}

TEST_F(ClientOnly, NoNmt) {
  start();
  EXPECT_FALSE(client.set_nmt_state(NMTState::NMT_STATE_STOPPED));
  EXPECT_EQ(client.get_nmt_state(), NMTState::NMT_STATE_UNKNOWN);
}

}  // namespace esphome::canopen::testing
