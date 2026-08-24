#include "api_buffer.h"

namespace esphome::api {

void APIBuffer::grow_(size_t n) {
  auto new_data = make_buffer(n);
  if (this->size_)
    std::memcpy(new_data.get(), this->data_.get(), this->size_);
  this->data_ = std::move(new_data);
  this->capacity_ = n;
}

}  // namespace esphome::api
