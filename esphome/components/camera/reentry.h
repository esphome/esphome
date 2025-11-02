#pragma once

namespace esphome::camera {

/// Helper class used by CallbackManagerReentry to control reentry of callbacks.
/// The Reentry object is reset before each callback execution. If a callback
/// calls set_not_done(), the CallbackManagerReentry will stop iteration and wait for
/// another call() invocation to resume from the same callback.
class Reentry {
 public:
  /// Mark this reentry as not done; iteration should pause here.
  void set_not_done() { not_done_ = true; }
  /// Return true if this reentry is marked as not done.
  bool is_not_done() const { return not_done_; }
  /// Reset reentry state to done.
  void reset() { not_done_ = false; }

 protected:
  bool not_done_{};
};

}  // namespace esphome::camera
