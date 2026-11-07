#include "api_buffer.h"
#include <new>

namespace esphome::api {

bool APIBuffer::grow_(size_t n) {
  // nothrow (no zero-fill) so OOM is reportable; plain new aborts instead
  // (NEW_OOM_ABORT on ESP8266 Arduino, exception stub on ESP-IDF).
  // RAMAllocator is no fit here: unique_ptr needs delete[]-compatible memory.
  std::unique_ptr<uint8_t[]> new_data(new (std::nothrow) uint8_t[n]);
  if (new_data == nullptr)
    return false;
  if (this->size_)
    std::memcpy(new_data.get(), this->data_.get(), this->size_);
  this->data_ = std::move(new_data);
  this->capacity_ = n;
  return true;
}

}  // namespace esphome::api
