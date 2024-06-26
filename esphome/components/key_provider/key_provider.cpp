#include "key_provider.h"

namespace esphome {
namespace key_provider {

void KeyProvider::add_on_key_callback(std::function<void(uint8_t)> &&callback) {
  this->key_callback_.add(std::move(callback));
}

void KeyProvider::send_key_(uint8_t key) { this->key_callback_.call(key); }

}  // namespace key_provider
}  // namespace esphome
