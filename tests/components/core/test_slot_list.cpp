#include <gtest/gtest.h>
#include "esphome/core/helpers.h"

namespace esphome::testing {

struct Listener {
  int id;
};

class Hub {
 public:
  SLOT_LIST(Listener, listeners_, register_listener, 2)
  // The macro ends in a public section.
  size_t size() const { return this->listeners_.size(); }
  Listener *at(size_t i) const { return this->listeners_[i]; }
};

// A count of 0 is what code generation emits for a hub nothing registered into
class EmptyHub {
 public:
  SLOT_LIST(Listener, listeners_, register_listener, 0)
  size_t walk() const {
    size_t n = 0;
    for (auto *listener : this->listeners_)
      n += listener->id;
    return n + this->listeners_.size();
  }
  uint32_t value{0};
};

TEST(SlotListTest, KeepsRegistrationOrder) {
  Hub hub;
  Listener a{1}, b{2};
  hub.register_listener(&a);
  hub.register_listener(&b);
  ASSERT_EQ(hub.size(), 2u);
  EXPECT_EQ(hub.at(0), &a);
  EXPECT_EQ(hub.at(1), &b);
}

TEST(SlotListTest, DropsRegistrationsPastCapacity) {
  Hub hub;
  Listener a{1}, b{2}, c{3};
  hub.register_listener(&a);
  hub.register_listener(&b);
  hub.register_listener(&c);
  ASSERT_EQ(hub.size(), 2u);
  EXPECT_EQ(hub.at(1), &b);
}

TEST(SlotListTest, StorageIsTheCodegenCountOfPointers) { EXPECT_EQ(sizeof(Hub), sizeof(StaticVector<Listener *, 2>)); }

TEST(SlotListTest, ZeroCountWalksNothingAndTakesNoSpace) {
  EmptyHub hub;
  Listener a{1};
  hub.register_listener(&a);
  EXPECT_EQ(hub.walk(), 0u);
  EXPECT_EQ(sizeof(EmptyHub), sizeof(uint32_t));
}

}  // namespace esphome::testing
