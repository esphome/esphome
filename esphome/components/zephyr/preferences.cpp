#ifdef USE_ZEPHYR

#include "esphome/core/preferences.h"
#include "esphome/core/log.h"

namespace esphome {
namespace zephyr {

static const char *const TAG = "esp32.preferences";

struct NVSData {
  std::string key;
  std::vector<uint8_t> data;
};

static std::vector<NVSData> s_pending_save;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

class ZephyrPreferenceBackend : public ESPPreferenceBackend {
 public:
  std::string key;
  // uint32_t nvs_handle;
  bool save(const uint8_t *data, size_t len) override {
    // try find in pending saves and update that
    for (auto &obj : s_pending_save) {
      if (obj.key == key) {
        obj.data.assign(data, data + len);
        return true;
      }
    }
    NVSData save{};
    save.key = key;
    save.data.assign(data, data + len);
    s_pending_save.emplace_back(save);
    ESP_LOGVV(TAG, "s_pending_save: key: %s, len: %d", key.c_str(), len);
    return true;
  }

  bool load(uint8_t *data, size_t len) override {
    // try find in pending saves and load from that
    for (auto &obj : s_pending_save) {
      if (obj.key == key) {
        if (obj.data.size() != len) {
          // size mismatch
          return false;
        }
        memcpy(data, obj.data.data(), len);
        return true;
      }
    }

  // TODO

  //   size_t actual_len;
  //   esp_err_t err = nvs_get_blob(nvs_handle, key.c_str(), nullptr, &actual_len);
  //   if (err != 0) {
  //     ESP_LOGV(TAG, "nvs_get_blob('%s'): %s - the key might not be set yet", key.c_str(), esp_err_to_name(err));
  //     return false;
  //   }
  //   if (actual_len != len) {
  //     ESP_LOGVV(TAG, "NVS length does not match (%u!=%u)", actual_len, len);
  //     return false;
  //   }
  //   err = nvs_get_blob(nvs_handle, key.c_str(), data, &len);
  //   if (err != 0) {
  //     ESP_LOGV(TAG, "nvs_get_blob('%s') failed: %s", key.c_str(), esp_err_to_name(err));
  //     return false;
  //   } else {
  //     ESP_LOGVV(TAG, "nvs_get_blob: key: %s, len: %d", key.c_str(), len);
  //   }
    return true;
  }
};

class ZephyrPreferences : public ESPPreferences {
 public:
  ESPPreferenceObject make_preference(size_t length, uint32_t type, bool in_flash) override {
    return make_preference(length, type);
  }

  ESPPreferenceObject make_preference(size_t length, uint32_t type) override {
    auto *pref = new ZephyrPreferenceBackend();  // NOLINT(cppcoreguidelines-owning-memory)
    // pref->nvs_handle = nvs_handle;

    uint32_t keyval = type;
    pref->key = str_sprintf("%" PRIu32, keyval);

    return ESPPreferenceObject(pref);
  }

  bool sync() override {
    // TODO
    return true;
  }

  bool reset() override {
    // TODO
    return true;
  }
};

void setup_preferences() {
  auto *prefs = new ZephyrPreferences();  // NOLINT(cppcoreguidelines-owning-memory)
  global_preferences = prefs;
}

}  // namespace zephyr

ESPPreferences *global_preferences;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esphome

#endif
