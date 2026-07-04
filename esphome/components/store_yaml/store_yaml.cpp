#include "store_yaml.h"

#ifdef USE_STORE_YAML

#include "esphome/core/log.h"

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

}  // namespace esphome::store_yaml

#endif  // USE_STORE_YAML
