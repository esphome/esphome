#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <deque>
#include <mutex>

#include "esphome/components/audio_analyzer/sample_buffer.h"

namespace esphome::audio_analyzer::testing {

/// The ring buffer's contract (all-or-nothing writes, reads that never wait) on a locked deque, safe between threads.
class FakeSampleBuffer final : public SampleBuffer {
 public:
  explicit FakeSampleBuffer(size_t size) : size_(size) {}

  bool write(const void *data, size_t length) override {
    std::lock_guard<std::mutex> lock(this->mutex_);
    if (this->bytes_.size() + length > this->size_) {
      return false;
    }
    const auto *bytes = static_cast<const uint8_t *>(data);
    this->bytes_.insert(this->bytes_.end(), bytes, bytes + length);
    return true;
  }
  size_t read(void *data, size_t length) override {
    std::lock_guard<std::mutex> lock(this->mutex_);
    const size_t count = std::min(length, this->bytes_.size());
    std::copy_n(this->bytes_.begin(), count, static_cast<uint8_t *>(data));
    this->bytes_.erase(this->bytes_.begin(), this->bytes_.begin() + count);
    return count;
  }
  size_t available() const override {
    std::lock_guard<std::mutex> lock(this->mutex_);
    return this->bytes_.size();
  }
  void reset() override {
    std::lock_guard<std::mutex> lock(this->mutex_);
    this->bytes_.clear();
  }

  /// The next ``SampleBuffer::create()`` returns nullptr, like an allocation that ran out of memory.
  static bool fail_next_create;

 protected:
  mutable std::mutex mutex_;
  std::deque<uint8_t> bytes_;
  size_t size_;
};

}  // namespace esphome::audio_analyzer::testing
