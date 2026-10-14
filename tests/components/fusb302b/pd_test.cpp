#include <gtest/gtest.h>
#include <vector>

#include "esphome/components/fusb302b/pd.h"

namespace esphome::fusb302b::testing {

static constexpr uint32_t fixed_pdo(uint16_t mv, uint16_t ma) { return ((mv / 50u) << 10) | (ma / 10u); }
// An augmented (PPS) object; only its type matters to the sink
static constexpr uint32_t AUGMENTED_PDO = static_cast<uint32_t>(PD_PDO_TYPE_AUGMENTED) << 30;
// Unknown to the sink, so it must be answered with Reject
static constexpr uint8_t PD_CNTRL_DR_SWAP = 0x09;

class FakePhy : public PowerDelivery {
 public:
  using PowerDelivery::ams_start_;
  using PowerDelivery::check_ams_;
  using PowerDelivery::check_transition_timeout_;
  using PowerDelivery::check_request_retry_;
  using PowerDelivery::contract_;
  using PowerDelivery::handle_message_;
  using PowerDelivery::handle_transmit_failed_;
  using PowerDelivery::retry_request_pending_;
  using PowerDelivery::set_state_and_contract_;
  using PowerDelivery::wait_received_ms_;

  std::vector<PdMsg> sent;
  bool send_ok{true};
  int send_attempts{0};
  int state_changes{0};
  int soft_resets{0};
  int source_caps{0};
  int transition_timeouts{0};

  void receive(uint8_t type, uint8_t id, std::vector<uint32_t> objects = {}) {
    PdMsg msg;
    msg.type = type;
    msg.id = id;
    msg.num_of_obj = objects.size();
    for (size_t i = 0; i < objects.size(); i++)
      msg.data_objects[i] = objects[i];
    this->handle_message_(msg);
  }
  void receive_source_caps(uint8_t id) {
    this->receive(PD_DATA_SOURCE_CAP, id,
                  {fixed_pdo(5000, 3000), fixed_pdo(9000, 3000), fixed_pdo(15000, 3000), fixed_pdo(20000, 2250)});
  }
  PackedContract contract() const { return PackedContract{this->contract_}; }
  // What the PHY does when a source is attached
  void attach() { this->set_state_and_contract_(PdState::PD_STATE_DEFAULT_CONTRACT, DEFAULT_CONTRACT); }
  // Moves the timestamps back instead of sleeping, since millis() is real time on the host
  void advance_ms(uint32_t ms) {
    this->ams_start_ -= ms;
    this->wait_received_ms_ -= ms;
  }

