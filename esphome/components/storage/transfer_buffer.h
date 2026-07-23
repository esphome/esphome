#pragma once
#include "esphome/core/defines.h"
// ESP32-only in fact as well as in name: the arena is external RAM reached through
// heap_caps_malloc, and codegen only sets the define when the psram component is configured
// (_transfer_buffer_final_validate enforces that). Spelling USE_ESP32 out keeps every other
// target -- and the host build the C++ unit tests use -- from reaching for esp_heap_caps.h.
#if defined(USE_STORAGE_TRANSFER_BUFFER) && defined(USE_ESP32)

#include <atomic>
#include <cstddef>
#include <cstdint>

#include "esphome/core/component.h"

namespace esphome::storage {

// Optional PSRAM staging arena for file transfers. Consumers (web_server file_api today,
// media/image prefetch later) borrow the whole buffer with single-owner semantics and give
// it back when done. Purely additive: when the buffer is absent, unallocated, busy or too
// small, every consumer falls back to its plain streaming path — no behavior depends on it.
class TransferBuffer : public Component {
 public:
  void setup() override;
  void dump_config() override;

  void set_size(size_t size) { this->size_ = size; }
  // Removes the 80%-of-PSRAM safety line for explicit sizes; a failing allocation then
  // remains the only (graceful) guard.
  void set_override_limit(bool v) { this->override_limit_ = v; }

  // Single-owner borrow, callable from any task (atomic claim). Returns the buffer base
  // when `need` fits and the buffer is free, nullptr otherwise — callers must treat a
  // nullptr as "stream instead", never as an error.
  uint8_t *try_acquire(size_t need);
  void release();
  size_t capacity() const { return this->buf_ != nullptr ? this->size_ : 0; }
  // True only when the arena is DMA-capable PSRAM (S3/P4). Consumers that want to DMA out of the
  // buffer must check this; on other chips the arena is memcpy-staging only.
  bool is_dma_capable() const { return this->dma_capable_; }

 protected:
  size_t size_{0};
  bool override_limit_{false};
  bool dma_capable_{false};
  uint8_t *buf_{nullptr};
  std::atomic<bool> busy_{false};
};

extern TransferBuffer *global_transfer_buffer;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esphome::storage
#endif  // USE_STORAGE_TRANSFER_BUFFER && USE_ESP32
