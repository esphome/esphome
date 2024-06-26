#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/component.h"

namespace esphome {
namespace key_provider {

/// interface for components that provide keypresses
class KeyProvider {
 public:
  void add_on_key_callback(std::function<void(uint8_t)> &&callback);

 protected:
  void send_key_(uint8_t key);

  CallbackManager<void(uint8_t)> key_callback_{};
};

}  // namespace key_provider
}  // namespace esphome
