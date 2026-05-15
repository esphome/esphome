#include "store_yaml.h"

#ifdef USE_STORE_YAML

#include "esphome/core/log.h"

namespace esphome::store_yaml {

static const char *const TAG = "store_yaml";

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
StoreYamlComponent *global_store_yaml = nullptr;

void StoreYamlComponent::setup() { global_store_yaml = this; }

void StoreYamlComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "YAML:");
  ESP_LOGCONFIG(TAG, "  Compressed size: %zu bytes", this->size_);
  ESP_LOGCONFIG(TAG, "  Uncompressed size: %zu bytes", this->uncompressed_size_);
  ESP_LOGCONFIG(TAG, "  Encoding: %s", ENCODING);
}

}  // namespace esphome::store_yaml

#endif  // USE_STORE_YAML
