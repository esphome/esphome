#pragma once

#include <functional>
#include <vector>

namespace esphome::camera {

/// @brief Class for registering cleanup callbacks.
/// DeleteCallback is inherited by other classes to register
/// actions that run just before the base object is deleted.
/// Callbacks must not access members of other derived classes.
template<typename T> class DeleteCallback {
 public:
  using Callback = std::function<void(T *)>;
  /// Register a callback executed when the object gets deleted.
  void on_delete(Callback callback) { callbacks_.push_back(std::move(callback)); }
  virtual ~DeleteCallback() {
    for (Callback &callback : callbacks_) {
      if (callback) {
        callback(static_cast<T *>(this));
      }
    }
  }

 protected:
  std::vector<Callback> callbacks_;
};

}  // namespace esphome::camera
