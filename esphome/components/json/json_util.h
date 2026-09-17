#pragma once

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"

#define ARDUINOJSON_ENABLE_STD_STRING 1  // NOLINT

#define ARDUINOJSON_USE_LONG_LONG 1  // NOLINT

#include <ArduinoJson.h>

namespace esphome::json {

/// Buffer for JSON serialization that uses stack allocation for small payloads.
/// Template parameter STACK_SIZE specifies the stack buffer size (default 512 bytes).
/// Supports move semantics for efficient return-by-value.
template<size_t STACK_SIZE = 640> class SerializationBuffer {
 public:
  static constexpr size_t BUFFER_SIZE = STACK_SIZE;  ///< Stack buffer size for this instantiation

  /// Construct with known size (typically from measureJson)
  explicit SerializationBuffer(size_t size) : size_(size) {
    if (size + 1 <= STACK_SIZE) {
      buffer_ = stack_buffer_;
    } else {
      heap_buffer_ = new char[size + 1];
      buffer_ = heap_buffer_;
    }
    buffer_[0] = '\0';
  }

  ~SerializationBuffer() { delete[] heap_buffer_; }

  // Move constructor - works with same template instantiation
  SerializationBuffer(SerializationBuffer &&other) noexcept : heap_buffer_(other.heap_buffer_), size_(other.size_) {
    if (other.buffer_ == other.stack_buffer_) {
      // Stack buffer - must copy content
      std::memcpy(stack_buffer_, other.stack_buffer_, size_ + 1);
      buffer_ = stack_buffer_;
    } else {
      // Heap buffer - steal ownership
      buffer_ = heap_buffer_;
      other.heap_buffer_ = nullptr;
    }
    // Leave moved-from object in valid empty state
    other.stack_buffer_[0] = '\0';
    other.buffer_ = other.stack_buffer_;
    other.size_ = 0;
  }

  // Move assignment
  SerializationBuffer &operator=(SerializationBuffer &&other) noexcept {
    if (this != &other) {
      delete[] heap_buffer_;
      heap_buffer_ = other.heap_buffer_;
      size_ = other.size_;
      if (other.buffer_ == other.stack_buffer_) {
        std::memcpy(stack_buffer_, other.stack_buffer_, size_ + 1);
        buffer_ = stack_buffer_;
      } else {
        buffer_ = heap_buffer_;
        other.heap_buffer_ = nullptr;
      }
      // Leave moved-from object in valid empty state
      other.stack_buffer_[0] = '\0';
      other.buffer_ = other.stack_buffer_;
      other.size_ = 0;
    }
    return *this;
  }

  // Delete copy operations
  SerializationBuffer(const SerializationBuffer &) = delete;
  SerializationBuffer &operator=(const SerializationBuffer &) = delete;

  /// Get null-terminated C string
  const char *c_str() const { return buffer_; }
  /// Get data pointer
  const char *data() const { return buffer_; }
  /// Get string length (excluding null terminator)
  size_t size() const { return size_; }

  /// Implicit conversion to std::string for backward compatibility
  /// WARNING: This allocates a new std::string on the heap. Prefer using
  /// c_str() or data()/size() directly when possible to avoid allocation.
  operator std::string() const { return std::string(buffer_, size_); }  // NOLINT(google-explicit-constructor)

 private:
  friend class JsonBuilder;  ///< Allows JsonBuilder::serialize() to call private methods

  /// Get writable buffer (for serialization)
  char *data_writable_() { return buffer_; }
  /// Set actual size after serialization (must not exceed allocated size)
  /// Also ensures null termination for c_str() safety
  void set_size_(size_t size) {
    size_ = size;
    buffer_[size] = '\0';
  }

  /// Reallocate to heap buffer with new size (for when stack buffer is too small)
  /// This invalidates any previous buffer content. Used by JsonBuilder::serialize().
  void reallocate_heap_(size_t size) {
    delete[] heap_buffer_;
    heap_buffer_ = new char[size + 1];
    buffer_ = heap_buffer_;
    size_ = size;
    buffer_[0] = '\0';
  }

  char stack_buffer_[STACK_SIZE];
  char *heap_buffer_{nullptr};
  char *buffer_;
  size_t size_;
};

#ifdef USE_PSRAM
// Build an allocator for the JSON Library using the RAMAllocator class
// This is only compiled when PSRAM is enabled
struct SpiRamAllocator : ArduinoJson::Allocator {
  void *allocate(size_t size) override {
    RAMAllocator<uint8_t> allocator;
    return allocator.allocate(size);
  }

  void deallocate(void *ptr) override {
    // ArduinoJson's Allocator interface doesn't provide the size parameter in deallocate.
    // RAMAllocator::deallocate() requires the size, which we don't have access to here.
    // RAMAllocator::deallocate implementation just calls free() regardless of whether
    // the memory was allocated with heap_caps_malloc or malloc.
    // This is safe because ESP-IDF's heap implementation internally tracks the memory region
    // and routes free() to the appropriate heap.
    free(ptr);  // NOLINT(cppcoreguidelines-owning-memory,cppcoreguidelines-no-malloc)
  }

