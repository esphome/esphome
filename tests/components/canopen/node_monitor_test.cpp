#include <gtest/gtest.h>

#include <chrono>
#include <cstdio>
#include <thread>
#include <vector>

#include "fake_canbus.h"

// Node monitoring (CANopenNode's heartbeat consumer, node events) and the NMT master.
namespace esphome::canopen::testing {

#ifdef USE_CANOPEN_NODE_MONITOR
struct Event {
  uint8_t node_id;
  NodeEvent event;
  NMTState state;
  bool operator==(const Event &other) const {
    return this->node_id == other.node_id && this->event == other.event && this->state == other.state;
  }
};

// A node (0x10) monitoring node 0x21, recording its node events.
class NodeMonitorTest : public ::testing::Test {
 protected:
  FakeCanbus bus_;
  TestHub hub_{&this->bus_};
  std::vector<Event> events_;

  void start_(uint16_t timeout_ms) {
    this->hub_.monitor_node(0x21, timeout_ms);
    this->hub_.add_on_node_event_callback(
        [this](uint8_t node_id, NodeEvent event, NMTState state) { this->events_.push_back({node_id, event, state}); });
    this->hub_.setup();
    ASSERT_FALSE(this->hub_.is_failed());
    this->hub_.loop();
  }
};

TEST_F(NodeMonitorTest, EventsFromHeartbeats) {
  this->start_(1000);
  EXPECT_FALSE(this->hub_.is_node_online(0x21));
  EXPECT_EQ(this->hub_.get_node_state(0x21), NMTState::NMT_STATE_UNKNOWN);

  // the first heartbeat: online, with its state
  this->hub_.frame(0x721, {0x05});
  ASSERT_FALSE(this->events_.empty());
  EXPECT_EQ(this->events_.front(), (Event{0x21, NodeEvent::NODE_EVENT_ONLINE, NMTState::NMT_STATE_OPERATIONAL}));
  EXPECT_TRUE(this->hub_.is_node_online(0x21));
  EXPECT_EQ(this->hub_.get_node_state(0x21), NMTState::NMT_STATE_OPERATIONAL);

  // another state in the heartbeat
  this->events_.clear();
  this->hub_.frame(0x721, {0x7F});
  ASSERT_FALSE(this->events_.empty());
  EXPECT_EQ(this->events_.back(),
            (Event{0x21, NodeEvent::NODE_EVENT_NMT_CHANGED, NMTState::NMT_STATE_PRE_OPERATIONAL}));
  EXPECT_EQ(this->hub_.get_node_state(0x21), NMTState::NMT_STATE_PRE_OPERATIONAL);

  // a boot-up message: the node restarted
  this->events_.clear();
  this->hub_.frame(0x721, {0x00});
  ASSERT_FALSE(this->events_.empty());
  EXPECT_EQ(this->events_.front().event, NodeEvent::NODE_EVENT_RESET);

  // a node that isn't monitored
  this->hub_.frame(0x722, {0x05});
  for (const Event &e : this->events_)
    EXPECT_EQ(e.node_id, 0x21);
  EXPECT_FALSE(this->hub_.is_node_online(0x22));
  EXPECT_EQ(this->hub_.get_node_state(0x22), NMTState::NMT_STATE_UNKNOWN);
}

// Frames before the hub's first loop(): a heartbeat and a boot-up aren't lost to the first processing (which takes the
// node to operational).
TEST_F(NodeMonitorTest, HeartbeatBeforeTheFirstLoop) {
  this->hub_.monitor_node(0x21, 1000);
  this->hub_.add_on_node_event_callback(
      [this](uint8_t node_id, NodeEvent event, NMTState state) { this->events_.push_back({node_id, event, state}); });
  this->hub_.setup();
  ASSERT_FALSE(this->hub_.is_failed());
  this->hub_.frame(0x721, {0x05});
  ASSERT_FALSE(this->events_.empty());
  EXPECT_EQ(this->events_.front(), (Event{0x21, NodeEvent::NODE_EVENT_ONLINE, NMTState::NMT_STATE_OPERATIONAL}));
  EXPECT_TRUE(this->hub_.is_node_online(0x21));
}

TEST_F(NodeMonitorTest, BootupBeforeTheFirstLoop) {
  this->hub_.monitor_node(0x21, 1000);
  this->hub_.add_on_node_event_callback(
      [this](uint8_t node_id, NodeEvent event, NMTState state) { this->events_.push_back({node_id, event, state}); });
  this->hub_.setup();
  ASSERT_FALSE(this->hub_.is_failed());
  this->hub_.frame(0x721, {0x00});
  ASSERT_FALSE(this->events_.empty());
  EXPECT_EQ(this->events_.front().event, NodeEvent::NODE_EVENT_RESET);
}

// The node is processed on the real clock (micros()), so the test waits out a short timeout.
TEST_F(NodeMonitorTest, TimeoutReportsOffline) {
  this->start_(30);
  this->hub_.frame(0x721, {0x05});
  ASSERT_TRUE(this->hub_.is_node_online(0x21));
  this->events_.clear();
  std::this_thread::sleep_for(std::chrono::milliseconds(60));
  this->hub_.loop();
  ASSERT_FALSE(this->events_.empty());
  EXPECT_EQ(this->events_.front(), (Event{0x21, NodeEvent::NODE_EVENT_OFFLINE, NMTState::NMT_STATE_UNKNOWN}));
  EXPECT_FALSE(this->hub_.is_node_online(0x21));
  EXPECT_EQ(this->hub_.get_node_state(0x21), NMTState::NMT_STATE_UNKNOWN);
  // the next heartbeat brings it back
  this->events_.clear();
  this->hub_.frame(0x721, {0x05});
  ASSERT_FALSE(this->events_.empty());
  EXPECT_EQ(this->events_.front().event, NodeEvent::NODE_EVENT_ONLINE);
}

// An event's automation sees the state it reports: CANopenNode calls the consumer callbacks before it stores the new
// state, so the events are dispatched after CO_process().
TEST(CanopenNodeMonitor, QueriesInAnEventSeeTheNewState) {
  struct Seen {
    NodeEvent event;
    bool online;
    NMTState state;
  };
  FakeCanbus bus;
  TestHub hub{&bus};
  std::vector<Seen> seen;
  hub.monitor_node(0x21, 30);
  hub.add_on_node_event_callback([&](uint8_t node_id, NodeEvent event, NMTState state) {
    seen.push_back({event, hub.is_node_online(node_id), hub.get_node_state(node_id)});
  });
  hub.setup();
  ASSERT_FALSE(hub.is_failed());
  hub.loop();
  auto find = [&](NodeEvent event) {
    for (const Seen &s : seen) {
      if (s.event == event)
        return s;
    }
    ADD_FAILURE() << "no event " << static_cast<int>(event);
    return Seen{event, false, NMTState::NMT_STATE_UNKNOWN};
  };

  hub.frame(0x721, {0x05});
  Seen online = find(NodeEvent::NODE_EVENT_ONLINE);
  EXPECT_TRUE(online.online);
  EXPECT_EQ(online.state, NMTState::NMT_STATE_OPERATIONAL);

  seen.clear();
  hub.frame(0x721, {0x00});
  Seen reset = find(NodeEvent::NODE_EVENT_RESET);
  EXPECT_FALSE(reset.online);
  EXPECT_EQ(reset.state, NMTState::NMT_STATE_UNKNOWN);

  hub.frame(0x721, {0x05});
  ASSERT_TRUE(hub.is_node_online(0x21));
  seen.clear();
  std::this_thread::sleep_for(std::chrono::milliseconds(60));
  hub.loop();
  Seen offline = find(NodeEvent::NODE_EVENT_OFFLINE);
  EXPECT_FALSE(offline.online);
  EXPECT_EQ(offline.state, NMTState::NMT_STATE_UNKNOWN);
}

// 0x1016 holds each node once, with the shortest timeout it was registered with; invalid entries and nodes added after
// setup are ignored. Read only: CANopenNode's consumer isn't built to take writes.
TEST(CanopenNodeMonitor, ConsumerEntries) {
  FakeCanbus bus;
  TestHub hub(&bus);
  hub.monitor_node(0x21, 3000);
  hub.monitor_node(0x22, 500);
  hub.monitor_node(0x21, 1500);
  hub.monitor_node(0x21, 2000);
  hub.monitor_node(0, 1000);     // no such node
  hub.monitor_node(128, 1000);   // no such node
  hub.monitor_node(0x10, 1000);  // our own id
  hub.monitor_node(0x24, 0);     // no timeout
  hub.setup();
  ASSERT_FALSE(hub.is_failed());
  hub.monitor_node(0x23, 1000);  // after setup: logged, ignored
  OD_entry_t *entry = hub.get_od().find(0x1016);
  ASSERT_NE(entry, nullptr);
  uint8_t count = 0;
  ASSERT_EQ(OD_get_u8(entry, 0, &count, true), ODR_OK);
  EXPECT_EQ(count, 2);
  uint32_t value = 0;
  ASSERT_EQ(OD_get_u32(entry, 1, &value, true), ODR_OK);
  EXPECT_EQ(value, (0x21u << 16) | 1500);
  ASSERT_EQ(OD_get_u32(entry, 2, &value, true), ODR_OK);
  EXPECT_EQ(value, (0x22u << 16) | 500);
  OD_IO_t io{};
  ASSERT_EQ(OD_getSub(entry, 1, &io, false), ODR_OK);
  EXPECT_EQ(io.stream.attribute & ODA_SDO_W, 0);
}
#endif  // USE_CANOPEN_NODE_MONITOR

#ifdef USE_CANOPEN_NMT_MASTER
TEST(CanopenNmtMaster, Commands) {
  FakeCanbus bus;
  TestHub hub(&bus);
  hub.setup();
  ASSERT_FALSE(hub.is_failed());
  hub.loop();
  bus.sent.clear();

  // to another node: one frame on 0x000, the node's own state unchanged
  ASSERT_TRUE(hub.send_nmt(0x21, NMTCommand::NMT_COMMAND_START));
  ASSERT_EQ(bus.count(0x000), 1u);
  EXPECT_EQ(bus.find(0x000)->data, (std::vector<uint8_t>{0x01, 0x21}));
  hub.loop();
  EXPECT_EQ(hub.get_nmt_state(), NMTState::NMT_STATE_OPERATIONAL);

  // to its own id: sent, and applied to the node itself
  ASSERT_TRUE(hub.send_nmt(0x10, NMTCommand::NMT_COMMAND_PRE_OPERATIONAL));
  EXPECT_EQ(bus.sent.back().data, (std::vector<uint8_t>{0x80, 0x10}));
  hub.loop();
  EXPECT_EQ(hub.get_nmt_state(), NMTState::NMT_STATE_PRE_OPERATIONAL);

  // to all nodes (0): applied to the node itself too
  ASSERT_TRUE(hub.send_nmt(0, NMTCommand::NMT_COMMAND_STOP));
  EXPECT_EQ(bus.sent.back().data, (std::vector<uint8_t>{0x02, 0x00}));
  hub.loop();
  EXPECT_EQ(hub.get_nmt_state(), NMTState::NMT_STATE_STOPPED);

  // no such node id
  const size_t sent = bus.sent.size();
  EXPECT_FALSE(hub.send_nmt(128, NMTCommand::NMT_COMMAND_START));
  EXPECT_EQ(bus.sent.size(), sent);
}

// A command while the previous one waits in the TX buffer is refused and doesn't replace it.
TEST(CanopenNmtMaster, CommandWhileTheFrameWaits) {
  FakeCanbus bus;
  TestHub hub(&bus);
  hub.setup();
  ASSERT_FALSE(hub.is_failed());
  hub.loop();
  bus.sent.clear();
  bus.busy = true;
  ASSERT_TRUE(hub.send_nmt(0x21, NMTCommand::NMT_COMMAND_START));  // queued
  EXPECT_FALSE(hub.send_nmt(0, NMTCommand::NMT_COMMAND_RESET_NODE));
  bus.busy = false;
  hub.loop();
  ASSERT_EQ(bus.count(0x000), 1u);
  EXPECT_EQ(bus.find(0x000)->data, (std::vector<uint8_t>{0x01, 0x21}));
  EXPECT_EQ(hub.get_nmt_state(), NMTState::NMT_STATE_OPERATIONAL);
}

// Commands for us (or all nodes) are applied right away: CANopenNode keeps one until its processing, where a received
// NMT frame or set_nmt_state() would replace it.
class NmtForUsTest : public ::testing::Test {
 protected:
  FakeCanbus bus_;
  TestHub hub_{&this->bus_};
  std::vector<NMTState> states_;

