#include <gtest/gtest.h>

#include "esphome/core/helpers.h"

namespace esphome::core::testing {
namespace {

struct Owner {
  int id;
};
struct Node {
  ChainLink<Node, Owner> link;
};

}  // namespace

TEST(ChainLink, HoldsTheNextNode) {
  Owner owner{1};
  Node first;
  Node second;
  first.link.set(&second, &owner);
  EXPECT_FALSE(first.link.is_owner());
  EXPECT_EQ(first.link.next(), &second);
}

TEST(ChainLink, HoldsTheOwnerOnTheLastNode) {
  Owner owner{42};
  Node last;
  last.link.set(nullptr, &owner);
  EXPECT_TRUE(last.link.is_owner());
  EXPECT_EQ(last.link.owner(), &owner);
  EXPECT_EQ(last.link.owner()->id, 42);
  EXPECT_EQ(last.link.next(), nullptr);
}

TEST(ChainLink, RelinkingTheLastNodeMakesItPointOn) {
  Owner owner{1};
  Node first;
  Node second;
  first.link.set(nullptr, &owner);
  first.link.set(&second, &owner);
  EXPECT_FALSE(first.link.is_owner());
  EXPECT_EQ(first.link.next(), &second);
}

TEST(ChainLink, IsOnePointer) { EXPECT_EQ(sizeof(ChainLink<Node, Owner>), sizeof(void *)); }

}  // namespace esphome::core::testing
