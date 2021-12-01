#pragma once

#include <vector>

#include "esphome/core/helpers.h"

#undef ARDUINOJSON_ENABLE_STD_STRING
#define ARDUINOJSON_ENABLE_STD_STRING 1

#include <ArduinoJson.h>

namespace esphome {
namespace json {

/// Callback function typedef for parsing JsonObjects.
using json_parse_t = std::function<void(JsonObject)>;

/// Callback function typedef for building JsonObjects.
using json_build_t = std::function<void(JsonObject)>;

/// Build a JSON string with the provided json build function.
const char *build_json(const json_build_t &f, size_t *length);

std::string build_json(const json_build_t &f);

/// Parse a JSON string and run the provided json parse function if it's valid.
void parse_json(const std::string &data, const json_parse_t &f);

extern DynamicJsonDocument global_json_document;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace json
}  // namespace esphome
