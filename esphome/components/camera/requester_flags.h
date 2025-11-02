#pragma once

#include "camera.h"

namespace esphome::camera {

/// RequesterFlags represents a set of CameraRequester flags with range-for support.
/// Can be used in range-based for loops to iterate active requesters.
class RequesterFlags {
 public:
  /// Initialize with no flags set.
  RequesterFlags() : flags_(0) {}
  /// Add a single requester flag.
  void add(CameraRequester requester) { flags_ |= (1 << requester); }
  /// Add all possible requester flags.
  void add_all() { flags_ = (1 << REQUESTER_COUNT) - 1; }
  /// Check if a specific requester flag is set.
  bool has(CameraRequester requester) const { return flags_ & (1 << requester); }
  /// Check if all requester flags are set.
  bool has_all() const { return flags_ == (1 << REQUESTER_COUNT) - 1; }
  /// Clear a specific requester flag.
  void clear(CameraRequester requester) { flags_ &= ~(1 << requester); }
  /// Clear all flags.
  void clear() { flags_ = 0; }
  /// Check if any flag is set.
  explicit operator bool() const { return flags_ != 0; }
  /// Convert set flags to a comma-separated string.
  const char *to_string() {
    this->conf_.clear();
    for (CameraRequester r : *this) {
      if (!this->conf_.empty())
        this->conf_ += ", ";
      this->conf_ += esphome::camera::to_string(r);
    }
    return this->conf_.c_str();
  }
  /// Return union of this and another RequesterFlags.
  RequesterFlags operator|(const RequesterFlags &rhs) const {
    RequesterFlags result;
    result.flags_ = this->flags_ | rhs.flags_;
    return result;
  }
  /// Return difference between this and another RequesterFlags.
  RequesterFlags operator-(const RequesterFlags &rhs) const {
    RequesterFlags result;
    result.flags_ = this->flags_ & ~rhs.flags_;
    return result;
  }
  /// Remove flags from this set that exist in another RequesterFlags.
  RequesterFlags &operator-=(const RequesterFlags &rhs) {
    flags_ &= ~rhs.flags_;
    return *this;
  }

  class Iterator {
   public:
    explicit Iterator(uint8_t flags, uint8_t pos = 0) : flags_(flags), pos_(pos) { advance_to_next(); }

    CameraRequester operator*() const { return static_cast<CameraRequester>(pos_); }

    Iterator &operator++() {
      ++pos_;
      advance_to_next();
      return *this;
    }

    bool operator!=(const Iterator &other) const { return pos_ != other.pos_ || flags_ != other.flags_; }

   protected:
    uint8_t flags_;
    uint8_t pos_;
    void advance_to_next() {
      while (pos_ < REQUESTER_COUNT && !(flags_ & (1 << pos_)))
        ++pos_;
    }
  };

  Iterator begin() const { return Iterator(flags_, 0); }
  Iterator end() const { return Iterator(flags_, REQUESTER_COUNT); }

 protected:
  uint8_t flags_;
  std::string conf_;
};

}  // namespace esphome::camera
