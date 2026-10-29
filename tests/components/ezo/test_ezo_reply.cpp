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
      out[0] = 1;
      memcpy(out + 1, this->reply.data(), std::min(this->reply.size(), out_size - 1));
      this->read_sizes.push_back(out_size);
    }
    return i2c::ERROR_OK;
  }

  std::string reply;
  std::vector<size_t> read_sizes;
};

static std::vector<std::string> run_custom(const std::string &reply) {
  EzoReplyBus bus;
  bus.reply = reply;
  EZOSensor ezo;
  ezo.set_i2c_bus(&bus);
  ezo.set_i2c_address(99);
  std::vector<std::string> replies;
  ezo.add_custom_callback([&replies](std::string x) { replies.push_back(std::move(x)); });
  ezo.send_custom("Status");
  // The reply is read once its delay has passed on the real clock.
  const uint32_t start = millis();
  while (replies.empty() && millis() - start < 2000) {
    LoopBlockingGuard guard{nullptr, nullptr, millis()};
    ezo.loop();
    delay(10);
  }
  return replies;
}

// EZO-RGB's largest reply is 52 characters; it arrives whole even with no terminator in the read.
TEST(EzoReplyTest, LargestReplyIsDeliveredWhole) {
  const std::string reply(52, 'x');
  EXPECT_EQ(run_custom(reply), (std::vector<std::string>{reply}));
}

TEST(EzoReplyTest, ShortReplyIsUnchanged) {
  EXPECT_EQ(run_custom("?Status,P,5.038"), (std::vector<std::string>{"?Status,P,5.038"}));
}

}  // namespace esphome::ezo::testing