  void *reallocate(void *ptr, size_t new_size) override {
    RAMAllocator<uint8_t> allocator;
    return allocator.reallocate(static_cast<uint8_t *>(ptr), new_size);
  }
};
#endif

/// Callback function typedef for parsing JsonObjects.
using json_parse_t = std::function<bool(JsonObject)>;

/// Callback function typedef for building JsonObjects.
using json_build_t = std::function<void(JsonObject)>;

/// Build a JSON string with the provided json build function.
/// Returns SerializationBuffer for stack-first allocation; implicitly converts to std::string.
SerializationBuffer<> build_json(const json_build_t &f);

/// Parse a JSON string and run the provided json parse function if it's valid.
bool parse_json(const std::string &data, const json_parse_t &f);
/// Parse JSON from raw bytes and run the provided json parse function if it's valid.
bool parse_json(const uint8_t *data, size_t len, const json_parse_t &f);

/// Parse a JSON string and return the root JsonDocument (or an unbound object on error)
JsonDocument parse_json(const uint8_t *data, size_t len);
/// Parse a JSON string and return the root JsonDocument (or an unbound object on error)
inline JsonDocument parse_json(const std::string &data) {
  return parse_json(reinterpret_cast<const uint8_t *>(data.c_str()), data.size());
}

/// The allocator a JsonBuilder uses by default (PSRAM first when available)
ArduinoJson::Allocator *heap_json_allocator();

/// Bump allocator over a fixed buffer, for a document that is built and serialized in one scope.
/// A request the buffer cannot hold goes to the heap allocator instead, so an oversized document
/// still works. Nothing is returned to the buffer until the arena itself goes away.
template<size_t N> class JsonArena final : public ArduinoJson::Allocator {
 public:
  JsonArena() = default;
  // The document holds pointers into buf_, so the arena must stay where it is
  JsonArena(const JsonArena &) = delete;
  JsonArena &operator=(const JsonArena &) = delete;

  void *allocate(size_t size) override {
    size = (size + ALIGN - 1) & ~(ALIGN - 1);
    if (size > N - this->used_) {
      return heap_json_allocator()->allocate(size);
    }
    this->last_ = this->used_;
    this->used_ += size;
    return this->buf_ + this->last_;
  }
  void deallocate(void *ptr) override {
    if (!this->owns_(ptr)) {
      heap_json_allocator()->deallocate(ptr);
    }
  }
  void *reallocate(void *ptr, size_t new_size) override {
    if (!this->owns_(ptr)) {
      return heap_json_allocator()->reallocate(ptr, new_size);
    }
    const size_t off = static_cast<uint8_t *>(ptr) - this->buf_;
    const size_t size = (new_size + ALIGN - 1) & ~(ALIGN - 1);
    if (off == this->last_) {
      if (size <= N - off) {
        this->used_ = off + size;  // the newest block grows or shrinks in place
        return ptr;
      }
      this->used_ = off;  // it moves to the heap, so its arena space is free again
    }
    // The old size of an older block is unknown; copying up to the end of the buffer stays in
    // bounds and covers whatever the block held
    void *moved = heap_json_allocator()->allocate(new_size);
    if (moved != nullptr) {
      std::memcpy(moved, ptr, std::min(new_size, N - off));
    }
    return moved;
  }

 private:
  static constexpr size_t ALIGN = alignof(std::max_align_t);
  bool owns_(const void *ptr) const { return ptr >= this->buf_ && ptr < this->buf_ + N; }
  alignas(ALIGN) uint8_t buf_[N];
  size_t used_{0};
  size_t last_{0};
};

/// Builder class for creating JSON documents without lambdas
class JsonBuilder {
 public:
  // Out of line: inlining the JsonDocument constructor duplicates it at every call site
  JsonBuilder();
  // The builder must not outlive the allocator, the document holds memory it handed out
  explicit JsonBuilder(ArduinoJson::Allocator *allocator);

  JsonObject root() {
    if (!root_created_) {
      root_ = doc_.to<JsonObject>();
      root_created_ = true;
    }
    return root_;
  }

  /// Serialize into a buffer the caller owns. Returns the length, which is cap or more when the
  /// document did not fit (buf then holds a truncated, unterminated copy). An overflowed document
  /// serializes as "{}".
  size_t serialize_to(char *buf, size_t cap);

  /// Serialize the JSON document to a SerializationBuffer (stack-first allocation)
  /// Uses 512-byte stack buffer by default, falls back to heap for larger JSON
  SerializationBuffer<> serialize();

 private:
  JsonDocument doc_;
  JsonObject root_;
  bool root_created_{false};
};

}  // namespace esphome::json
