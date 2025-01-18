#include "json_util.h"
#include "esphome/core/log.h"

namespace esphome {
namespace json {

static const char *const TAG = "json";

static std::vector<char> global_json_build_buffer;  // NOLINT
static const auto ALLOCATOR = RAMAllocator<uint8_t>(RAMAllocator<uint8_t>::ALLOC_INTERNAL);

std::string build_json(const json_build_t &f) {
  // Here we are allocating up to 5kb of memory,
  // with the heap size minus 2kb to be safe if less than 5kb
  // as we can not have a true dynamic sized document.
  // The excess memory is freed below with `shrinkToFit()`
  auto free_heap = ALLOCATOR.get_max_free_block_size();
  size_t request_size = std::min(free_heap, (size_t) 512);
  while (true) {
    ESP_LOGV(TAG, "Attempting to allocate %zu bytes for JSON serialization", request_size);
    DynamicJsonDocument json_document(request_size);
    if (json_document.capacity() == 0) {
      ESP_LOGE(TAG,
               "Could not allocate memory for JSON document! Requested %zu bytes, largest free heap block: %zu bytes",
               request_size, free_heap);
      return "{}";
    }
    JsonObject root = json_document.to<JsonObject>();
    f(root);
    if (json_document.overflowed()) {
      if (request_size == free_heap) {
        ESP_LOGE(TAG, "Could not allocate memory for JSON document! Overflowed largest free heap block: %zu bytes",
                 free_heap);
        return "{}";
      }
      request_size = std::min(request_size * 2, free_heap);
      continue;
    }
    json_document.shrinkToFit();
    ESP_LOGV(TAG, "Size after shrink %zu bytes", json_document.capacity());
    std::string output;
    serializeJson(json_document, output);
    return output;
  }
}

bool parse_json(const std::string &data, const json_parse_t &f) {
  // Here we are allocating 1.5 times the data size,
  // with the heap size minus 2kb to be safe if less than that
  // as we can not have a true dynamic sized document.
  // The excess memory is freed below with `shrinkToFit()`
  auto free_heap = ALLOCATOR.get_max_free_block_size();
  size_t request_size = std::min(free_heap, (size_t) (data.size() * 1.5));
  while (true) {
    DynamicJsonDocument json_document(request_size);
    if (json_document.capacity() == 0) {
      ESP_LOGE(TAG, "Could not allocate memory for JSON document! Requested %zu bytes, free heap: %zu", request_size,
               free_heap);
      return false;
    }
    DeserializationError err = deserializeJson(json_document, data);
    json_document.shrinkToFit();

    JsonObject root = json_document.as<JsonObject>();

    if (err == DeserializationError::Ok) {
      return f(root);
    } else if (err == DeserializationError::NoMemory) {
      if (request_size * 2 >= free_heap) {
        ESP_LOGE(TAG, "Can not allocate more memory for deserialization. Consider making source string smaller");
        return false;
      }
      ESP_LOGV(TAG, "Increasing memory allocation.");
      request_size *= 2;
      continue;
    } else {
      ESP_LOGE(TAG, "JSON parse error: %s", err.c_str());
      return false;
    }
  };
  return false;
}

}  // namespace json
}  // namespace esphome
