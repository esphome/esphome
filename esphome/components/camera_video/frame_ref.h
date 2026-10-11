#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

namespace esphome::camera_video {

/** Intrusively reference-counted frame base.
 *
 * Frame descriptors are preallocated by their producer. The final reference
 * lets the producer reclaim or reuse the underlying storage.
 */
class RefCountedFrame {
 public:
  void retain() { this->reference_count_.fetch_add(1, std::memory_order_relaxed); }

  void release() {
    if (this->reference_count_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
      this->on_last_reference();
    }
  }

  bool has_references() const { return this->reference_count_.load(std::memory_order_acquire) != 0; }

 protected:
  virtual void on_last_reference() = 0;
  virtual ~RefCountedFrame() = default;

 private:
  std::atomic<uint32_t> reference_count_{0};
};

template<typename T> class FrameRef {
  static_assert(std::is_base_of_v<RefCountedFrame, T>);

 public:
  FrameRef() = default;
  FrameRef(std::nullptr_t) {}

  explicit FrameRef(T *frame) : frame_(frame) {
    if (this->frame_ != nullptr) {
      this->frame_->retain();
    }
  }

  FrameRef(const FrameRef &other) : FrameRef(other.frame_) {}

  template<typename U, std::enable_if_t<std::is_convertible_v<U *, T *>, int> = 0>
  FrameRef(const FrameRef<U> &other) : FrameRef(other.get()) {}

  FrameRef(FrameRef &&other) noexcept : frame_(std::exchange(other.frame_, nullptr)) {}

  template<typename U, std::enable_if_t<std::is_convertible_v<U *, T *>, int> = 0>
  FrameRef(FrameRef<U> &&other) noexcept : frame_(std::exchange(other.frame_, nullptr)) {}

  ~FrameRef() { this->reset(); }

  FrameRef &operator=(const FrameRef &other) {
    if (this != &other) {
      FrameRef copy(other);
      this->swap(copy);
    }
    return *this;
  }

  FrameRef &operator=(FrameRef &&other) noexcept {
    if (this != &other) {
      this->reset();
      this->frame_ = std::exchange(other.frame_, nullptr);
    }
    return *this;
  }

  FrameRef &operator=(std::nullptr_t) {
    this->reset();
    return *this;
  }

  void reset() {
    T *frame = std::exchange(this->frame_, nullptr);
    if (frame != nullptr) {
      frame->release();
    }
  }

  void swap(FrameRef &other) noexcept { std::swap(this->frame_, other.frame_); }

  T *get() const { return this->frame_; }
  T &operator*() const { return *this->frame_; }
  T *operator->() const { return this->frame_; }
  explicit operator bool() const { return this->frame_ != nullptr; }
  bool operator==(std::nullptr_t) const { return this->frame_ == nullptr; }
  bool operator!=(std::nullptr_t) const { return this->frame_ != nullptr; }

 private:
  template<typename U> friend class FrameRef;
  T *frame_{nullptr};
};

template<typename T> void swap(FrameRef<T> &left, FrameRef<T> &right) noexcept { left.swap(right); }

}  // namespace esphome::camera_video
