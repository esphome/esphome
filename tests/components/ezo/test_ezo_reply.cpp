#include <gtest/gtest.h>

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

#include "esphome/components/ezo/ezo.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"

namespace esphome::ezo::testing {

// Answers every read with a success code and `reply`, with no terminator when the reply fills the read.
class EzoReplyBus final : public i2c::I2CBus {
 public:
  i2c::ErrorCode write_readv(uint8_t address, const uint8_t *data, size_t size, uint8_t *out,
                             size_t out_size) override {
    if (out_size != 0) {
      memset(out, 0, out_size);
      out[0] = this->busy ? 254 : 1;
      memcpy(out + 1, this->reply.data(), std::min(this->reply.size(), out_size - 1));
      this->read_sizes.push_back(out_size);
    }
    return i2c::ERROR_OK;
  }

  std::string reply;
  std::vector<size_t> read_sizes;
  bool busy{false};
};

// A sensor on EzoReplyBus that collects custom replies; queue a command with send_custom().
struct CustomRig {
  explicit CustomRig(const std::string &reply) {
    this->bus.reply = reply;
    this->ezo.set_i2c_bus(&this->bus);
    this->ezo.set_i2c_address(99);
    this->ezo.add_custom_callback([this](const std::string &x) { this->replies.push_back(x); });
  }
  void loop_at(uint32_t now) {
    LoopBlockingGuard guard{nullptr, nullptr, now};
    this->ezo.loop();
  }

  EzoReplyBus bus;
  EZOSensor ezo;
  std::vector<std::string> replies;
};

static std::vector<std::string> run_custom(const std::string &reply) {
  CustomRig rig(reply);
  rig.ezo.send_custom("Status");
  // The reply is read once its delay has passed on the real clock.
  const uint32_t start = millis();
  while (rig.replies.empty() && millis() - start < 2000) {
    rig.loop_at(millis());
    delay(10);
  }
  return rig.replies;
}

// EZO-RGB's largest reply is 52 characters; it arrives whole even with no terminator in the read.
TEST(EzoReplyTest, LargestReplyIsDeliveredWhole) {
  const std::string reply(52, 'x');
  EXPECT_EQ(run_custom(reply), (std::vector<std::string>{reply}));
}

TEST(EzoReplyTest, ShortReplyIsUnchanged) {
  EXPECT_EQ(run_custom("?Status,P,5.038"), (std::vector<std::string>{"?Status,P,5.038"}));
}

// A device that answers "still busy" for longer than the 16-bit reply window is still read every loop,
// and its reply arrives as soon as it is ready.
TEST(EzoReplyTest, LongBusySpellKeepsPolling) {
  CustomRig rig("?Status,P,5.038");
  rig.ezo.send_custom("Status");
  static constexpr uint32_t READY_AT = 40000;
  for (uint32_t now = 1000; now < READY_AT + 1000 && rig.replies.empty(); now += 100) {
    rig.bus.busy = now < READY_AT;
    rig.loop_at(now);
  }
  EXPECT_EQ(rig.replies, (std::vector<std::string>{"?Status,P,5.038"}));
}

// A reply due just past a 16-bit wrap of the clock is not read early.
TEST(EzoReplyTest, ReplyDueAcrossSixteenBitWrap) {
  CustomRig rig("?Status,P,5.038");
  rig.ezo.send_custom("Status");
  rig.loop_at(65300);  // sent; due at 65600, stored as 64
  rig.loop_at(65400);
  EXPECT_TRUE(rig.bus.read_sizes.empty());
  rig.loop_at(65600);
  EXPECT_EQ(rig.replies, (std::vector<std::string>{"?Status,P,5.038"}));
}

}  // namespace esphome::ezo::testing
