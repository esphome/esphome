#ifdef USE_HOST
#include "../common.h"

#include "esphome/components/number/number.h"

namespace esphome::ld6004::testing {

namespace {

constexpr uint16_t TYPE_SET_DWELL_LIFETIME = 0x0206;
constexpr uint16_t TYPE_SET_OUTPUT_INTERVAL = 0x0207;
constexpr uint16_t TYPE_REPORT_DWELL_LIFETIME = 0x0A17;

}  // namespace

class LD6004NumberTest : public LD6004Test {
 protected:
  void SetUp() override {
    for (auto *n : {&this->dwell_lifetime_, &this->output_interval_}) {
      n->traits.set_min_value(0);
      n->traits.set_max_value(16777215);
      n->traits.set_step(1);
    }
    this->sut_.set_dwell_lifetime_number(&this->dwell_lifetime_);
    this->sut_.set_output_interval_number(&this->output_interval_);
    LD6004Test::SetUp();
    // Each test starts with an idle queue
    this->drain_commands_();
  }

  TestNumber dwell_lifetime_;
  TestNumber output_interval_;
};

TEST_F(LD6004NumberTest, WholeValuesAreWrittenAndPublished) {
  const struct {
    TestNumber *number;
    uint16_t type;
    uint32_t value;
  } cases[] = {{&this->dwell_lifetime_, TYPE_SET_DWELL_LIFETIME, 600},
               {&this->output_interval_, TYPE_SET_OUTPUT_INTERVAL, 250}};
  for (const auto &c : cases) {
    ASSERT_TRUE(this->uart_.tx.empty());
    c.number->make_call().set_value(c.value).perform();
    ASSERT_EQ(this->uart_.tx.size(), ld600x::TF_PAYLOAD_OFFSET + 4 + 1);
    EXPECT_EQ(encode_uint16(this->uart_.tx[5], this->uart_.tx[6]), c.type);
    EXPECT_EQ(encode_value_le<uint32_t>(&this->uart_.tx[ld600x::TF_PAYLOAD_OFFSET]), c.value);
    EXPECT_FLOAT_EQ(c.number->state, static_cast<float>(c.value));
    this->uart_.tx.clear();
    this->feed_(ack(c.type));
  }
}

// NumberCall does not round to the step, so the hub rejects a fraction and the number keeps the module's value
TEST_F(LD6004NumberTest, FractionalValueIsNotWrittenOrPublished) {
  this->feed_(tf_frame(TYPE_REPORT_DWELL_LIFETIME, u32_payload(30)));
  ASSERT_FLOAT_EQ(this->dwell_lifetime_.state, 30.0f);
  this->uart_.tx.clear();
  this->dwell_lifetime_.make_call().set_value(12.5f).perform();
  EXPECT_TRUE(this->uart_.tx.empty());
  EXPECT_FLOAT_EQ(this->dwell_lifetime_.state, 30.0f);
}

TEST_F(LD6004NumberTest, ValueIsNotPublishedWhenQueueFull) {
  this->feed_(tf_frame(TYPE_REPORT_DWELL_LIFETIME, u32_payload(30)));
  for (int i = 0; i < 32; i++)
    this->output_interval_.make_call().set_value(100).perform();
  this->dwell_lifetime_.make_call().set_value(60).perform();
  EXPECT_FLOAT_EQ(this->dwell_lifetime_.state, 30.0f);
}

}  // namespace esphome::ld6004::testing

#endif  // USE_HOST
