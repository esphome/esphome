#pragma once

#include <vector>

#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"

#define ARDUINOJSON_ENABLE_STD_STRING 1  // NOLINT

#define ARDUINOJSON_USE_LONG_LONG 1  // NOLINT

#include <ArduinoJson.h>

namespace esphome {
namespace json {

#ifdef USE_PSRAM
// Build an allocator for the JSON Library using the RAMAllocator class
// This is only compiled when PSRAM is enabled
struct SpiRamAllocator : ArduinoJson::Allocator {
  void *allocate(size_t size) override { return allocator_.allocate(size); }

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
    return allocator_.reallocate(static_cast<uint8_t *>(ptr), new_size);
  }

 protected:
  RAMAllocator<uint8_t> allocator_{RAMAllocator<uint8_t>::NONE};
};
#endif

/// Callback function typedef for parsing JsonObjects.
using json_parse_t = std::function<bool(JsonObject)>;

/// Callback function typedef for building JsonObjects.
using json_build_t = std::function<void(JsonObject)>;

/// Build a JSON string with the provided json build function.
std::string build_json(const json_build_t &f);

/// Parse a JSON string and run the provided json parse function if it's valid.
bool parse_json(const std::string &data, const json_parse_t &f);

/// Parse a JSON string and return the root JsonDocument (or an unbound object on error)
JsonDocument parse_json(const uint8_t *data, size_t len);
/// Parse a JSON string and return the root JsonDocument (or an unbound object on error)
inline JsonDocument parse_json(const std::string &data) {
  return parse_json(reinterpret_cast<const uint8_t *>(data.c_str()), data.size());
}

/// Builder class for creating JSON documents without lambdas
class JsonBuilder {
 public:
  JsonObject root() {
    if (!root_created_) {
      root_ = doc_.to<JsonObject>();
      root_created_ = true;
    }
    return root_;
  }

  std::string serialize();

 private:
#ifdef USE_PSRAM
  SpiRamAllocator allocator_;
  JsonDocument doc_{&allocator_};
#else
  JsonDocument doc_;
#endif
  JsonObject root_;
  bool root_created_{false};
};

}  // namespace json
}  // namespace esphome
