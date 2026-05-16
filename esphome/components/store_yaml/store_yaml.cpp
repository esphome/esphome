#include "store_yaml.h"

#ifdef USE_STORE_YAML

#include "esphome/core/log.h"
#include <cstring>
#ifdef USE_ESP8266
#include <pgmspace.h>
#endif

namespace esphome::store_yaml {

static const char *const TAG = "store_yaml";

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
StoreYamlComponent *global_store_yaml = nullptr;

void StoreYamlComponent::setup() { global_store_yaml = this; }

void StoreYamlComponent::dump_config() {
  ESP_LOGCONFIG(TAG,
                "YAML:\n"
                "  Compressed size: %zu bytes\n"
                "  Uncompressed size: %zu bytes\n"
                "  Encoding: %s",
                this->size_, this->uncompressed_size_, ENCODING);
}

void StoreYamlComponent::read_chunk(size_t pos, uint8_t *dst, size_t len) const {
#ifdef USE_ESP8266
  // ESP8266 needs `memcpy_P` for aligned bulk flash reads; the byte-by-byte
  // `progmem_read_byte` loop would otherwise emit ~4x as many flash accesses.
  memcpy_P(dst, this->data_ + pos, len);
#else
  // PROGMEM is a no-op everywhere else and the data lives in normal address
  // space, so a plain `std::memcpy` is correct and the fast path.
  std::memcpy(dst, this->data_ + pos, len);
#endif
}

}  // namespace esphome::store_yaml

#endif  // USE_STORE_YAML
