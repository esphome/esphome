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
  float get_setup_priority() const override { return setup_priority::DATA; }

  // Called once from codegen with the PROGMEM blob.
  void set_data(const uint8_t *data, size_t size, size_t uncompressed_size) {
    this->data_ = data;
    this->size_ = size;
    this->uncompressed_size_ = uncompressed_size;
  }
  const uint8_t *get_data() const { return this->data_; }
  size_t get_size() const { return this->size_; }
  size_t get_uncompressed_size() const { return this->uncompressed_size_; }

 protected:
  const uint8_t *data_{nullptr};
  size_t size_{0};
  size_t uncompressed_size_{0};
};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
extern StoreYamlComponent *global_store_yaml;

}  // namespace esphome::store_yaml

#endif  // USE_STORE_YAML
