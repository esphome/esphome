#include <gtest/gtest.h>

#include "esphome/core/helpers.h"

namespace esphome::testing {

class PublishingHub {
 public:
  bool publish(int value) {
    if (this->value_publisher_ == nullptr)
      return false;
    this->value_publisher_(value);
    return true;
  }

  HUB_PUBLISHER(value, int)
};

static int published = 0;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static void record(int value) { published = value; }

TEST(HubPublisher, StartsUnsetAndCallsTheFunctionOnceSet) {
  PublishingHub hub;
  published = 0;
  EXPECT_FALSE(hub.publish(5));
  hub.set_value_publisher(record);
  EXPECT_TRUE(hub.publish(7));
  EXPECT_EQ(published, 7);
}

}  // namespace esphome::testing
