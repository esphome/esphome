#pragma once

#ifdef USE_ARDUINO

#include <vector>

#include "esphome/core/helpers.h"
#include <ArduinoJson.h>

namespace esphome {
namespace json {

/// Callback function typedef for parsing JsonObjects.
using json_parse_t = std::function<void(JsonObject &)>;

/// Callback function typedef for building JsonObjects.
using json_build_t = std::function<void(JsonObject &)>;

/// Build a JSON string with the provided json build function.
const char *build_json(const json_build_t &f, size_t *length);

std::string build_json(const json_build_t &f);

/// Parse a JSON string and run the provided json parse function if it's valid.
void parse_json(const std::string &data, const json_parse_t &f);

class VectorJsonBuffer : public ArduinoJson::Internals::JsonBufferBase<VectorJsonBuffer> {
 public:
  class String {
   public:
    String(VectorJsonBuffer *parent);

    void append(char c) const;

    const char *c_str() const;

   protected:
    VectorJsonBuffer *parent_;
    uint32_t start_;
  };

  void *alloc(size_t bytes) override;

  size_t size() const;

  void clear();

  String startString();  // NOLINT

 protected:
  void *do_alloc(size_t bytes);  // NOLINT

  void resize(size_t size);  // NOLINT

  void reserve(size_t size);  // NOLINT

  char *buffer_{nullptr};
  size_t size_{0};
  size_t capacity_{0};
  std::vector<char *> free_blocks_;
};

extern VectorJsonBuffer global_json_buffer;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace json
}  // namespace esphome

#endif  // USE_ARDUINO
