#include "key_provider.h"

namespace esphome {
namespace key_provider {

void KeyProvider::send_key_(uint8_t key) { this->key_callback_.call(key); }

}  // namespace key_provider
}  // namespace esphome
