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

// this function can not handle nesting of braces/brackets more than 32 levels deep
int16_t try_find_json(std::span<const unsigned char> buffer) {
  static const uint8_t MAX_JSON_DEPTH = 32;
  typedef std::size_t position_t;
  // brackets and braces are used as a bitset, for indicators of the nesting
  std::bitset<MAX_JSON_DEPTH> brackets, braces;
  uint8_t depth = 0;
  bool inside_string = false;
  bool backslash = false;
  bool corrupt = false;
  bool overflow = false;

  size_t idx = 0;

  for (unsigned char it : buffer) {
    if (it == '{' || it == '[') {
      break;
    }
    idx++;
  }

  if (idx > 0)
    return -idx;

  for (unsigned char it : buffer) {
    if (std::isspace(it)) {
    } else if (!std::isprint(it)) {
      corrupt = true;
    } else if (!inside_string) {
      if (it == '{') {
        braces.set(position_t(depth++), true);
        if (depth > MAX_JSON_DEPTH) {
          overflow = true;
        }
      } else if (it == '}') {
        if (depth > 0 && braces[depth - 1]) {
          braces.set(position_t(--depth), false);
        } else {
          corrupt = true;
        }
      } else if (it == '[') {
        brackets.set(position_t(depth++), true);
        if (depth > MAX_JSON_DEPTH) {
          overflow = true;
        }
      } else if (it == ']') {
        if (depth > 0 && brackets[depth - 1]) {
          brackets.set(position_t(--depth), false);
        } else {
          corrupt = true;
        }
      } else if (it == '"') {
        inside_string = true;
      }
    } else {  // !inside_string
      if (backslash) {
        // any character can be escaped. This ends the escape.
        backslash = false;
      } else if (it == '\\') {
        backslash = true;
      } else if (it == '"') {
        inside_string = false;
      }
    }
    idx++;
    if (overflow || corrupt) {
      return -1;
    } else if (depth == 0) {
      return idx;
    }
  }
  return 0;
}

}  // namespace json
}  // namespace esphome
