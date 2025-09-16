#pragma once

#include <vector>

#include "esphome/core/helpers.h"

#define ARDUINOJSON_ENABLE_STD_STRING 1  // NOLINT

#define ARDUINOJSON_USE_LONG_LONG 1  // NOLINT

#include <ArduinoJson.h>

namespace esphome {
namespace json {

#ifdef USE_PSRAM
// Allocator for JSON that uses PSRAM on supported devices
struct SpiRamAllocator : ArduinoJson::Allocator {
  void *allocate(size_t size) override { return allocator_.allocate(size); }

  void deallocate(void *ptr) override {
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

/// Builder class for creating JSON documents without lambdas
class JsonBuilder {
 public:
  JsonBuilder();

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
  SpiRamAllocator allocator_;  // Just a regular member on the stack!
#endif
  JsonDocument doc_;
  JsonObject root_;
  bool root_created_{false};
};

}  // namespace json
}  // namespace esphome
