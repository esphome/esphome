#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "esphome/components/canopen/canopen_node.h"

namespace esphome::canopen::testing {

// the hub's event sources are protected: raise them as CANopenNode's heartbeat consumer / frame callbacks do
class EventHub : public CANopenComponent {
 public:
  EventHub() : CANopenComponent(nullptr, 0x10) {}
  void node_event(uint8_t node_id, NodeEvent event, NMTState state) { this->node_event_(node_id, event, state); }
  void emcy(uint8_t node_id, const Emergency &emergency) { this->emcy_callback_.call(node_id, emergency); }
};

class RecordingNode : public CANopenNode {
 public:
  using CANopenNode::CANopenNode;
  std::vector<std::string> calls;
  std::vector<uint16_t> emcy_codes;

 protected:
  void on_node_online() override { this->calls.emplace_back("online"); }
  void on_node_offline() override { this->calls.emplace_back("offline"); }
  void on_node_nmt_changed(NMTState state) override {
    this->calls.emplace_back(std::string("nmt ") + LOG_STR_ARG(nmt_state_to_string(state)));
  }
  void on_node_reset() override { this->calls.emplace_back("reset"); }
  void on_node_emcy(const Emergency &emergency) override { this->emcy_codes.push_back(emergency.error_code); }
};

TEST(CanopenNode, HooksForOwnNodeOnly) {
  EventHub hub;
  RecordingNode drive(&hub, 0x21);
  RecordingNode other(&hub, 0x22);

  hub.node_event(0x21, NodeEvent::NODE_EVENT_ONLINE, NMTState::NMT_STATE_PRE_OPERATIONAL);
  hub.node_event(0x21, NodeEvent::NODE_EVENT_NMT_CHANGED, NMTState::NMT_STATE_OPERATIONAL);
  hub.node_event(0x21, NodeEvent::NODE_EVENT_RESET, NMTState::NMT_STATE_BOOTUP);
  hub.node_event(0x21, NodeEvent::NODE_EVENT_OFFLINE, NMTState::NMT_STATE_UNKNOWN);
  hub.emcy(0x21, Emergency{0x8110, 0x11, {}});
  hub.node_event(0x23, NodeEvent::NODE_EVENT_ONLINE, NMTState::NMT_STATE_OPERATIONAL);
  hub.emcy(0x23, Emergency{0x1000, 0x01, {}});

  EXPECT_EQ(drive.calls, (std::vector<std::string>{"online", "nmt operational", "reset", "offline"}));
  EXPECT_EQ(drive.emcy_codes, std::vector<uint16_t>{0x8110});
  EXPECT_TRUE(other.calls.empty());
  EXPECT_TRUE(other.emcy_codes.empty());
}

TEST(CanopenHub, NmtStateBeforeSetup) {
  EventHub hub;
  EXPECT_EQ(hub.get_nmt_state(), NMTState::NMT_STATE_UNKNOWN);
  EXPECT_FALSE(hub.set_nmt_state(NMTState::NMT_STATE_OPERATIONAL));
  uint8_t value = 1;
  EXPECT_EQ(hub.write_od(0x1001, 0, &value, 1), OdResult::OD_RESULT_DEVICE_STATE);
}

TEST(CanopenNode, NodeIdAndUnmonitoredState) {
  EventHub hub;
  RecordingNode drive(&hub, 0x21);

  EXPECT_EQ(drive.get_node_id(), 0x21);
  EXPECT_EQ(drive.get_canopen(), &hub);
  // the hub isn't set up (no heartbeat consumer): unknown, offline
  EXPECT_FALSE(drive.is_online());
  EXPECT_EQ(drive.get_nmt_state(), NMTState::NMT_STATE_UNKNOWN);
  EXPECT_FALSE(drive.is_operational());
}

TEST(CanopenNode, ForwardsWithNodeIdAndContext) {
  EventHub hub;
  RecordingNode drive(&hub, 0x21);
  struct Result {
    void *context{nullptr};
    SDOResult result{SDOResult::SDO_RESULT_SUCCESS};
    int calls{0};
  } result;
  static Result *current = nullptr;
  current = &result;
  auto record = +[](void *context, const SDOReply &reply) {
    current->context = context;
    current->result = reply.result;
    current->calls++;
  };

  // the hub failed: rejected, called back synchronously, with `this` as the default context
  hub.mark_failed();
  EXPECT_FALSE(drive.read_sdo<uint16_t>(0x1017, 0, record));
  EXPECT_EQ(result.calls, 1);
  EXPECT_EQ(result.context, &drive);
  EXPECT_EQ(result.result, SDOResult::SDO_RESULT_REJECTED);

  int own_context = 0;
  EXPECT_FALSE(drive.write_sdo<uint16_t>(0x6040, 0, 0x0F, record, &own_context));
  EXPECT_EQ(result.calls, 2);
  EXPECT_EQ(result.context, &own_context);

  drive.cancel_sdo();
  EXPECT_FALSE(drive.send_nmt(NMTCommand::NMT_COMMAND_START));
  current = nullptr;
}

// a profile with another base before CANopenNode, as components are (Component, CANopenNode)
struct OtherBase {
  virtual ~OtherBase() = default;
  int data{0};
};
class MultiNode : public OtherBase, public CANopenNode {
 public:
  using CANopenNode::CANopenNode;
};

TEST(CanopenNode, FromContextWithOtherBases) {
  EventHub hub;
  MultiNode drive(&hub, 0x21);
  static void *seen = nullptr;
  seen = nullptr;

  // rejected by the failed hub: the callback runs right away
  hub.mark_failed();
  drive.read_sdo<uint16_t>(0x1017, 0, +[](void *context, const SDOReply &) { seen = context; });
  ASSERT_NE(seen, nullptr);
  // the default context is the CANopenNode subobject, not the start of MultiNode
  EXPECT_NE(seen, static_cast<void *>(&drive));
  EXPECT_EQ(CANopenNode::from_context<MultiNode>(seen), &drive);
}

}  // namespace esphome::canopen::testing
