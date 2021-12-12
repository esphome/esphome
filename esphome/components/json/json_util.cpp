#ifdef USE_ARDUINO

#include "json_util.h"
#include "esphome/core/log.h"

namespace esphome {
namespace json {

static const char *const TAG = "json";

static std::vector<char> global_json_build_buffer;  // NOLINT

const char *build_json(const json_build_t &f, size_t *length) {
  global_json_buffer.clear();
  JsonObject &root = global_json_buffer.createObject();

  f(root);

  // The Json buffer size gives us a good estimate for the required size.
  // Usually, it's a bit larger than the actual required string size
  //             | JSON Buffer Size | String Size |
  // Discovery   | 388              | 351         |
  // Discovery   | 372              | 356         |
  // Discovery   | 336              | 311         |
  // Discovery   | 408              | 393         |
  global_json_build_buffer.reserve(global_json_buffer.size() + 1);
  size_t bytes_written = root.printTo(global_json_build_buffer.data(), global_json_build_buffer.capacity());

  if (bytes_written >= global_json_build_buffer.capacity() - 1) {
    global_json_build_buffer.reserve(root.measureLength() + 1);
    bytes_written = root.printTo(global_json_build_buffer.data(), global_json_build_buffer.capacity());
  }

  *length = bytes_written;
  return global_json_build_buffer.data();
}
void parse_json(const std::string &data, const json_parse_t &f) {
  global_json_buffer.clear();
  JsonObject &root = global_json_buffer.parseObject(data);

  if (!root.success()) {
    ESP_LOGW(TAG, "Parsing JSON failed.");
    return;
  }

  f(root);
}
std::string build_json(const json_build_t &f) {
  size_t len;
  const char *c_str = build_json(f, &len);
  return std::string(c_str, len);
}

VectorJsonBuffer::String::String(VectorJsonBuffer *parent) : parent_(parent), start_(parent->size_) {}
void VectorJsonBuffer::String::append(char c) const {
  char *last = static_cast<char *>(this->parent_->do_alloc(1));
  *last = c;
}
const char *VectorJsonBuffer::String::c_str() const {
  this->append('\0');
  return &this->parent_->buffer_[this->start_];
}
void VectorJsonBuffer::clear() {
  for (char *block : this->free_blocks_)
    free(block);  // NOLINT

  this->size_ = 0;
  this->free_blocks_.clear();
}
VectorJsonBuffer::String VectorJsonBuffer::startString() { return {this}; }  // NOLINT
void *VectorJsonBuffer::alloc(size_t bytes) {
  // Make sure memory addresses are aligned
  uint32_t new_size = round_size_up(this->size_);
  this->resize(new_size);
  return this->do_alloc(bytes);
}
void *VectorJsonBuffer::do_alloc(size_t bytes) {  // NOLINT
  const uint32_t begin = this->size_;
  this->resize(begin + bytes);
  return &this->buffer_[begin];
}
void VectorJsonBuffer::resize(size_t size) {  // NOLINT
  if (size <= this->size_) {
    this->size_ = size;
    return;
  }

  this->reserve(size);
  this->size_ = size;
}
void VectorJsonBuffer::reserve(size_t size) {  // NOLINT
  if (size <= this->capacity_)
    return;

  uint32_t target_capacity = this->capacity_;
  if (this->capacity_ == 0) {
    // lazily initialize with a reasonable size
    target_capacity = JSON_OBJECT_SIZE(16);
  }
  while (target_capacity < size)
    target_capacity *= 2;

  char *old_buffer = this->buffer_;
  this->buffer_ = new char[target_capacity];  // NOLINT
  if (old_buffer != nullptr && this->capacity_ != 0) {
    this->free_blocks_.push_back(old_buffer);
    memcpy(this->buffer_, old_buffer, this->capacity_);
  }
  this->capacity_ = target_capacity;
}

size_t VectorJsonBuffer::size() const { return this->size_; }

VectorJsonBuffer global_json_buffer;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace json
}  // namespace esphome

#endif  // USE_ARDUINO
