#include "json_util.h"
#include "esphome/core/log.h"

// ArduinoJson::Allocator is included via ArduinoJson.h in json_util.h

namespace esphome {
namespace json {

static const char *const TAG = "json";

// Build an allocator for the JSON Library using the RAMAllocator class
struct SpiRamAllocator : ArduinoJson::Allocator {
  void *allocate(size_t size) override { return this->allocator_.allocate(size); }

  void deallocate(void *pointer) override {
    // ArduinoJson's Allocator interface doesn't provide the size parameter in deallocate.
    // RAMAllocator::deallocate() requires the size, which we don't have access to here.
    // RAMAllocator::deallocate implementation just calls free() regardless of whether
    // the memory was allocated with heap_caps_malloc or malloc.
    // This is safe because ESP-IDF's heap implementation internally tracks the memory region
    // and routes free() to the appropriate heap.
    free(pointer);  // NOLINT(cppcoreguidelines-owning-memory,cppcoreguidelines-no-malloc)
  }

  void *reallocate(void *ptr, size_t new_size) override {
    return this->allocator_.reallocate(static_cast<uint8_t *>(ptr), new_size);
  }

 protected:
  RAMAllocator<uint8_t> allocator_{RAMAllocator<uint8_t>(RAMAllocator<uint8_t>::NONE)};
};

std::string build_json(const json_build_t &f) {
  // NOLINTBEGIN(clang-analyzer-cplusplus.NewDeleteLeaks) false positive with ArduinoJson
  auto doc_allocator = SpiRamAllocator();
  JsonDocument json_document(&doc_allocator);
  if (json_document.overflowed()) {
    ESP_LOGE(TAG, "Could not allocate memory for JSON document!");
    return "{}";
  }
  JsonObject root = json_document.to<JsonObject>();
  f(root);
  if (json_document.overflowed()) {
    ESP_LOGE(TAG, "Could not allocate memory for JSON document!");
    return "{}";
  }
  std::string output;
  serializeJson(json_document, output);
  return output;
  // NOLINTEND(clang-analyzer-cplusplus.NewDeleteLeaks)
}

bool parse_json(const std::string &data, const json_parse_t &f) {
  // NOLINTBEGIN(clang-analyzer-cplusplus.NewDeleteLeaks) false positive with ArduinoJson
  auto doc_allocator = SpiRamAllocator();
  JsonDocument json_document(&doc_allocator);
  if (json_document.overflowed()) {
    ESP_LOGE(TAG, "Could not allocate memory for JSON document!");
    return false;
  }
  DeserializationError err = deserializeJson(json_document, data);

  JsonObject root = json_document.as<JsonObject>();

  if (err == DeserializationError::Ok) {
    return f(root);
  } else if (err == DeserializationError::NoMemory) {
    ESP_LOGE(TAG, "Can not allocate more memory for deserialization. Consider making source string smaller");
    return false;
  }
  ESP_LOGE(TAG, "Parse error: %s", err.c_str());
  return false;
  // NOLINTEND(clang-analyzer-cplusplus.NewDeleteLeaks)
}

}  // namespace json
}  // namespace esphome
