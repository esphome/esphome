#pragma once

#include <vector>

#include "esphome/core/helpers.h"

#define ARDUINOJSON_ENABLE_STD_STRING 1  // NOLINT

#define ARDUINOJSON_USE_LONG_LONG 1  // NOLINT

#include <ArduinoJson.h>

namespace esphome {
namespace json {

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
  ~JsonBuilder();

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
  // Storage for SpiRamAllocator - typically around 24-32 bytes on ESP32
  // Static assert in .cpp file ensures this is large enough
  std::aligned_storage<32, alignof(void *)>::type allocator_storage_;
#endif
  JsonDocument doc_;
  JsonObject root_;
  bool root_created_{false};
};

}  // namespace json
}  // namespace esphome
