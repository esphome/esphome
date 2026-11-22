#pragma once

#include <cstdint>

namespace esphome {

/** One pointer that holds the next node of a chain, or on the last node the chain's owner.
 *
 * Replaces a next pointer plus an owner pointer that only the last node needs, saving a pointer per node.
 * The owner is tagged in the low bit, so it needs an alignment of at least 2; any class with a vtable or a
 * pointer member has it. A chain may also end in nullptr, with no owner.
 */
template<typename Next, typename Owner> class ChainLink {
 public:
  bool is_owner() const { return reinterpret_cast<uintptr_t>(this->ptr_) & OWNER_TAG; }
  /// The next node, or nullptr on the last one.
  Next *next() const { return this->is_owner() ? nullptr : this->ptr_; }
  /// Only valid when is_owner().
  Owner *owner() const { return reinterpret_cast<Owner *>(reinterpret_cast<char *>(this->ptr_) - OWNER_TAG); }
  /// Where the next node goes, for appending without a branch; only valid when !is_owner().
  Next **next_slot() { return &this->ptr_; }
  void set_next(Next *next) { this->ptr_ = next; }
  /// Makes this the last node.
  void set_owner(Owner *owner) {
    static_assert(alignof(Owner) > OWNER_TAG, "the owner tag needs a free low bit");
    this->ptr_ = reinterpret_cast<Next *>(reinterpret_cast<char *>(owner) + OWNER_TAG);
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
 protected:
  Node *chain_last_() {
    Node *node = static_cast<Node *>(this);
    while (Node *next = node->link_.next())
      node = next;
    return node;
  }
  /// Walks to the end of the chain on every call.
  Owner *chain_owner_() { return this->chain_last_()->link_.owner(); }

  /// The next node, or the owner on the last node.
  ChainLink<Node, Owner> link_;
};

}  // namespace esphome
