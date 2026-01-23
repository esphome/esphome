#include "json_util.h"
#include "esphome/core/log.h"

// ArduinoJson::Allocator is included via ArduinoJson.h in json_util.h

namespace esphome {
namespace json {

static const char *const TAG = "json";

#ifdef USE_PSRAM
// Global allocator that outlives all JsonDocuments returned by parse_json()
// This prevents dangling pointer issues when JsonDocuments are returned from functions
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables) - Must be mutable for ArduinoJson::Allocator
static SpiRamAllocator global_json_allocator;
#endif

std::string build_json(const json_build_t &f) {
  // NOLINTBEGIN(clang-analyzer-cplusplus.NewDeleteLeaks) false positive with ArduinoJson
  JsonBuilder builder;
  JsonObject root = builder.root();
  f(root);
  return builder.serialize();
  // NOLINTEND(clang-analyzer-cplusplus.NewDeleteLeaks)
}

bool parse_json(const std::string &data, const json_parse_t &f) {
  // NOLINTBEGIN(clang-analyzer-cplusplus.NewDeleteLeaks) false positive with ArduinoJson
  JsonDocument doc = parse_json(reinterpret_cast<const uint8_t *>(data.c_str()), data.size());
  if (doc.overflowed() || doc.isNull())
    return false;
  return f(doc.as<JsonObject>());
  // NOLINTEND(clang-analyzer-cplusplus.NewDeleteLeaks)
}

JsonDocument parse_json(const uint8_t *data, size_t len) {
  // NOLINTBEGIN(clang-analyzer-cplusplus.NewDeleteLeaks) false positive with ArduinoJson
  if (data == nullptr || len == 0) {
    ESP_LOGE(TAG, "No data to parse");
    return JsonObject();  // return unbound object
  }
#ifdef USE_PSRAM
  JsonDocument json_document(&global_json_allocator);
#else
  JsonDocument json_document;
#endif
  if (json_document.overflowed()) {
    ESP_LOGE(TAG, "Could not allocate memory for JSON document!");
    return JsonObject();  // return unbound object
  }
  DeserializationError err = deserializeJson(json_document, data, len);

  if (err == DeserializationError::Ok) {
    return json_document;
  } else if (err == DeserializationError::NoMemory) {
    ESP_LOGE(TAG, "Can not allocate more memory for deserialization. Consider making source string smaller");
    return JsonObject();  // return unbound object
  }
  ESP_LOGE(TAG, "Parse error: %s", err.c_str());
  return JsonObject();  // return unbound object
  // NOLINTEND(clang-analyzer-cplusplus.NewDeleteLeaks)
}

std::string JsonBuilder::serialize() {
  if (doc_.overflowed()) {
    ESP_LOGE(TAG, "JSON document overflow");
    return "{}";
  }
  std::string output;
  serializeJson(doc_, output);
  return output;
}

}  // namespace json
}  // namespace esphome
