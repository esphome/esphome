#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/component.h"

namespace esphome::key_provider {

/// interface for components that provide keypresses
class KeyProvider {
 public:
  template<typename F> void add_on_key_callback(F &&callback) { this->key_callback_.add(std::forward<F>(callback)); }

 protected:
  void send_key_(uint8_t key);

  CallbackManager<void(uint8_t)> key_callback_{};
};

}  // namespace esphome::key_provider
