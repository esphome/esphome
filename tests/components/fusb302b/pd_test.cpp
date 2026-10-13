#include <gtest/gtest.h>
#include <vector>

#include "esphome/components/fusb302b/pd.h"

namespace esphome::fusb302b::testing {

static constexpr uint32_t fixed_pdo(uint16_t mv, uint16_t ma) { return ((mv / 50u) << 10) | (ma / 10u); }

class FakePhy : public PowerDelivery {
 public:
  using PowerDelivery::contract_;
  using PowerDelivery::handle_message_;
  using PowerDelivery::reset_protocol_;

  std::vector<PdMsg> sent;
  int soft_resets{0};

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

 protected:
  bool send_message(const PdMsg &msg) override {
    this->sent.push_back(msg);
    return true;
  }
  void on_state_changed() override {}
  void on_soft_reset_received() override { this->soft_resets++; }
};

TEST(FUSB302BPd, RequestsHighestVoltageNotAboveLimit) {
  FakePhy pd;
  pd.set_request_voltage(18);
  pd.receive_source_caps(0);
  ASSERT_EQ(pd.sent.size(), 1u);
  EXPECT_EQ(pd.sent[0].type, PD_DATA_REQUEST);
  EXPECT_EQ(pd.sent[0].data_objects[0] >> 28, 3u);  // 15 V is object 3
  EXPECT_EQ(pd.sent[0].data_objects[0] & 0x3FF, 300u);
}

TEST(FUSB302BPd, AcceptThenPsRdyReachesExplicitContract) {
  FakePhy pd;
  pd.set_request_voltage(20);
  pd.receive_source_caps(0);
  pd.receive(PD_CNTRL_ACCEPT, 1);
  EXPECT_EQ(pd.get_state(), PdState::PD_STATE_TRANSITION);
  pd.receive(PD_CNTRL_PS_RDY, 2);
  EXPECT_EQ(pd.get_state(), PdState::PD_STATE_EXPLICIT_CONTRACT);
  PackedContract contract{pd.contract_};
  EXPECT_FLOAT_EQ(contract.voltage(), 20.0f);
  EXPECT_FLOAT_EQ(contract.current(), 2.25f);
}

TEST(FUSB302BPd, RetransmissionIsIgnored) {
  FakePhy pd;
  pd.receive_source_caps(0);
  pd.receive_source_caps(0);
  EXPECT_EQ(pd.sent.size(), 1u);
}

TEST(FUSB302BPd, SoftResetIsAccepted) {
  FakePhy pd;
  pd.receive(PD_CNTRL_SOFT_RESET, 0);
  ASSERT_EQ(pd.sent.size(), 1u);
  EXPECT_EQ(pd.sent[0].type, PD_CNTRL_ACCEPT);
  EXPECT_EQ(pd.soft_resets, 1);
}

TEST(FUSB302BPd, GetSinkCapIsAnswered) {
  FakePhy pd;
  pd.receive(PD_CNTRL_GET_SINK_CAP, 0);
  ASSERT_EQ(pd.sent.size(), 1u);
  EXPECT_EQ(pd.sent[0].type, PD_DATA_SINK_CAP);
  EXPECT_EQ(pd.sent[0].num_of_obj, 1);
}

}  // namespace esphome::fusb302b::testing
