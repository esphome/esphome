#include "hid_media_keys.h"
#include "esphome/core/log.h"
#include <cstring>

namespace esphome {
namespace hid {

static const char *const TAG = "hid";

void MediaKeys::control(std::vector<uint16_t> &&keys) {
  media_keys_ = 0;
  if (!keys.empty()) {
    media_keys_ = keys[0];
  }
  if (keys.size() > 1) {
    keys.resize(1);
  }
  report();
  // TODO public, callbacks???
  *this->keys_ = std::move(keys);
}

}  // namespace hid
}  // namespace esphome