  void SetUp() override {
    this->hub_.add_on_nmt_state_callback([this](NMTState state) { this->states_.push_back(state); });
    this->hub_.setup();
    ASSERT_FALSE(this->hub_.is_failed());
    this->hub_.loop();
    ASSERT_EQ(this->hub_.get_nmt_state(), NMTState::NMT_STATE_OPERATIONAL);
    this->states_.clear();
    this->bus_.sent.clear();
  }
};

TEST_F(NmtForUsTest, AppliedRightAway) {
  ASSERT_TRUE(this->hub_.send_nmt(0, NMTCommand::NMT_COMMAND_STOP));
  EXPECT_EQ(this->hub_.get_nmt_state(), NMTState::NMT_STATE_STOPPED);
  ASSERT_TRUE(this->hub_.send_nmt(0x10, NMTCommand::NMT_COMMAND_START));
  EXPECT_EQ(this->hub_.get_nmt_state(), NMTState::NMT_STATE_OPERATIONAL);
  EXPECT_EQ(this->bus_.count(0x000), 2u);
  EXPECT_EQ(this->states_, (std::vector<NMTState>{NMTState::NMT_STATE_STOPPED, NMTState::NMT_STATE_OPERATIONAL}));
}

TEST_F(NmtForUsTest, ReceivedCommandDoesntReplaceOurs) {
  ASSERT_TRUE(this->hub_.send_nmt(0x10, NMTCommand::NMT_COMMAND_STOP));
  this->hub_.frame(0x000, {0x01, 0x10});  // a master on the bus starts us
  this->hub_.loop();
  EXPECT_EQ(this->states_, (std::vector<NMTState>{NMTState::NMT_STATE_STOPPED, NMTState::NMT_STATE_OPERATIONAL}));
}

TEST_F(NmtForUsTest, SetNmtStateDoesntReplaceOurs) {
  ASSERT_TRUE(this->hub_.send_nmt(0, NMTCommand::NMT_COMMAND_STOP));
  ASSERT_TRUE(this->hub_.set_nmt_state(NMTState::NMT_STATE_PRE_OPERATIONAL));
  this->hub_.loop();
  EXPECT_EQ(this->states_, (std::vector<NMTState>{NMTState::NMT_STATE_STOPPED, NMTState::NMT_STATE_PRE_OPERATIONAL}));
}

// From an automation the processing runs, a command for us waits for the next processing, and a second one is refused.
TEST_F(NmtForUsTest, FromInsideTheProcessing) {
  bool first = false, second = true, set = true;
  this->hub_.add_on_nmt_state_callback([&](NMTState state) {
    if (state != NMTState::NMT_STATE_STOPPED)
      return;
    first = this->hub_.send_nmt(0x10, NMTCommand::NMT_COMMAND_PRE_OPERATIONAL);
    second = this->hub_.send_nmt(0, NMTCommand::NMT_COMMAND_START);
    set = this->hub_.set_nmt_state(NMTState::NMT_STATE_OPERATIONAL);
  });
  ASSERT_TRUE(this->hub_.send_nmt(0x10, NMTCommand::NMT_COMMAND_STOP));
  EXPECT_TRUE(first);
  EXPECT_FALSE(second);
  EXPECT_FALSE(set);
  EXPECT_EQ(this->hub_.get_nmt_state(), NMTState::NMT_STATE_STOPPED);
  this->hub_.loop();
  EXPECT_EQ(this->hub_.get_nmt_state(), NMTState::NMT_STATE_PRE_OPERATIONAL);
}

// A log handler inside the send (here the canbus driver) calling send_nmt() again is refused: it would overwrite the
// frame being sent and count CANopenNode's TX buffer twice.
TEST(CanopenNmtMaster, CommandFromInsideItsOwnSend) {
  FakeCanbus bus;
  TestHub hub(&bus);
  hub.setup();
  ASSERT_FALSE(hub.is_failed());
  hub.loop();
  bus.sent.clear();
  bool nested = true;
  bus.on_send = [&](uint32_t) {
    bus.on_send = nullptr;
    nested = hub.send_nmt(0x22, NMTCommand::NMT_COMMAND_STOP);
  };
  bus.busy = true;
  ASSERT_TRUE(hub.send_nmt(0x21, NMTCommand::NMT_COMMAND_START));  // queued
  EXPECT_FALSE(nested);
  bus.busy = false;
  hub.loop();
  ASSERT_EQ(bus.count(0x000), 1u);
  EXPECT_EQ(bus.find(0x000)->data, (std::vector<uint8_t>{0x01, 0x21}));
  EXPECT_EQ(hub.tx_count(), 0u);
}

// A reset_node to all nodes reboots us (safe_reboot() exits on the host) only 20 ms after its frame is handed to the
// driver (the controller may still hold it), or after 100 ms.
// Each case runs in a death test's child: a reboot too early would end the test binary.
TEST(CanopenNmtMaster, ResetNodeRebootsAfterItsFrame) {
  FakeCanbus bus;
  TestHub hub(&bus);
  hub.setup();
  ASSERT_FALSE(hub.is_failed());
  hub.loop();
  bus.busy = true;
  ASSERT_TRUE(hub.send_nmt(0, NMTCommand::NMT_COMMAND_RESET_NODE));  // queued
  auto wait = [&hub]() {
    hub.loop();
    advance_loop_clock(50);
    hub.loop();
  };
  // still waiting for the frame
  EXPECT_EXIT(
      {
        wait();
        exit(bus.count(0x000) == 0 ? 3 : 4);
      },
      ::testing::ExitedWithCode(3), "");
  // handed to the driver: not rebooted yet
  EXPECT_EXIT(
      {
        wait();
        bus.busy = false;
        hub.loop();
        advance_loop_clock(19);
        hub.loop();
        exit(bus.count(0x000) == 1 ? 3 : 4);
      },
      ::testing::ExitedWithCode(3), "");
  // rebooted 20 ms after the hand-over; on_send runs before the frame is taken or refused, so only a frame the
  // driver takes counts
  bus.on_send = [&bus](uint32_t can_id) {
    if (can_id == 0x000 && !bus.busy)
      fprintf(stderr, "NMT frame sent\n");
  };
  EXPECT_EXIT(
      {
        wait();
        bus.busy = false;
        hub.loop();
        advance_loop_clock(20);
        hub.loop();
        exit(4);
      },
      ::testing::ExitedWithCode(0), "NMT frame sent");
  // a frame that can't be sent doesn't hold the reset forever
  EXPECT_EXIT(
      {
        wait();
        advance_loop_clock(50);
        hub.loop();
        exit(4);
      },
      ::testing::ExitedWithCode(0), "");
}

// A hub without node id has no NMT object.
TEST(CanopenNmtMaster, ClientOnlyRefuses) {
  FakeCanbus bus;
  TestHub hub(&bus, CANopenComponent::NO_NODE_ID);
  hub.setup();
  ASSERT_FALSE(hub.is_failed());
  EXPECT_FALSE(hub.send_nmt(0x21, NMTCommand::NMT_COMMAND_START));
  EXPECT_EQ(bus.count(0x000), 0u);
}
#endif  // USE_CANOPEN_NMT_MASTER

}  // namespace esphome::canopen::testing
