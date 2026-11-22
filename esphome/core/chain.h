#pragma once

#include <cstdint>

namespace esphome {

/** One pointer per node that holds the next node of a chain, or on the last node the chain's owner.
 *
 * The owner is tagged in the low bit, so it needs an alignment of at least 2. A chain may also end in nullptr,
 * with no owner.
 */
template<typename Next, typename Owner> class ChainLink {
 public:
  bool is_owner() const { return reinterpret_cast<uintptr_t>(this->ptr_) & OWNER_TAG; }
  /// The next node, or nullptr on the last one.
  Next *next() const { return this->is_owner() ? nullptr : this->ptr_; }
  /// Only valid when is_owner().
  Owner *owner() const { return reinterpret_cast<Owner *>(reinterpret_cast<char *>(this->ptr_) - OWNER_TAG); }
  /// Where the next node goes, for appending without a branch; only valid when !is_owner().
  Next **next_slot() {
    static_assert(alignof(Next) > OWNER_TAG, "a node address must leave the owner tag bit free");
    return &this->ptr_;
  }
  void set_next(Next *next) {
    static_assert(alignof(Next) > OWNER_TAG, "a node address must leave the owner tag bit free");
    this->ptr_ = next;
  }
  /// Makes this the last node.
  void set_owner(Owner *owner) { this->ptr_ = tagged(owner); }
  /// The value set_owner() stores, for writing through next_slot(). The owner is never null.
  static Next *tagged(Owner *owner) {
    static_assert(alignof(Owner) > OWNER_TAG, "the owner tag needs a free low bit");
    char *base = reinterpret_cast<char *>(owner);
    // The analyzer cannot see that an owner read back through owner() is non-null
    // NOLINTNEXTLINE(clang-analyzer-core.NullPointerArithm)
    return reinterpret_cast<Next *>(base + OWNER_TAG);
  }
  /// Link to next, or to owner when next is nullptr.
  void set(Next *next, Owner *owner) {
    if (next != nullptr) {
      this->set_next(next);
    } else {
      this->set_owner(owner);
    }
  }

 protected:
  static constexpr uintptr_t OWNER_TAG = 1;
  // A pointer, not a uintptr_t, so no integer is ever cast back to a pointer
  Next *ptr_{nullptr};
};

/// Base for a node of a ChainLink chain: holds the link and finds the chain's owner.
template<typename Node, typename Owner> class ChainNode {
  // Only Node can be built on this base
  friend Node;
  ChainNode() = default;

 protected:
  /// Only for a chain that ends in its owner, never in nullptr.
  Node *chain_last_() {
    Node *node = static_cast<Node *>(this);
    while (!node->link_.is_owner())
      node = node->link_.next();
    return node;
  }
  /// Walks to the end of the chain on every call.
  Owner *chain_owner_() { return this->chain_last_()->link_.owner(); }

  /// The next node, or the owner on the last node.
  ChainLink<Node, Owner> link_;
};

}  // namespace esphome
