#include <gtest/gtest.h>

#include "esphome/components/mk2pvrouter/mk2pvrouter.h"

namespace esphome::mk2pvrouter {

namespace {
class TestListener final : public Mk2PVRouterListener {
 public:
  explicit TestListener(const char *tag) : Mk2PVRouterListener(tag) {}
  void publish_val(const char *val) override {}
};
}  // namespace

TEST(Mk2PVRouterListenerTest, GetTagReturnsConstructorTag) {
  TestListener listener("P1");
  EXPECT_STREQ(listener.get_tag(), "P1");
}

}  // namespace esphome::mk2pvrouter
