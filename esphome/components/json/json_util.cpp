#include "json_util.h"
#include "esphome/core/log.h"

namespace esphome {
namespace json {

static const char *const TAG = "json";

static std::vector<char> global_json_build_buffer;  // NOLINT

const char *build_json(const json_build_t &f, size_t *length) {
  global_json_document.clear();
  JsonObject root = global_json_document.to<JsonObject>();

  f(root);

  // The Json buffer size gives us a good estimate for the required size.
  // Usually, it's a bit larger than the actual required string size
  //             | JSON Buffer Size | String Size |
  // Discovery   | 388              | 351         |
  // Discovery   | 372              | 356         |
  // Discovery   | 336              | 311         |
  // Discovery   | 408              | 393         |
  global_json_build_buffer.reserve(root.size() + 1);
  size_t bytes_written = serializeJson(root, global_json_build_buffer.data(), global_json_build_buffer.capacity());

  if (bytes_written >= global_json_build_buffer.capacity() - 1) {
    global_json_build_buffer.reserve(measureJson(root) + 1);
    bytes_written = serializeJson(root, global_json_build_buffer.data(), global_json_build_buffer.capacity());
  }

  *length = bytes_written;
  return global_json_build_buffer.data();
}
void parse_json(const std::string &data, const json_parse_t &f) {
  global_json_document.clear();
  DeserializationError err = deserializeJson(global_json_document, data);

  JsonObject root = global_json_document.as<JsonObject>();

  if (err) {
    ESP_LOGW(TAG, "Parsing JSON failed.");
    return;
  }

  f(root);
}
std::string build_json(const json_build_t &f) {
  size_t len;
  const char *c_str = build_json(f, &len);
  return std::string(c_str, len);
}

DynamicJsonDocument global_json_document(2048);  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace json
}  // namespace esphome
