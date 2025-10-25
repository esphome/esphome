#ifdef USE_ZEPHYR

#include <zephyr/kernel.h>
#include "esphome/core/preferences.h"
#include "esphome/core/log.h"
#include <zephyr/settings/settings.h>

namespace esphome {
namespace zephyr {

static const char *const TAG = "zephyr.preferences";

#define ESPHOME_SETTINGS_KEY "esphome"

class ZephyrPreferenceBackend : public ESPPreferenceBackend {
 public:
  ZephyrPreferenceBackend(uint32_t type) { this->type_ = type; }
  ZephyrPreferenceBackend(uint32_t type, std::vector<uint8_t> &&data) : data(std::move(data)) { this->type_ = type; }

  bool save(const uint8_t *data, size_t len) override {
    this->data.resize(len);
    std::memcpy(this->data.data(), data, len);
    ESP_LOGVV(TAG, "save key: %u, len: %d", this->type_, len);
    return true;
  }

  bool load(uint8_t *data, size_t len) override {
    if (len != this->data.size()) {
      ESP_LOGE(TAG, "size of setting key %s changed, from: %u, to: %u", get_key().c_str(), this->data.size(), len);
      return false;
    }
    std::memcpy(data, this->data.data(), len);
    ESP_LOGVV(TAG, "load key: %u, len: %d", this->type_, len);
    return true;
  }

  uint32_t get_type() const { return this->type_; }
  std::string get_key() const { return str_sprintf(ESPHOME_SETTINGS_KEY "/%" PRIx32, this->type_); }

  std::vector<uint8_t> data;

 protected:
  uint32_t type_ = 0;
};

class ZephyrPreferences : public ESPPreferences {
 public:
  void open() {
    int err = settings_subsys_init();
    if (err) {
      ESP_LOGE(TAG, "Failed to initialize settings subsystem, err: %d", err);
      return;
    }

    static struct settings_handler settings_cb = {
        .name = ESPHOME_SETTINGS_KEY,
        .h_set = load_setting,
        .h_export = export_settings,
    };

    err = settings_register(&settings_cb);
    if (err) {
      ESP_LOGE(TAG, "setting_register failed, err, %d", err);
      return;
    }

    err = settings_load_subtree(ESPHOME_SETTINGS_KEY);
    if (err) {
      ESP_LOGE(TAG, "Cannot load settings, err: %d", err);
      return;
    }
    ESP_LOGD(TAG, "Loaded %u settings.", this->backends_.size());
  }

  ESPPreferenceObject make_preference(size_t length, uint32_t type, bool in_flash) override {
    return make_preference(length, type);
  }

  ESPPreferenceObject make_preference(size_t length, uint32_t type) override {
    for (auto *backend : this->backends_) {
      if (backend->get_type() == type) {
        return ESPPreferenceObject(backend);
      }
    }
    printf("type %u size %u\n", type, this->backends_.size());
    auto *pref = new ZephyrPreferenceBackend(type);  // NOLINT(cppcoreguidelines-owning-memory)
    ESP_LOGD(TAG, "Add new setting %s.", pref->get_key().c_str());
    this->backends_.push_back(pref);
    return ESPPreferenceObject(pref);
  }

  bool sync() override {
    ESP_LOGD(TAG, "Save settings");
    int err = settings_save();
    if (err) {
      ESP_LOGE(TAG, "Cannot save settings, err: %d", err);
      return false;
    }
    return true;
  }

  bool reset() override {
    ESP_LOGD(TAG, "Reset settings");
    for (auto *backend : this->backends_) {
      // save empty delete data
      backend->data.clear();
    }
    sync();
    return true;
  }

 protected:
  std::vector<ZephyrPreferenceBackend *> backends_;

  static int load_setting(const char *name, size_t len, settings_read_cb read_cb, void *cb_arg) {
    auto type = parse_hex<uint32_t>(name);
    if (!type.has_value()) {
      std::string full_name(ESPHOME_SETTINGS_KEY);
      full_name += "/";
      full_name += name;
      // Delete unusable keys. Otherwise it will stay in flash forever.
      settings_delete(full_name.c_str());
      return 1;
    }
    std::vector<uint8_t> data(len);
    int err = read_cb(cb_arg, data.data(), len);

    ESP_LOGD(TAG, "load setting, name: %s(%u), len %u, err %u", name, *type, len, err);
    auto *pref = new ZephyrPreferenceBackend(*type, std::move(data));  // NOLINT(cppcoreguidelines-owning-memory)
    static_cast<ZephyrPreferences *>(global_preferences)->backends_.push_back(pref);
    return 0;
  }

  static int export_settings(int (*cb)(const char *name, const void *value, size_t val_len)) {
    for (auto *backend : static_cast<ZephyrPreferences *>(global_preferences)->backends_) {
      auto name = backend->get_key();
      int err = cb(name.c_str(), backend->data.data(), backend->data.size());
      ESP_LOGD(TAG, "save in flash, name %s, len %u, err %d", name.c_str(), backend->data.size(), err);
    }
    return 0;
  }
};

void setup_preferences() {
  auto *prefs = new ZephyrPreferences();  // NOLINT(cppcoreguidelines-owning-memory)
  global_preferences = prefs;
  prefs->open();
}

}  // namespace zephyr

ESPPreferences *global_preferences;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esphome

#endif
