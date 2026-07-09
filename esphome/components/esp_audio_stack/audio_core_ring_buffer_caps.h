#pragma once

#ifdef USE_ESP32

#include "esphome/components/ring_buffer/ring_buffer.h"
#include "audio_core_log_utils.h"

#include <esp_heap_caps.h>
#include <esp_memory_utils.h>
#include <freertos/FreeRTOS.h>
#include <freertos/ringbuf.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <algorithm>
#include <cstring>

namespace esphome::esp_audio_stack {

/// Ring buffer allocation policy.
///
/// ESPHome's stock RingBuffer::create() now supports EXTERNAL_FIRST and
/// INTERNAL_FIRST preferences. It still does not offer strict placement or
/// boot-time placement logging, so latency-sensitive audio code cannot prove
/// whether a hot ring landed in internal RAM.
///
/// This helper provides explicit, verifiable placement:
///   - INTERNAL: always internal RAM. Use for anything in the audio hot path.
///   - PREFER_PSRAM: try PSRAM first, fall back to internal. Use for large
///     non-realtime buffers (protocol, staging) when internal is tight.
///
/// At creation time the helper logs name, size, policy, and actual placement
/// (verified via esp_ptr_internal). This makes memory policy auditable at boot.
enum class RingBufferPolicy {
  INTERNAL,
  PREFER_PSRAM,
};

/// ESPHome ring_buffer::RingBuffer with caller-controlled storage capabilities.
///
/// Keep the concrete type in our ownership model: ESPHome's RingBuffer
/// destructor is not virtual, so deleting a derived buffer through
/// std::unique_ptr<RingBuffer> would be undefined behaviour even though the
/// derived object has no additional fields.
class CapsRingBuffer : public ring_buffer::RingBuffer {
 public:
  ~CapsRingBuffer();

  bool install(size_t len, uint32_t caps, RingbufferType_t type = RINGBUF_TYPE_BYTEBUF);
  const void *probe_storage() const { return this->storage_; }

  size_t read(void *data, size_t len, TickType_t ticks_to_wait = 0);
  size_t write(const void *data, size_t len);
  size_t write_without_replacement(const void *data, size_t len, TickType_t ticks_to_wait = 0,
                                   bool write_partial = true);
  BaseType_t reset();

 protected:
  bool discard_bytes_(size_t discard_bytes);