 protected:
  bool send_message(const PdMsg &msg) override {
    this->send_attempts++;
    if (!this->send_ok)
      return false;
    this->sent.push_back(msg);
    return true;
  }
  void on_state_changed() override { this->state_changes++; }
  void on_soft_reset_received() override { this->soft_resets++; }
  void on_source_caps_received() override {
    // The hook runs before the request is sent
    EXPECT_EQ(this->sent.size(), static_cast<size_t>(this->source_caps));
    this->source_caps++;
  }
  void on_transition_timeout() override { this->transition_timeouts++; }
};

TEST(FUSB302BPd, HeaderRoundTrip) {
  PdMsg msg;
  msg.type = PD_DATA_REQUEST;
  msg.spec_rev = PD_SPEC_REV_2;
  msg.id = 5;
  msg.num_of_obj = 1;
  // Type 2, spec revision 1 at bit 6, MessageID 5 at bit 9, one object at bit 12
  EXPECT_EQ(msg.get_coded_header(), 0x1A42);

  PdMsg decoded;
  decoded.set_header(0x1A42 | (1u << 15));
  EXPECT_EQ(decoded.type, PD_DATA_REQUEST);
  EXPECT_EQ(decoded.spec_rev, PD_SPEC_REV_2);
  EXPECT_EQ(decoded.id, 5);
  EXPECT_EQ(decoded.num_of_obj, 1);
  EXPECT_TRUE(decoded.extended);
}

TEST(FUSB302BPd, RequestsHighestVoltageNotAboveLimit) {
  FakePhy pd;
  pd.set_request_voltage(18);
  pd.receive_source_caps(0);
  EXPECT_EQ(pd.source_caps, 1);
  ASSERT_EQ(pd.sent.size(), 1u);
  EXPECT_EQ(pd.sent[0].type, PD_DATA_REQUEST);
  EXPECT_EQ(pd.sent[0].data_objects[0] >> 28, 3u);  // 15 V is object 3
  EXPECT_EQ(pd.sent[0].data_objects[0] & 0x3FF, 300u);
}

TEST(FUSB302BPd, LowestRequestPicksFirstObject) {
  FakePhy pd;
  pd.set_request_voltage(5);
  pd.receive_source_caps(0);
  ASSERT_EQ(pd.sent.size(), 1u);
  EXPECT_EQ(pd.sent[0].data_objects[0] >> 28, 1u);
}

TEST(FUSB302BPd, NonFixedObjectKeepsItsPosition) {
  FakePhy pd;
  pd.set_request_voltage(9);
  pd.receive(PD_DATA_SOURCE_CAP, 0, {fixed_pdo(5000, 3000), AUGMENTED_PDO, fixed_pdo(9000, 2000)});
  ASSERT_EQ(pd.sent.size(), 1u);
  EXPECT_EQ(pd.sent[0].data_objects[0] >> 28, 3u);
  EXPECT_EQ(pd.sent[0].data_objects[0] & 0x3FF, 200u);
}

TEST(FUSB302BPd, NoFixedObjectFallsBackToObjectOne) {
  FakePhy pd;
  pd.set_request_voltage(20);
  pd.receive(PD_DATA_SOURCE_CAP, 0, {AUGMENTED_PDO, AUGMENTED_PDO});
  ASSERT_EQ(pd.sent.size(), 1u);
  const uint32_t rdo = pd.sent[0].data_objects[0];
  EXPECT_EQ(rdo >> 28, 1u);
  EXPECT_EQ(rdo & 0x3FF, 30u);          // 300 mA maximum
  EXPECT_EQ((rdo >> 10) & 0x3FF, 10u);  // 100 mA operating
}

TEST(FUSB302BPd, AcceptThenPsRdyReachesExplicitContract) {
  FakePhy pd;
  pd.set_request_voltage(20);
  pd.receive_source_caps(0);
  pd.receive(PD_CNTRL_ACCEPT, 1);
  EXPECT_EQ(pd.get_state(), PdState::PD_STATE_TRANSITION);
  pd.receive(PD_CNTRL_PS_RDY, 2);
  EXPECT_EQ(pd.get_state(), PdState::PD_STATE_EXPLICIT_CONTRACT);
  EXPECT_FLOAT_EQ(pd.contract().voltage(), 20.0f);
  EXPECT_FLOAT_EQ(pd.contract().current(), 2.25f);
}

TEST(FUSB302BPd, AcceptPublishesStateAndContractOnce) {
  FakePhy pd;
  pd.set_request_voltage(20);
  pd.receive_source_caps(0);
  pd.state_changes = 0;
  pd.receive(PD_CNTRL_ACCEPT, 1);
  EXPECT_EQ(pd.state_changes, 1);
  EXPECT_EQ(pd.get_state(), PdState::PD_STATE_TRANSITION);
  EXPECT_FLOAT_EQ(pd.contract().voltage(), 20.0f);
}

TEST(FUSB302BPd, AcceptForSameContractSkipsTransition) {
  FakePhy pd;
  pd.set_request_voltage(20);
  pd.receive_source_caps(0);
  pd.receive(PD_CNTRL_ACCEPT, 1);
  pd.receive(PD_CNTRL_PS_RDY, 2);
  pd.receive_source_caps(3);
  pd.receive(PD_CNTRL_ACCEPT, 4);
  EXPECT_EQ(pd.get_state(), PdState::PD_STATE_EXPLICIT_CONTRACT);
  pd.receive(PD_CNTRL_PS_RDY, 5);
  EXPECT_EQ(pd.get_state(), PdState::PD_STATE_EXPLICIT_CONTRACT);
  EXPECT_FALSE(pd.check_ams_());
}

TEST(FUSB302BPd, PsRdyWithoutAcceptIsIgnored) {
  FakePhy pd;
  pd.receive(PD_CNTRL_PS_RDY, 0);
  EXPECT_EQ(pd.get_state(), PdState::PD_STATE_DISCONNECTED);

  pd.receive_source_caps(1);
  pd.receive(PD_CNTRL_PS_RDY, 2);
  EXPECT_EQ(pd.get_state(), PdState::PD_STATE_DISCONNECTED);
}

TEST(FUSB302BPd, PsRdyAfterRejectIsIgnored) {
  FakePhy pd;
  pd.receive_source_caps(0);
  pd.receive(PD_CNTRL_REJECT, 1);
  EXPECT_FALSE(pd.check_ams_());
  pd.receive(PD_CNTRL_PS_RDY, 2);
  EXPECT_EQ(pd.get_state(), PdState::PD_STATE_DISCONNECTED);
}

TEST(FUSB302BPd, PsRdyAfterSequenceTimeoutIsIgnored) {
  FakePhy pd;
  pd.set_request_voltage(20);
  pd.receive_source_caps(0);
  pd.receive(PD_CNTRL_ACCEPT, 1);
  pd.advance_ms(2001);
  pd.receive(PD_CNTRL_PS_RDY, 2);
  EXPECT_EQ(pd.get_state(), PdState::PD_STATE_TRANSITION);
}

TEST(FUSB302BPd, TransitionTimeoutFallsBackToDefault) {
  FakePhy pd;
  pd.attach();
  pd.set_request_voltage(20);
  pd.receive_source_caps(0);
  pd.receive(PD_CNTRL_ACCEPT, 1);
  pd.check_transition_timeout_();
  EXPECT_EQ(pd.get_state(), PdState::PD_STATE_TRANSITION);
  EXPECT_EQ(pd.transition_timeouts, 0);

  pd.advance_ms(2001);
  pd.state_changes = 0;
  pd.check_transition_timeout_();
  EXPECT_EQ(pd.get_state(), PdState::PD_STATE_DEFAULT_CONTRACT);
  EXPECT_FLOAT_EQ(pd.contract().voltage(), 5.0f);
  EXPECT_FLOAT_EQ(pd.contract().current(), 0.5f);
  EXPECT_EQ(pd.state_changes, 1);
  EXPECT_EQ(pd.transition_timeouts, 1);

  // Reported once, and a late PS_RDY does not bring back the unconfirmed contract
  pd.check_transition_timeout_();
  pd.receive(PD_CNTRL_PS_RDY, 2);
  EXPECT_EQ(pd.transition_timeouts, 1);
  EXPECT_EQ(pd.get_state(), PdState::PD_STATE_DEFAULT_CONTRACT);
}

TEST(FUSB302BPd, RequestVoltageIsClamped) {
  FakePhy pd;
  pd.set_request_voltage(255);
  EXPECT_EQ(pd.get_request_voltage(), MAX_REQUEST_VOLTAGE);
  pd.set_request_voltage(0);
  EXPECT_EQ(pd.get_request_voltage(), MIN_REQUEST_VOLTAGE);
}

TEST(FUSB302BPd, WaitRepeatsRequestAfterSinkRequestTime) {
  FakePhy pd;
  pd.set_request_voltage(20);
  pd.receive_source_caps(0);
  pd.receive(PD_CNTRL_WAIT, 1);
  EXPECT_FALSE(pd.check_ams_());
  pd.check_request_retry_();
  EXPECT_EQ(pd.sent.size(), 1u);
  pd.advance_ms(101);
  pd.check_request_retry_();
  ASSERT_EQ(pd.sent.size(), 2u);
  EXPECT_EQ(pd.sent[1].type, PD_DATA_REQUEST);
  EXPECT_EQ(pd.sent[1].data_objects[0], pd.sent[0].data_objects[0]);
  EXPECT_TRUE(pd.check_ams_());
}

TEST(FUSB302BPd, WaitRetriesStopAtLimit) {
  FakePhy pd;
  pd.attach();
  pd.receive_source_caps(0);
  for (uint8_t i = 1; i <= 10; i++) {
    pd.receive(PD_CNTRL_WAIT, i % 8);
    pd.advance_ms(101);
    pd.check_request_retry_();
  }
  EXPECT_EQ(pd.sent.size(), 11u);
  pd.receive(PD_CNTRL_WAIT, 3);
  EXPECT_FALSE(pd.retry_request_pending_);
  EXPECT_EQ(pd.get_state(), PdState::PD_STATE_PD_TIMEOUT);
  pd.advance_ms(101);
  pd.check_request_retry_();
  EXPECT_EQ(pd.sent.size(), 11u);
}

TEST(FUSB302BPd, FailedRequestEndsNegotiation) {
  FakePhy pd;
  pd.attach();
  pd.set_request_voltage(20);
  pd.send_ok = false;
  pd.receive_source_caps(0);
  EXPECT_EQ(pd.send_attempts, 1);
  EXPECT_FALSE(pd.check_ams_());
  EXPECT_FALSE(pd.retry_request_pending_);
  EXPECT_EQ(pd.get_state(), PdState::PD_STATE_PD_TIMEOUT);
  pd.receive(PD_CNTRL_ACCEPT, 1);
  EXPECT_EQ(pd.get_state(), PdState::PD_STATE_PD_TIMEOUT);
  EXPECT_FLOAT_EQ(pd.contract().voltage(), 5.0f);
}

TEST(FUSB302BPd, FailedRetryIsTriedAgain) {
  FakePhy pd;
  pd.receive_source_caps(0);
  pd.receive(PD_CNTRL_WAIT, 1);
  pd.send_ok = false;
  pd.advance_ms(101);
  pd.check_request_retry_();
  EXPECT_EQ(pd.send_attempts, 2);
  EXPECT_TRUE(pd.retry_request_pending_);
  EXPECT_FALSE(pd.check_ams_());

  pd.send_ok = true;
  pd.check_request_retry_();
  EXPECT_EQ(pd.send_attempts, 2);  // waits another tSinkRequest first
  pd.advance_ms(101);
  pd.check_request_retry_();
  EXPECT_EQ(pd.sent.size(), 2u);
  EXPECT_FALSE(pd.retry_request_pending_);
}

TEST(FUSB302BPd, FailedRetriesStopAtLimit) {
  FakePhy pd;
  pd.attach();
  pd.receive_source_caps(0);
  pd.receive(PD_CNTRL_WAIT, 1);
  pd.send_ok = false;
  for (int i = 0; i < 20 && pd.retry_request_pending_; i++) {
    pd.advance_ms(101);
    pd.check_request_retry_();
  }
  EXPECT_FALSE(pd.retry_request_pending_);
  // The first request, then one attempt for each of the 10 allowed repeats
  EXPECT_EQ(pd.send_attempts, 11);
  EXPECT_EQ(pd.get_state(), PdState::PD_STATE_PD_TIMEOUT);
}

TEST(FUSB302BPd, RejectedFirstRequestEndsNegotiation) {
  FakePhy pd;
  pd.attach();
  pd.set_request_voltage(20);
  pd.receive_source_caps(0);
  pd.receive(PD_CNTRL_REJECT, 1);
  EXPECT_EQ(pd.get_state(), PdState::PD_STATE_PD_TIMEOUT);
  EXPECT_FLOAT_EQ(pd.contract().voltage(), 5.0f);
}

TEST(FUSB302BPd, RejectedRenegotiationKeepsContract) {
  FakePhy pd;
  pd.attach();
  pd.set_request_voltage(20);
  pd.receive_source_caps(0);
  pd.receive(PD_CNTRL_ACCEPT, 1);
  pd.receive(PD_CNTRL_PS_RDY, 2);
  pd.set_request_voltage(9);
  pd.receive_source_caps(3);
  pd.receive(PD_CNTRL_REJECT, 4);
  EXPECT_EQ(pd.get_state(), PdState::PD_STATE_EXPLICIT_CONTRACT);
  EXPECT_FLOAT_EQ(pd.contract().voltage(), 20.0f);
}

TEST(FUSB302BPd, RejectAndWaitOutsideRequestAreIgnored) {
  FakePhy pd;
  pd.attach();
  pd.receive(PD_CNTRL_REJECT, 0);
  pd.receive(PD_CNTRL_WAIT, 1);
  EXPECT_EQ(pd.get_state(), PdState::PD_STATE_DEFAULT_CONTRACT);
  EXPECT_FALSE(pd.retry_request_pending_);
}

TEST(FUSB302BPd, UnsupportedDataMessageIsIgnored) {
  FakePhy pd;
  pd.receive(PD_DATA_REQUEST, 0, {0});
  EXPECT_TRUE(pd.sent.empty());
  EXPECT_EQ(pd.source_caps, 0);
}

TEST(FUSB302BPd, AcceptOutsideSequenceIsIgnored) {
  FakePhy pd;
  pd.set_request_voltage(20);
  pd.receive_source_caps(0);
  pd.receive(PD_CNTRL_REJECT, 1);
  pd.receive(PD_CNTRL_ACCEPT, 2);
  pd.receive(PD_CNTRL_PS_RDY, 3);
  EXPECT_EQ(pd.get_state(), PdState::PD_STATE_DISCONNECTED);
  EXPECT_FALSE(pd.contract().is_set());
}

TEST(FUSB302BPd, GoodCrcMustMatchMessageId) {
  FakePhy pd;
  pd.receive(PD_CNTRL_DR_SWAP, 0);
  ASSERT_EQ(pd.sent.size(), 1u);
  EXPECT_EQ(pd.sent[0].id, 0);

  pd.receive(PD_CNTRL_GOODCRC, 1);
  pd.receive(PD_CNTRL_DR_SWAP, 1);
  ASSERT_EQ(pd.sent.size(), 2u);
  EXPECT_EQ(pd.sent[1].id, 0);

  pd.receive(PD_CNTRL_GOODCRC, 0);
  pd.receive(PD_CNTRL_GOODCRC, 0);  // duplicate
  pd.receive(PD_CNTRL_DR_SWAP, 2);
  ASSERT_EQ(pd.sent.size(), 3u);
  EXPECT_EQ(pd.sent[2].id, 1);
}

TEST(FUSB302BPd, FailedTransmitAdvancesMessageId) {
  FakePhy pd;
  pd.receive_source_caps(0);
  ASSERT_EQ(pd.sent.size(), 1u);
  EXPECT_EQ(pd.sent[0].id, 0);
  pd.handle_transmit_failed_();
  // A late GoodCRC for the failed message must not advance the counter a second time
  pd.receive(PD_CNTRL_GOODCRC, 0);
  pd.receive(PD_CNTRL_DR_SWAP, 1);
  ASSERT_EQ(pd.sent.size(), 2u);
  EXPECT_EQ(pd.sent[1].id, 1);
}

TEST(FUSB302BPd, RetransmissionIsIgnored) {
  FakePhy pd;
  pd.receive_source_caps(0);
  pd.receive_source_caps(0);
  EXPECT_EQ(pd.sent.size(), 1u);
  EXPECT_EQ(pd.source_caps, 1);
}

TEST(FUSB302BPd, SoftResetIsAccepted) {
  FakePhy pd;
  pd.receive(PD_CNTRL_SOFT_RESET, 0);
  ASSERT_EQ(pd.sent.size(), 1u);
  EXPECT_EQ(pd.sent[0].type, PD_CNTRL_ACCEPT);
  EXPECT_EQ(pd.soft_resets, 1);
}

TEST(FUSB302BPd, SoftResetHookRunsWhenAcceptFails) {
  FakePhy pd;
  pd.send_ok = false;
  pd.receive(PD_CNTRL_SOFT_RESET, 0);
  EXPECT_EQ(pd.send_attempts, 1);
  EXPECT_EQ(pd.soft_resets, 1);
}

TEST(FUSB302BPd, SoftResetDuringTransitionFallsBackToDefault) {
  FakePhy pd;
  pd.set_request_voltage(20);
  pd.receive_source_caps(0);
  pd.receive(PD_CNTRL_ACCEPT, 1);
  pd.state_changes = 0;
  pd.receive(PD_CNTRL_SOFT_RESET, 0);
  EXPECT_EQ(pd.state_changes, 1);
  EXPECT_EQ(pd.get_state(), PdState::PD_STATE_DEFAULT_CONTRACT);
  EXPECT_FLOAT_EQ(pd.contract().voltage(), 5.0f);
  EXPECT_FLOAT_EQ(pd.contract().current(), 0.5f);
}

TEST(FUSB302BPd, UnknownControlMessageIsRejected) {
  FakePhy pd;
  pd.receive(PD_CNTRL_DR_SWAP, 0);
  ASSERT_EQ(pd.sent.size(), 1u);
  EXPECT_EQ(pd.sent[0].type, PD_CNTRL_REJECT);
}

TEST(FUSB302BPd, SinkCapAt5VHasOneObject) {
  FakePhy pd;
  pd.receive(PD_CNTRL_GET_SINK_CAP, 0);
  ASSERT_EQ(pd.sent.size(), 1u);
  EXPECT_EQ(pd.sent[0].type, PD_DATA_SINK_CAP);
  ASSERT_EQ(pd.sent[0].num_of_obj, 1);
  const uint32_t pdo = pd.sent[0].data_objects[0];
  EXPECT_EQ((pdo >> 10) & 0x3FF, 100u);
  EXPECT_EQ(pdo & 0x3FF, 300u);
  EXPECT_FALSE(pdo & (1u << 28));
  EXPECT_TRUE(pdo & (1u << 26));
}

TEST(FUSB302BPd, SinkCapListsRequestedVoltage) {
  FakePhy pd;
  pd.set_request_voltage(20);
  pd.receive(PD_CNTRL_GET_SINK_CAP, 0);
  ASSERT_EQ(pd.sent.size(), 1u);
  ASSERT_EQ(pd.sent[0].num_of_obj, 2);
  EXPECT_TRUE(pd.sent[0].data_objects[0] & (1u << 28));
  const uint32_t pdo = pd.sent[0].data_objects[1];
  EXPECT_EQ(pdo >> 30, 0u);
  EXPECT_EQ((pdo >> 10) & 0x3FF, 400u);
  EXPECT_EQ(pdo & 0x3FF, 300u);
  EXPECT_EQ(pdo >> 25, 0u);  // capability flags are only valid in the first object
}

TEST(FUSB302BPd, SinkCapForOutOfRangeVoltageStaysValid) {
  FakePhy pd;
  pd.set_request_voltage(255);
  pd.receive(PD_CNTRL_GET_SINK_CAP, 0);
  ASSERT_EQ(pd.sent.size(), 1u);
  ASSERT_EQ(pd.sent[0].num_of_obj, 2);
  const uint32_t pdo = pd.sent[0].data_objects[1];
  EXPECT_EQ((pdo >> 10) & 0x3FF, 400u);
  EXPECT_EQ(pdo >> 20, 0u);  // nothing spills into the peak current or flag bits
}

}  // namespace esphome::fusb302b::testing
