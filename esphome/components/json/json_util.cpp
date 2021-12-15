#include "json_util.h"
#include "esphome/core/log.h"

namespace esphome {
namespace json {

static const char *const TAG = "json";

static std::vector<char> global_json_build_buffer;  // NOLINT

std::string build_json(const json_build_t &f) {
#ifdef USE_ESP8266
  const size_t free_heap = ESP.getMaxFreeBlockSize() - 2048;
#elif defined(USE_ESP32)
  const size_t free_heap = heap_caps_get_largest_free_block() - 2048;
#endif

  DynamicJsonDocument json_document(free_heap);
  JsonObject root = json_document.to<JsonObject>();
  f(root);
  json_document.shrinkToFit();

  std::string output;
  serializeJson(json_document, output);
  return output;
}

void parse_json(const std::string &data, const json_parse_t &f) {
#ifdef USE_ESP8266
  const size_t free_heap = ESP.getMaxFreeBlockSize() - 2048;
#elif defined(USE_ESP32)
  const size_t free_heap = heap_caps_get_largest_free_block() - 2048;
#endif

  DynamicJsonDocument json_document(free_heap);
  DeserializationError err = deserializeJson(json_document, data);
  json_document.shrinkToFit();

  JsonObject root = json_document.as<JsonObject>();

  if (err) {
    ESP_LOGW(TAG, "Parsing JSON failed.");
    return;
  }

  f(root);
}

}  // namespace json
}  // namespace esphome
