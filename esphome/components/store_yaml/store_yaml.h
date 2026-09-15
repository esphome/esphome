#pragma once

#include "esphome/core/defines.h"
#ifdef USE_STORE_YAML

#include "esphome/core/component.h"
#include "esphome/core/hal.h"

namespace esphome::store_yaml {

// "zstd" — published in GetYamlResponse.encoding so clients know how to decompress.
constexpr const char *ENCODING = "zstd";

class StoreYamlComponent : public Component {
 public:
  void setup() override;
  void dump_config() override;

  // Called once from codegen with the PROGMEM blob.
  void set_data(const uint8_t *data, size_t size, size_t uncompressed_size) {
    this->data_ = data;
    this->size_ = size;
    this->uncompressed_size_ = uncompressed_size;
  }
  size_t get_size() const { return this->size_; }

  // Raw pointer to the PROGMEM blob. On ESP8266 the address is in instruction
  // flash and must be read via `progmem_memcpy`; everywhere else PROGMEM is a
  // no-op and the data is directly addressable.
  const uint8_t *get_data() const { return this->data_; }

 protected:
  // Points to a `const uint8_t[] PROGMEM` array emitted by codegen.
  const uint8_t *data_{nullptr};
  size_t size_{0};
  size_t uncompressed_size_{0};
};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
extern StoreYamlComponent *global_store_yaml;

}  // namespace esphome::store_yaml

#endif  // USE_STORE_YAML
