#include "json_util.h"
#include "esphome/core/log.h"

// ArduinoJson::Allocator is included via ArduinoJson.h in json_util.h

namespace esphome {
namespace json {

static const char *const TAG = "json";

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
#ifdef USE_PSRAM
  auto doc_allocator = SpiRamAllocator();
  JsonDocument json_document(&doc_allocator);
#else
  JsonDocument json_document;
#endif
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
