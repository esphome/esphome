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

SerializationBuffer<> build_json(const json_build_t &f) {
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

SerializationBuffer<> JsonBuilder::serialize() {
  if (doc_.overflowed()) {
    ESP_LOGE(TAG, "JSON document overflow");
    SerializationBuffer<> result(2);
    auto *buf = result.data_writable();
    buf[0] = '{';
    buf[1] = '}';
    buf[2] = '\0';
    return result;
  }
  // Intentionally avoid measureJson() - it instantiates DummyWriter templates that add ~1KB of flash.
  // Instead, try serializing to stack buffer first. 768 bytes covers 99.9% of JSON payloads
  // (sensors ~200B, lights ~170B, climate ~700B). Only entities with 40+ options exceed this.
  // For the common case: single serialize to stack, no heap allocation, no measurement overhead.
  // For the rare large case: serialize twice (once truncated, once to heap) - less efficient but
  // saves ~1KB flash that would otherwise be wasted on every build.
  // serializeJson() returns actual size needed even if truncated, so we can retry with exact size.
  constexpr size_t buf_size = SerializationBuffer<>::BUFFER_SIZE;
  SerializationBuffer<> result(buf_size - 1);  // Max content size (reserve 1 for null)
  size_t size = serializeJson(doc_, result.data_writable(), buf_size);
  if (size < buf_size) {
    // Fits in stack buffer - update size to actual length
    result.set_size(size);
    return result;
  }
  // Needs heap allocation - serialize again with exact size
  SerializationBuffer<> heap_result(size);
  serializeJson(doc_, heap_result.data_writable(), size + 1);
  return heap_result;
}

}  // namespace json
}  // namespace esphome
