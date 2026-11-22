#include <gtest/gtest.h>

#include "esphome/core/chain.h"

namespace esphome::core::testing {
namespace {

struct Owner {
  int id;
};
struct Node : ChainNode<Node, Owner> {
  using ChainNode::chain_last_;
  using ChainNode::chain_owner_;
  using ChainNode::link_;
};

}  // namespace

static_assert(sizeof(ChainLink<Node, Owner>) == sizeof(void *));

TEST(ChainLink, HoldsTheNextNode) {
  Node first;
  Node second;
  first.link_.set_next(&second);
  EXPECT_FALSE(first.link_.is_owner());
  EXPECT_EQ(first.link_.next(), &second);
}

TEST(ChainLink, HoldsTheOwnerOnTheLastNode) {
  Owner owner{42};
  Node last;
  last.link_.set_owner(&owner);
  EXPECT_TRUE(last.link_.is_owner());
  EXPECT_EQ(last.link_.owner(), &owner);
  EXPECT_EQ(last.link_.owner()->id, 42);
  EXPECT_EQ(last.link_.next(), nullptr);
}

TEST(ChainLink, SetPicksNextOrOwner) {
  Owner owner{1};
  Node first;
  Node second;
  first.link_.set(&second, &owner);
  EXPECT_EQ(first.link_.next(), &second);
  first.link_.set(nullptr, &owner);
  EXPECT_EQ(first.link_.owner(), &owner);
}

TEST(ChainLink, AnUnlinkedNodeEndsTheChainWithoutAnOwner) {
  Node node;
  EXPECT_FALSE(node.link_.is_owner());
  EXPECT_EQ(node.link_.next(), nullptr);
}

TEST(ChainNode, FindsTheLastNodeAndItsOwner) {
  Owner owner{7};
  Node a;
  Node b;
  Node c;
  a.link_.set_next(&b);
  b.link_.set_next(&c);
  c.link_.set_owner(&owner);
  EXPECT_EQ(a.chain_last_(), &c);
  EXPECT_EQ(b.chain_owner_(), &owner);
  EXPECT_EQ(c.chain_owner_(), &owner);
}

}  // namespace esphome::core::testing
