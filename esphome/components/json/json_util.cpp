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
  // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks) false positive with ArduinoJson
  return parse_json(reinterpret_cast<const uint8_t *>(data.c_str()), data.size(), f);
}

bool parse_json(const uint8_t *data, size_t len, const json_parse_t &f) {
  // NOLINTBEGIN(clang-analyzer-cplusplus.NewDeleteLeaks) false positive with ArduinoJson
  JsonDocument doc = parse_json(data, len);
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
  // ===========================================================================================
  // CRITICAL: NRVO (Named Return Value Optimization) - DO NOT REFACTOR WITHOUT UNDERSTANDING
  // ===========================================================================================
  //
  // This function is carefully structured to enable NRVO. The compiler constructs `result`
  // directly in the caller's stack frame, eliminating the move constructor call entirely.
  //
  // WITHOUT NRVO: Each return would trigger SerializationBuffer's move constructor, which
  // must memcpy up to 512 bytes of stack buffer content. This happens on EVERY JSON
  // serialization (sensor updates, web server responses, MQTT publishes, etc.).
  //
  // WITH NRVO: Zero memcpy, zero move constructor overhead. The buffer lives directly
  // where the caller needs it.
  //
  // Requirements for NRVO to work:
  //   1. Single named variable (`result`) returned from ALL paths
  //   2. All paths must return the SAME variable (not different variables)
  //   3. No std::move() on the return statement
  //
  // If you must modify this function:
  //   - Keep a single `result` variable declared at the top
  //   - All code paths must return `result` (not a different variable)
  //   - Verify NRVO still works by checking the disassembly for move constructor calls
  //   - Test: objdump -d -C firmware.elf | grep "SerializationBuffer.*SerializationBuffer"
  //     Should show only destructor, NOT move constructor
  //
  // Why we avoid measureJson(): It instantiates DummyWriter templates adding ~1KB flash.
  // Instead, try stack buffer first. 512 bytes covers 99.9% of JSON payloads (sensors ~200B,
  // lights ~170B, climate ~700B). Only entities with 40+ options exceed this.
  //
  // ===========================================================================================
  constexpr size_t buf_size = SerializationBuffer<>::BUFFER_SIZE;
  SerializationBuffer<> result(buf_size - 1);  // Max content size (reserve 1 for null)

  if (doc_.overflowed()) {
    ESP_LOGE(TAG, "JSON document overflow");
    auto *buf = result.data_writable_();
    buf[0] = '{';
    buf[1] = '}';
    buf[2] = '\0';
    result.set_size_(2);
    return result;
  }

  size_t size = serializeJson(doc_, result.data_writable_(), buf_size);
  if (size < buf_size) {
    // Fits in stack buffer - update size to actual length
    result.set_size_(size);
    return result;
  }

  // Needs heap allocation - reallocate and serialize again with exact size
  result.reallocate_heap_(size);
  serializeJson(doc_, result.data_writable_(), size + 1);
  return result;
}

}  // namespace json
}  // namespace esphome
