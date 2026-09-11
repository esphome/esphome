#include "api_buffer.h"
#include <new>

namespace esphome::api {

bool APIBuffer::grow_(size_t n, size_t drop) {
  if (n > UINT16_MAX)
    return false;
  // nothrow (no zero-fill) so OOM is reportable; plain new aborts instead
  // (NEW_OOM_ABORT on ESP8266 Arduino, exception stub on ESP-IDF).
  // RAMAllocator is no fit here: unique_ptr needs delete[]-compatible memory.
  std::unique_ptr<uint8_t[]> new_data(new (std::nothrow) uint8_t[n]);
  if (new_data == nullptr)
    return false;
  const uint16_t live = this->size_ - static_cast<uint16_t>(drop);
  if (live)
    std::memcpy(new_data.get(), this->data_.get() + drop, live);
  this->data_ = std::move(new_data);
  this->capacity_ = static_cast<uint16_t>(n);
  this->size_ = live;
  return true;
}

bool APIBuffer::drop_front_and_reserve(size_t drop, size_t n) {
  if (n > this->capacity_)
    return this->grow_(n, drop);
  this->size_ -= static_cast<uint16_t>(drop);
  std::memmove(this->data_.get(), this->data_.get() + drop, this->size_);
  return true;
}

}  // namespace esphome::api
