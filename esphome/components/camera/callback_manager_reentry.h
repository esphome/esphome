#pragma once

#include "reentry.h"

#include <functional>

namespace esphome::camera {

/// Manages a list of callbacks with reentry control.
/// Each callback receives the provided arguments (Ts...) and a reference to a Reentry object.
/// The Reentry object allows a callback to pause the sequence (by calling set_not_done()), in
/// which case the manager will stop iteration and return false so the caller can retry later.
template<typename... Ts> class CallbackManagerReentry {
 public:
  using Callback = std::function<void(Ts..., Reentry &)>;
  /// Add a callback to the list.
  void add(Callback &&callback) { this->callbacks_.push_back(std::move(callback)); }
  /// Invoke callbacks until completion or a callback requests reentry.
  /// Returns true if all callbacks completed, false if additional calls are required.
  bool call(Ts... args) {
    while (this->index_ < this->callbacks_.size()) {
      this->reentry_.reset();
      this->callbacks_[this->index_](args..., this->reentry_);
      if (this->reentry_.is_not_done())
        return false;

      ++this->index_;
    }

    this->index_ = 0;
    return true;
  }
  /// Equivalent to call().
  bool operator()(Ts... args) { return call(args...); }

 protected:
  size_t index_{};
  std::vector<Callback> callbacks_;
  Reentry reentry_;
};

}  // namespace esphome::camera
