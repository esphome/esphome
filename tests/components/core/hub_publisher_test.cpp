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

  bool claim(uint32_t id) { return this->claim_publisher_ != nullptr && this->claim_publisher_(id); }

  HUB_PUBLISHER_RETURNING(bool, claim, uint32_t)
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

// The shape cg.HubFunction emits for a bool function (it names these esphome__arg_id and esphome__found)
static bool claim_seven(uint32_t id) {
  bool found = false;
  (found |= id == 7);
  return found;
}

TEST(HubPublisher, BoolPublisherReturnsWhetherAnyItemClaimed) {
  PublishingHub hub;
  EXPECT_FALSE(hub.claim(7));
  hub.set_claim_publisher(claim_seven);
  EXPECT_TRUE(hub.claim(7));
  EXPECT_FALSE(hub.claim(8));
}

}  // namespace esphome::testing