 private:
  bool is_nosplit_() const { return this->type_ == RINGBUF_TYPE_NOSPLIT; }
  RingbufferType_t type_{RINGBUF_TYPE_BYTEBUF};
};

using RingBufferPtr = std::unique_ptr<CapsRingBuffer>;

/// Create a ring buffer with an explicit memory-placement policy.
///
/// @param len    capacity in bytes
/// @param policy placement policy
/// @param name   identifier for boot-time placement log (must outlive the call)
/// @return owned pointer, or nullptr on allocation failure
RingBufferPtr create_ring_buffer(size_t len, RingBufferPolicy policy, const char *name);
RingBufferPtr create_nosplit_ring_buffer(size_t len, RingBufferPolicy policy, const char *name);

/// Convenience: force internal RAM. Use for audio hot-path ring buffers.
inline RingBufferPtr create_internal(size_t len, const char *name) {
  return create_ring_buffer(len, RingBufferPolicy::INTERNAL, name);
}

/// Convenience: prefer PSRAM with internal fallback. Use for non-realtime buffers.
inline RingBufferPtr create_prefer_psram(size_t len, const char *name) {
  return create_ring_buffer(len, RingBufferPolicy::PREFER_PSRAM, name);
}

inline RingBufferPtr create_nosplit_internal(size_t len, const char *name) {
  return create_nosplit_ring_buffer(len, RingBufferPolicy::INTERNAL, name);
}

inline RingBufferPtr create_nosplit_prefer_psram(size_t len, const char *name) {
  return create_nosplit_ring_buffer(len, RingBufferPolicy::PREFER_PSRAM, name);
}

namespace detail {

inline const char *policy_str(RingBufferPolicy p) {
  switch (p) {
    case RingBufferPolicy::INTERNAL:
      return "internal";
    case RingBufferPolicy::PREFER_PSRAM:
      return "prefer_psram";
  }
  return "?";
}

}  // namespace detail

inline CapsRingBuffer::~CapsRingBuffer() {
  if (this->handle_ != nullptr) {
    vRingbufferDelete(this->handle_);
    heap_caps_free(this->storage_);
    this->handle_ = nullptr;
    this->storage_ = nullptr;
    this->size_ = 0;
  }
}

inline bool CapsRingBuffer::install(size_t len, uint32_t caps, RingbufferType_t type) {
  uint8_t *mem = static_cast<uint8_t *>(heap_caps_malloc(len, caps));
  if (mem == nullptr)
    return false;
  this->storage_ = mem;
  this->size_ = len;
  this->type_ = type;
  this->handle_ = xRingbufferCreateStatic(len, type, mem, &this->structure_);
  if (this->handle_ == nullptr) {
    heap_caps_free(mem);
    this->storage_ = nullptr;
    this->size_ = 0;
    this->type_ = RINGBUF_TYPE_BYTEBUF;
    return false;
  }
  return true;
}

inline size_t CapsRingBuffer::read(void *data, size_t len, TickType_t ticks_to_wait) {
  if (!this->is_nosplit_())
    return ring_buffer::RingBuffer::read(data, len, ticks_to_wait);

  size_t item_len = 0;
  void *item = xRingbufferReceive(this->handle_, &item_len, ticks_to_wait);
  if (item == nullptr)
    return 0;

  const size_t copied = std::min(item_len, len);
  std::memcpy(data, item, copied);
  vRingbufferReturnItem(this->handle_, item);
  return copied;
}

inline size_t CapsRingBuffer::write(const void *data, size_t len) {
  if (!this->is_nosplit_())
    return ring_buffer::RingBuffer::write(data, len);

  while (this->free() < len) {
    if (!this->discard_bytes_(len - this->free()))
      break;
  }
  return this->write_without_replacement(data, len, 0, false);
}

inline size_t CapsRingBuffer::write_without_replacement(const void *data, size_t len, TickType_t ticks_to_wait,
                                                        bool write_partial) {
  if (!this->is_nosplit_())
    return ring_buffer::RingBuffer::write_without_replacement(data, len, ticks_to_wait, write_partial);

  if (xRingbufferSend(this->handle_, data, len, ticks_to_wait))
    return len;
  return 0;
}

inline BaseType_t CapsRingBuffer::reset() {
  if (!this->is_nosplit_())
    return ring_buffer::RingBuffer::reset();

  while (true) {
    size_t item_len = 0;
    void *item = xRingbufferReceive(this->handle_, &item_len, 0);
    if (item == nullptr)
      return pdPASS;
    vRingbufferReturnItem(this->handle_, item);
  }
}

inline bool CapsRingBuffer::discard_bytes_(size_t discard_bytes) {
  if (!this->is_nosplit_())
    return ring_buffer::RingBuffer::discard_bytes_(discard_bytes);

  size_t discarded = 0;
  while (discarded < discard_bytes) {
    size_t item_len = 0;
    void *item = xRingbufferReceive(this->handle_, &item_len, 0);
    if (item == nullptr)
      break;
    discarded += item_len;
    vRingbufferReturnItem(this->handle_, item);
  }
  return discarded >= discard_bytes;
}

inline RingBufferPtr create_ring_buffer_with_type(size_t len, RingBufferPolicy policy, const char *name,
                                                  RingbufferType_t type) {
  auto rb = std::make_unique<CapsRingBuffer>();

  bool ok = false;
  switch (policy) {
    case RingBufferPolicy::INTERNAL:
      ok = rb->install(len, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT, type);
      break;
    case RingBufferPolicy::PREFER_PSRAM:
      ok = rb->install(len, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT, type);
      if (!ok)
        ok = rb->install(len, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT, type);
      break;
  }

  if (!ok) {
    log_error("ring_buffer_caps", "ringbuffer '%s': alloc %u bytes FAILED (policy=%s)", name,
              static_cast<unsigned>(len), detail::policy_str(policy));
    return nullptr;
  }

  const void *storage = rb->probe_storage();
  const char *placement = esp_ptr_internal(storage) ? "internal" : "psram";
  log_info("ring_buffer_caps", "ringbuffer '%s': size=%u policy=%s type=%s placement=%s", name,
           static_cast<unsigned>(len), detail::policy_str(policy), type == RINGBUF_TYPE_NOSPLIT ? "nosplit" : "bytebuf",
           placement);

  return rb;
}

inline RingBufferPtr create_ring_buffer(size_t len, RingBufferPolicy policy, const char *name) {
  return create_ring_buffer_with_type(len, policy, name, RINGBUF_TYPE_BYTEBUF);
}

inline RingBufferPtr create_nosplit_ring_buffer(size_t len, RingBufferPolicy policy, const char *name) {
  return create_ring_buffer_with_type(len, policy, name, RINGBUF_TYPE_NOSPLIT);
}

}  // namespace esphome::esp_audio_stack

#endif  // USE_ESP32
