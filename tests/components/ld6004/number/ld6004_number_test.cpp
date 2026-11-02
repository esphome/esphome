#ifdef USE_HOST
#include "../common.h"

#include "esphome/components/number/number.h"

namespace esphome::ld6004::testing {

namespace {

// The settings are uint32 on the wire, but a float holds every whole number only up to 2**24.
constexpr float MAX_WHOLE_FLOAT = 16777215;

}  // namespace

class LD6004NumberTest : public LD6004Test {
 protected:
  void SetUp() override {
    for (auto *n : {&this->dwell_lifetime_, &this->output_interval_}) {
      n->traits.set_min_value(0);
      n->traits.set_max_value(MAX_WHOLE_FLOAT);
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
    EXPECT_EQ(this->uart_.tx, command(this->next_command_id_++, c.type, u32_payload(c.value)));
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
  this->fill_command_queue_([this] { return this->sut_.set_number_value(&this->output_interval_, 100); });
  this->dwell_lifetime_.make_call().set_value(60).perform();
  EXPECT_FLOAT_EQ(this->dwell_lifetime_.state, 30.0f);
}

}  // namespace esphome::ld6004::testing

#endif  // USE_HOST
