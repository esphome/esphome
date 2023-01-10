#include "json_util.h"
#include "esphome/core/log.h"

#ifdef USE_ESP8266
#include <Esp.h>
#endif
#ifdef USE_ESP32
#include <esp_heap_caps.h>
#endif
#ifdef USE_RP2040
#include <Arduino.h>
#endif

namespace esphome {
namespace json {

static const char *const TAG = "json";

static std::vector<char> global_json_build_buffer;  // NOLINT

std::string build_json(const json_build_t &f) {
  // Here we are allocating up to 5kb of memory,
  // with the heap size minus 2kb to be safe if less than 5kb
  // as we can not have a true dynamic sized document.
  // The excess memory is freed below with `shrinkToFit()`
#ifdef USE_ESP8266
  const size_t free_heap = ESP.getMaxFreeBlockSize();  // NOLINT(readability-static-accessed-through-instance)
#elif defined(USE_ESP32)
  const size_t free_heap = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
#elif defined(USE_RP2040)
  const size_t free_heap = rp2040.getFreeHeap();
#endif

  size_t request_size = std::min(free_heap, (size_t) 512);
  while (true) {
    ESP_LOGV(TAG, "Attempting to allocate %u bytes for JSON serialization", request_size);
    DynamicJsonDocument json_document(request_size);
    if (json_document.capacity() == 0) {
      ESP_LOGE(TAG,
               "Could not allocate memory for JSON document! Requested %u bytes, largest free heap block: %u bytes",
               request_size, free_heap);
      return "{}";
    }
    JsonObject root = json_document.to<JsonObject>();
    f(root);
    if (json_document.overflowed()) {
      if (request_size == free_heap) {
        ESP_LOGE(TAG, "Could not allocate memory for JSON document! Overflowed largest free heap block: %u bytes",
                 free_heap);
        return "{}";
      }
      request_size = std::min(request_size * 2, free_heap);
      continue;
    }
    json_document.shrinkToFit();
    ESP_LOGV(TAG, "Size after shrink %u bytes", json_document.capacity());
    std::string output;
    serializeJson(json_document, output);
    return output;
  }
}

void parse_json(const std::string &data, const json_parse_t &f) {
  // Here we are allocating 1.5 times the data size,
  // with the heap size minus 2kb to be safe if less than that
  // as we can not have a true dynamic sized document.
  // The excess memory is freed below with `shrinkToFit()`
#ifdef USE_ESP8266
  const size_t free_heap = ESP.getMaxFreeBlockSize();  // NOLINT(readability-static-accessed-through-instance)
#elif defined(USE_ESP32)
  const size_t free_heap = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
#elif defined(USE_RP2040)
  const size_t free_heap = rp2040.getFreeHeap();
#endif
  bool pass = false;
  size_t request_size = std::min(free_heap, (size_t)(data.size() * 1.5));
  do {
    DynamicJsonDocument json_document(request_size);
    if (json_document.capacity() == 0) {
      ESP_LOGE(TAG, "Could not allocate memory for JSON document! Requested %u bytes, free heap: %u", request_size,
               free_heap);
      return;
    }
    DeserializationError err = deserializeJson(json_document, data);
    json_document.shrinkToFit();

    JsonObject root = json_document.as<JsonObject>();

    if (err == DeserializationError::Ok) {
      pass = true;
      f(root);
    } else if (err == DeserializationError::NoMemory) {
      if (request_size * 2 >= free_heap) {
        ESP_LOGE(TAG, "Can not allocate more memory for deserialization. Consider making source string smaller");
        return;
      }
      ESP_LOGV(TAG, "Increasing memory allocation.");
      request_size *= 2;
      continue;
    } else {
      ESP_LOGE(TAG, "JSON parse error: %s", err.c_str());
      return;
    }
  } while (!pass);
}

}  // namespace json
}  // namespace esphome
