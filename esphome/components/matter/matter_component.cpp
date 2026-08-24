#include "esphome/core/defines.h"

// USE_MATTER_VARIANT_SUPPORTED is set by matter's Python to_code() via
// cg.add_define() on the 5 esp-matter-supported ESP32 variants (ESP32,
// S3, C3, C6, H2). It is deliberately NOT declared in
// esphome/core/defines.h — that path is only exercised by clang-tidy and
// static-analysis tools, which do not have esp_matter.h available (the
// SDK is a third-party managed component fetched at build time). Keeping
// the symbol out of defines.h means matter files strip on lint (no
// missing-header errors) but compile normally on real builds where
// Python-side codegen has run. Runtime variant enforcement lives in the
// only_on_variant validator in matter/__init__.py.
#if defined(USE_ESP_IDF) && defined(USE_MATTER_VARIANT_SUPPORTED)

#include "matter_component.h"

#include "esphome/core/log.h"
#include "esphome/core/application.h"

#include <esp_matter.h>
#include <esp_matter_providers.h>
#include <esp_wifi.h>
#include <nvs.h>

#ifdef USE_WIFI
#include "esphome/components/wifi/wifi_component.h"
#endif
#include <setup_payload/ManualSetupPayloadGenerator.h>
#include <setup_payload/QRCodeSetupPayloadGenerator.h>
#include <setup_payload/SetupPayload.h>

#include <app-common/zap-generated/cluster-objects.h>
#include <app/reporting/reporting.h>
#include <app/server/CommissioningWindowManager.h>
#include <app/server/Server.h>
#include <lib/support/attribute-storage-null-handling.h>
#include <crypto/CHIPCryptoPAL.h>
#include <lib/support/CHIPMemString.h>
#include <platform/CHIPDeviceLayer.h>
#include <platform/DeviceInfoProvider.h>
#include <platform/DeviceInstanceInfoProvider.h>
#ifdef USE_WIFI
#include <platform/ESP32/PlatformManagerImpl.h>
#include <esp_event.h>
#endif

#include <atomic>
#include <cstdio>
#include <cstring>

#ifdef USE_SWITCH
#include "matter_switch_endpoint.h"
#include "esphome/components/switch/switch.h"
#endif
#ifdef USE_BINARY_SENSOR
#include "matter_binary_sensor_endpoint.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#ifdef USE_SENSOR
#include "matter_sensor_endpoint.h"
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_COVER
#include "matter_cover_endpoint.h"
#include "esphome/components/cover/cover.h"
#endif
#ifdef USE_FAN
#include "matter_fan_endpoint.h"
#include "esphome/components/fan/fan.h"
#endif
#ifdef USE_LOCK
#include "matter_lock_endpoint.h"
#include "esphome/components/lock/lock.h"
#endif
#ifdef USE_VALVE
#include "matter_valve_endpoint.h"
#include "esphome/components/valve/valve.h"
#endif
#ifdef USE_SELECT
#include "matter_select_endpoint.h"
#include "esphome/components/select/select.h"
#endif
#ifdef USE_BUTTON
#include "matter_button_endpoint.h"
#include "matter_action_button.h"
#include "esphome/components/button/button.h"
#endif
#ifdef USE_LIGHT
#include "matter_light_endpoint.h"
#include "esphome/components/light/light_state.h"
#endif
#ifdef USE_CLIMATE
#include "matter_climate_endpoint.h"
#include "esphome/components/climate/climate.h"
#endif

namespace esphome::matter {

static const char *const TAG = "matter";

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
MatterComponent *MatterComponent::global_matter_component = nullptr;

namespace {

// Linear lookup over the endpoint→wrapper vectors. Entity counts are small
// (typically 1–30 per platform), so a linear scan beats the ~2KB fixed
// overhead of std::unordered_map — see the header comment on the *_by_id_
// vectors.
template<typename T> T *find_endpoint_by_id(const FixedVector<std::pair<uint16_t, T *>> &vec, uint16_t endpoint_id) {
  for (const auto &kv : vec) {
    if (kv.first == endpoint_id) {
      return kv.second;
    }
  }
  return nullptr;
}

}  // namespace

namespace {

// FixedLabel iterator scoped to a single endpoint. Serves at most one entry
// ("name" → truncated entity name) — we do not expose multiple labels per
// endpoint. Storage stays alive for the whole iterator lifetime because the
// backing string is copied into `label_`; CharSpan just references it.
// Method and parameter names in the classes below are dictated by the CHIP
// DeviceInfoProvider / DeviceInstanceInfoProvider virtual interfaces, which
// use UpperCamelCase methods and camelCase parameter names. Renaming to
// match ESPHome conventions would break the override contract, so suppress
// clang-tidy's naming check across these classes.
// NOLINTBEGIN(readability-identifier-naming)
class EndpointFixedLabelIterator : public chip::DeviceLayer::DeviceInfoProvider::FixedLabelIterator {
 public:
  EndpointFixedLabelIterator(const FixedVector<std::pair<uint16_t, std::string>> &labels, chip::EndpointId endpoint) {
    for (const auto &pair : labels) {
      if (pair.first == endpoint) {
        this->label_ = pair.second;
        break;
      }
    }
  }
  size_t Count() override { return this->label_.empty() ? 0 : 1; }
  bool Next(chip::DeviceLayer::DeviceInfoProvider::FixedLabelType &item) override {
    if (this->label_.empty() || this->emitted_) {
      return false;
    }
    item.label = chip::CharSpan::fromCharString("name");
    item.value = chip::CharSpan(this->label_.data(), this->label_.size());
    this->emitted_ = true;
    return true;
  }
  void Release() override { chip::Platform::Delete(this); }

 private:
  std::string label_;
  bool emitted_ = false;
};

// Empty iterator that satisfies the DeviceInfoProvider interface without
// exposing any data. Used for SupportedLocales / SupportedCalendarTypes
// which we do not populate.
template<typename T> class EmptyDeviceInfoIterator : public chip::DeviceLayer::DeviceInfoProvider::Iterator<T> {
 public:
  size_t Count() override { return 0; }
  bool Next(T &) override { return false; }
  void Release() override { chip::Platform::Delete(this); }
};

// UserLabel iterator scoped to one endpoint. Copies the endpoint's entries
// into local storage at construction time so mutations to the source
// (fabric write racing with a read on the same task — shouldn't happen since
// CHIP serializes, but defensive) don't corrupt the iteration.
class EndpointUserLabelIterator : public chip::DeviceLayer::DeviceInfoProvider::UserLabelIterator {
 public:
  explicit EndpointUserLabelIterator(const std::vector<MatterComponent::UserLabelEntry> *entries) {
    if (entries != nullptr) {
      this->entries_ = *entries;
    }
  }
  size_t Count() override { return this->entries_.size(); }
  bool Next(chip::DeviceLayer::DeviceInfoProvider::UserLabelType &item) override {
    if (this->index_ >= this->entries_.size()) {
      return false;
    }
    const auto &entry = this->entries_[this->index_];
    item.label = chip::CharSpan(entry.label.data(), entry.label.size());
    item.value = chip::CharSpan(entry.value.data(), entry.value.size());
    this->index_++;
    return true;
  }
  void Release() override { chip::Platform::Delete(this); }

 private:
  std::vector<MatterComponent::UserLabelEntry> entries_;
  size_t index_ = 0;
};

// In-memory DeviceInfoProvider that serves FixedLabel entries from a vector
// of (endpoint_id, name) pairs owned by the MatterComponent, and delegates
// UserLabel reads/writes to the MatterComponent (which owns the map and the
// NVS persistence). Registered via esp_matter::set_custom_device_info_provider()
// so it wins over the default (which would need
// ENABLE_ESP32_FACTORY_DATA_PROVIDER=y and therefore break the EXAMPLE
// Commissionable/DAC providers we depend on).
class MatterDeviceInfoProvider : public chip::DeviceLayer::DeviceInfoProvider {
 public:
  const FixedVector<std::pair<uint16_t, std::string>> *labels = nullptr;
  MatterComponent *component = nullptr;

  FixedLabelIterator *IterateFixedLabel(chip::EndpointId endpoint) override {
    if (this->labels == nullptr) {
      return chip::Platform::New<EmptyDeviceInfoIterator<FixedLabelType>>();
    }
    return chip::Platform::New<EndpointFixedLabelIterator>(*this->labels, endpoint);
  }
  UserLabelIterator *IterateUserLabel(chip::EndpointId endpoint) override {
    const std::vector<MatterComponent::UserLabelEntry> *entries =
        (this->component != nullptr) ? this->component->user_labels_for(endpoint) : nullptr;
    return chip::Platform::New<EndpointUserLabelIterator>(entries);
  }
  SupportedLocalesIterator *IterateSupportedLocales() override {
    return chip::Platform::New<EmptyDeviceInfoIterator<chip::CharSpan>>();
  }
  SupportedCalendarTypesIterator *IterateSupportedCalendarTypes() override {
    return chip::Platform::New<EmptyDeviceInfoIterator<CalendarType>>();
  }

 protected:
  // CHIP hands us CharSpans backed by decode buffers valid only during the
  // call — copy the bytes into std::string before storing so the entries
  // survive past this frame.
  CHIP_ERROR SetUserLabelAt(chip::EndpointId endpoint, size_t index, const UserLabelType &userLabel) override {
    if (this->component == nullptr) {
      return CHIP_ERROR_UNINITIALIZED;
    }
    std::string label(userLabel.label.data(), userLabel.label.size());
    std::string value(userLabel.value.data(), userLabel.value.size());
    if (this->component->set_user_label_at(endpoint, index, label, value) != 0) {
      return CHIP_ERROR_PERSISTED_STORAGE_FAILED;
    }
    return CHIP_NO_ERROR;
  }
  CHIP_ERROR DeleteUserLabelAt(chip::EndpointId endpoint, size_t index) override {
    if (this->component == nullptr) {
      return CHIP_ERROR_UNINITIALIZED;
    }
    if (this->component->delete_user_label_at(endpoint, index) != 0) {
      return CHIP_ERROR_PERSISTED_STORAGE_FAILED;
    }
    return CHIP_NO_ERROR;
  }
  CHIP_ERROR SetUserLabelLength(chip::EndpointId endpoint, size_t val) override {
    if (this->component == nullptr) {
      return CHIP_ERROR_UNINITIALIZED;
    }
    if (this->component->set_user_label_length(endpoint, val) != 0) {
      return CHIP_ERROR_PERSISTED_STORAGE_FAILED;
    }
    return CHIP_NO_ERROR;
  }
  CHIP_ERROR GetUserLabelLength(chip::EndpointId endpoint, size_t &val) override {
    val = (this->component != nullptr) ? this->component->user_label_count_for(endpoint) : 0;
    return CHIP_NO_ERROR;
  }
};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
MatterDeviceInfoProvider s_device_info_provider;

// Serves vendor/product name + version strings from the YAML-configured
// values on MatterComponent. Without this, CHIP's GenericDeviceInstanceInfoProvider
// hardcodes returns to compile-time constants ("TEST_VENDOR" / "TEST_PRODUCT"
// from CHIPDeviceConfig.h) — so the controller sees those regardless of what
// the user wrote in the yaml.
class MatterDeviceInstanceInfoProvider : public chip::DeviceLayer::DeviceInstanceInfoProvider {
 public:
  // Owning pointers filled by install_device_info_provider_() — vector of
  // strings on the MatterComponent, alive for the whole app lifetime.
  const std::string *vendor_name = nullptr;
  const std::string *product_name = nullptr;
  const std::string *hardware_version_string = nullptr;
  uint16_t vendor_id = 0;
  uint16_t product_id = 0;

  CHIP_ERROR GetVendorName(char *buf, size_t bufSize) override {
    return this->copy_string_(this->vendor_name, buf, bufSize);
  }
  CHIP_ERROR GetVendorId(uint16_t &vendorId) override {
    vendorId = this->vendor_id;
    return CHIP_NO_ERROR;
  }
  CHIP_ERROR GetProductName(char *buf, size_t bufSize) override {
    return this->copy_string_(this->product_name, buf, bufSize);
  }
  CHIP_ERROR GetProductId(uint16_t &productId) override {
    productId = this->product_id;
    return CHIP_NO_ERROR;
  }
  // The rest of the DeviceInstanceInfoProvider interface — return
  // NOT_IMPLEMENTED for optional fields. CHIP tolerates this and just omits
  // the corresponding BasicInformation attributes from responses.
  CHIP_ERROR GetPartNumber(char *, size_t) override { return CHIP_ERROR_NOT_IMPLEMENTED; }
  CHIP_ERROR GetProductURL(char *, size_t) override { return CHIP_ERROR_NOT_IMPLEMENTED; }
  CHIP_ERROR GetProductLabel(char *, size_t) override { return CHIP_ERROR_NOT_IMPLEMENTED; }
  CHIP_ERROR GetSerialNumber(char *, size_t) override { return CHIP_ERROR_NOT_IMPLEMENTED; }
  CHIP_ERROR GetManufacturingDate(uint16_t &, uint8_t &, uint8_t &) override { return CHIP_ERROR_NOT_IMPLEMENTED; }
  CHIP_ERROR GetHardwareVersion(uint16_t &hardwareVersion) override {
    hardwareVersion = 0;
    return CHIP_NO_ERROR;
  }
  CHIP_ERROR GetHardwareVersionString(char *buf, size_t bufSize) override {
    return this->copy_string_(this->hardware_version_string, buf, bufSize);
  }
  CHIP_ERROR GetRotatingDeviceIdUniqueId(chip::MutableByteSpan &) override { return CHIP_ERROR_NOT_IMPLEMENTED; }

 private:
  static CHIP_ERROR copy_string_(const std::string *src, char *buf, size_t bufSize) {
    if (src == nullptr || src->empty()) {
      return CHIP_ERROR_NOT_IMPLEMENTED;
    }
    if (src->size() + 1 > bufSize) {
      return CHIP_ERROR_BUFFER_TOO_SMALL;
    }
    std::memcpy(buf, src->data(), src->size());
    buf[src->size()] = '\0';
    return CHIP_NO_ERROR;
  }
};
// NOLINTEND(readability-identifier-naming)

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
MatterDeviceInstanceInfoProvider s_device_instance_info_provider;

}  // namespace

static esp_err_t attribute_update_cb(::esp_matter::attribute::callback_type_t type, uint16_t endpoint_id,
                                     uint32_t cluster_id, uint32_t attribute_id, ::esp_matter_attr_val_t *val,
                                     void *priv_data) {
  // Only act on PRE_UPDATE — that's when the fabric is applying a write and
  // we can either honor it (drive the ESPHome entity) or veto by returning
  // an error. POST_UPDATE is echo; READ we don't need to interpose.
  if (type != ::esp_matter::attribute::PRE_UPDATE) {
    return ESP_OK;
  }
  MatterComponent *self = MatterComponent::instance();
  if (self == nullptr || val == nullptr) {
    return ESP_OK;
  }
  // Each handled cluster/attribute path calls the corresponding dispatcher
  // and translates a dispatch miss (endpoint not owned by us for a cluster
  // we intercept) into ESP_ERR_NOT_FOUND so CHIP does not silently accept a
  // write that never reached an ESPHome entity. Writes to attributes we do
  // not intercept fall through to the final `return ESP_OK`.
  bool dispatched = true;
  bool intercepted = false;
  // OnOff.OnOff (bool) — switch endpoints.
  if (cluster_id == chip::app::Clusters::OnOff::Id &&
      attribute_id == chip::app::Clusters::OnOff::Attributes::OnOff::Id) {
    intercepted = true;
    dispatched = self->handle_attribute_write(endpoint_id, cluster_id, attribute_id, val->val.b);
  }
  // WindowCovering.TargetPositionLiftPercent100ths (nullable uint16) — cover
  // endpoints. The CHIP window-covering server writes this attribute in
  // response to UpOrOpen / DownOrClose / GoToLiftPercentage commands, so
  // hooking the attribute path covers all three without a command delegate.
  else if (cluster_id == chip::app::Clusters::WindowCovering::Id &&
           attribute_id == chip::app::Clusters::WindowCovering::Attributes::TargetPositionLiftPercent100ths::Id) {
    intercepted = true;
    // NullableTraits sentinel for uint16 is 0xFFFF; the fabric shouldn't send
    // that as a real position but guard anyway so a bad write doesn't drive
    // the cover to an out-of-range target.
    if (!chip::app::NumericAttributeTraits<uint16_t>::IsNullValue(val->val.u16)) {
      dispatched = self->handle_cover_target_write(endpoint_id, val->val.u16);
    }
  }
  // FanControl attributes — writes come from the fabric setting speed or
  // fan mode. PercentSetting is a nullable uint8; treat null as "no change"
  // (fabric never sends null explicitly).
  else if (cluster_id == chip::app::Clusters::FanControl::Id) {
    if (attribute_id == chip::app::Clusters::FanControl::Attributes::PercentSetting::Id) {
      intercepted = true;
      if (!chip::app::NumericAttributeTraits<uint8_t>::IsNullValue(val->val.u8)) {
        dispatched = self->handle_fan_percent_write(endpoint_id, val->val.u8);
      }
    } else if (attribute_id == chip::app::Clusters::FanControl::Attributes::FanMode::Id) {
      intercepted = true;
      dispatched = self->handle_fan_mode_write(endpoint_id, val->val.u8);
    }
  }
  // ModeSelect.CurrentMode (uint8) — mode_select endpoints. CHIP's
  // mode-select-server writes CurrentMode via Attributes::CurrentMode::Set
  // after validating the incoming ChangeToMode command via the
  // SupportedModesManager, so hooking the attribute path covers the command.
  else if (cluster_id == chip::app::Clusters::ModeSelect::Id &&
           attribute_id == chip::app::Clusters::ModeSelect::Attributes::CurrentMode::Id) {
    intercepted = true;
    dispatched = self->handle_select_current_mode_write(endpoint_id, val->val.u8);
  }
  // LevelControl.CurrentLevel — light endpoints (dimmable+). CHIP's
  // level-control server writes here from MoveToLevel / MoveToLevelWithOnOff
  // / Step / Move commands.
  else if (cluster_id == chip::app::Clusters::LevelControl::Id &&
           attribute_id == chip::app::Clusters::LevelControl::Attributes::CurrentLevel::Id) {
    intercepted = true;
    // CurrentLevel is nullable uint8. Skip null writes — for lights those
    // would be indeterminate anyway.
    if (!chip::app::NumericAttributeTraits<uint8_t>::IsNullValue(val->val.u8)) {
      dispatched = self->handle_light_level_write(endpoint_id, val->val.u8);
    }
  }
  // ColorControl.ColorTemperatureMireds — CT+ light endpoints. CHIP writes
  // here from MoveToColorTemperature / MoveColorTemperature / StepColorTemperature.
  else if (cluster_id == chip::app::Clusters::ColorControl::Id &&
           attribute_id == chip::app::Clusters::ColorControl::Attributes::ColorTemperatureMireds::Id) {
    intercepted = true;
    dispatched = self->handle_light_color_temp_write(endpoint_id, val->val.u16);
  }
  // Thermostat cluster — three writable attributes surface here. SystemMode
  // is enum8 → uint8; the two setpoints are non-nullable int16 hundredths of
  // °C so we forward val->val.i16 directly. SetpointRaiseLower is a command,
  // handled by CHIP's thermostat server which writes the setpoint attribute
  // back to us here — no separate command hook needed for the MVP.
  else if (cluster_id == chip::app::Clusters::Thermostat::Id) {
    if (attribute_id == chip::app::Clusters::Thermostat::Attributes::SystemMode::Id) {
      intercepted = true;
      dispatched = self->handle_climate_system_mode_write(endpoint_id, val->val.u8);
    } else if (attribute_id == chip::app::Clusters::Thermostat::Attributes::OccupiedHeatingSetpoint::Id) {
      intercepted = true;
      dispatched = self->handle_climate_heating_setpoint_write(endpoint_id, val->val.i16);
    } else if (attribute_id == chip::app::Clusters::Thermostat::Attributes::OccupiedCoolingSetpoint::Id) {
      intercepted = true;
      dispatched = self->handle_climate_cooling_setpoint_write(endpoint_id, val->val.i16);
    }
  }
  if (intercepted && !dispatched) {
    // Intercepted cluster+attribute but no ESPHome endpoint owns it: signal
    // the error to CHIP so the write is not silently applied. The handler
    // already logged a WARN with the concrete endpoint id.
    return ESP_ERR_NOT_FOUND;
  }
  return ESP_OK;
}

#ifdef USE_WIFI
// Copy the fabric-provisioned Wi-Fi credentials from the ESP-IDF WiFi driver
// into ESPHome's own SavedWifiSettings preference so the next boot's
// WiFiComponent::start() picks them up via its pref_.load() path and
// dynamically populates sta_. Without this, a yaml with `wifi: ap:` only
// (typical for BLE-commissioned devices) has has_sta()==false at boot and
// forces the wifi component into WIFI_COMPONENT_STATE_AP even though CHIP
// already persisted the credentials into nvs.net80211 via
// esp_wifi_set_config() at commissioning time.
//
// Deferred onto the ESPHome main loop because matter_event_cb runs in
// CHIP's event-loop task, and preferences save + WiFiComponent::set_sta /
// connect_soon_ touch main-thread state.
static void persist_ble_provisioned_wifi_creds_() {
  MatterComponent *self = MatterComponent::instance();
  if (self == nullptr) {
    return;
  }
  self->defer_on_main_loop([]() {
    // Read credentials from the ESP-IDF WiFi driver's runtime config —
    // ``esp_wifi_get_config`` returns exactly the ``wifi_config_t`` that
    // CHIP's NetworkCommissioning delegate set via ``esp_wifi_set_config``
    // during BLE commissioning. On IDF 5.x this returns the passphrase
    // verbatim (verified with a hex dump: byte-for-byte match against the
    // known-good WPA2-PSK). We do NOT read ``nvs.net80211/sta.pswd`` as
    // a fallback because ESPHome's ``wifi:`` component has set the driver
    // storage mode to ``WIFI_STORAGE_RAM`` (see ``matter::setup``
    // rationale) and we intentionally leave it that way, so that NVS
    // blob is never populated in the first place.
    wifi_config_t conf = {};
    esp_err_t err = esp_wifi_get_config(WIFI_IF_STA, &conf);
    if (err != ESP_OK) {
      ESP_LOGW(TAG,
               "esp_wifi_get_config(STA) failed: %s — Wi-Fi credentials "
               "will not persist across reboot",
               esp_err_to_name(err));
      return;
    }
    if (conf.sta.ssid[0] == '\0') {
      ESP_LOGW(TAG, "post-commissioning STA config has empty SSID — nothing to persist");
      return;
    }
    if (wifi::global_wifi_component == nullptr) {
      ESP_LOGW(TAG, "wifi:global_wifi_component null — cannot persist BLE-provisioned credentials");
      return;
    }
    const std::string ssid(reinterpret_cast<const char *>(conf.sta.ssid),
                           strnlen(reinterpret_cast<const char *>(conf.sta.ssid), sizeof(conf.sta.ssid)));
    const std::string password(reinterpret_cast<const char *>(conf.sta.password),
                               strnlen(reinterpret_cast<const char *>(conf.sta.password), sizeof(conf.sta.password)));

    ESP_LOGI(TAG,
             "persisting BLE-provisioned Wi-Fi credentials to ESPHome preferences "
             "(ssid='%s' pw_len=%u)",
             ssid.c_str(), static_cast<unsigned>(password.length()));
    if (password.empty()) {
      ESP_LOGW(TAG, "post-commissioning Wi-Fi password came back empty from "
                    "esp_wifi_get_config — the next boot will attempt to "
                    "associate without a passphrase and likely fail");
    }
    // Write to the SavedWifiSettings preference slot directly, WITHOUT
    // going through wifi::WiFiComponent::save_wifi_sta — that path also
    // calls set_sta() + connect_soon_(), which pushes ESPHome's wifi
    // component into a station-connect state machine that races with
    // CHIP's ConnectivityManager on the current boot. CHIP has the
    // driver associated with the just-commissioned SSID; ESPHome's
    // subsequent esp_wifi_disconnect + esp_wifi_set_config drops that
    // association ("Association Leave" in the logs) and the two stacks
    // start fighting for STA control.
    //
    // The next-boot restore path in WiFiComponent::start() reads this
    // same slot (hash=kMagic when the yaml has no `networks:` block,
    // which is the standard BLE-commissioned shape) and populates sta_
    // dynamically BEFORE the wifi component's state machine engages —
    // no race, no fight. See wifi_component.cpp:648-673 for the read
    // path.
    constexpr uint32_t kMagic = 88491487UL;
    auto pref = global_preferences->make_preference<wifi::SavedWifiSettings>(kMagic, true);
    wifi::SavedWifiSettings save{};
    strncpy(save.ssid, ssid.c_str(), sizeof(save.ssid) - 1);
    strncpy(save.password, password.c_str(), sizeof(save.password) - 1);
    pref.save(&save);
    global_preferences->sync();
  });
}
#endif  // USE_WIFI

static void matter_event_cb(const ChipDeviceEvent *event, intptr_t /*arg*/) {
  switch (event->Type) {
    case chip::DeviceLayer::DeviceEventType::kInterfaceIpAddressChanged:
      ESP_LOGI(TAG, "matter interface ip changed");
      break;
    case chip::DeviceLayer::DeviceEventType::kCommissioningComplete:
      // Do NOT reboot here, even though the BLE-deinit path is about to
      // hit a spinlock crash on ESP32-S3 (btdm_controller_task →
      // semphr_take_wrapper → vPortExitCritical → assert lock->count < 0x100).
      //
      // Earlier iterations of this handler called `esp_restart()` right
      // after CommissioningComplete to (a) skip the crashy BLE deinit and
      // (b) guarantee NVS commits landed. Both concerns turned out to be
      // safe to ignore:
      //
      //   - Wi-Fi credentials now persist via `esp_wifi_set_storage(FLASH)`
      //     that we set at the top of MatterComponent::setup(), so we no
      //     longer need the reboot to protect against a partial commit.
      //   - The spinlock crash lands ~7ms AFTER the BLE GAP is terminated
      //     cleanly, so the peer controller has already seen the BLE
      //     session close normally by the time we panic. The panic itself
      //     triggers a reboot in ~500ms, and the fabric + Wi-Fi survive it.
      //
      // The reason we can't reboot proactively: iOS-side commissioners
      // (Apple Home, and SmartLife/Tuya which hands off to Apple Home for
      // the BLE PASE phase, then does its own CASE handshake over Wi-Fi
      // *after* the phone-side "Generating Matter credentials" step) need
      // a live device on Wi-Fi in the seconds immediately following
      // CommissioningComplete. A 1s proactive reboot preempts that CASE
      // handshake and the app reports "Failed to add the device". HA is
      // more tolerant and accepts the device on the strength of the mDNS
      // operational advertisement alone, which is why we didn't catch this
      // before switching commissioners.
      //
      // Result: this branch is intentionally just an informational log.
      // If the spinlock crash later becomes a real problem (e.g. blocks a
      // stricter commissioner), the fix belongs at the esp-matter/CHIP
      // BLE-deinit level, not here.
      ESP_LOGI(TAG, "commissioning complete");
#ifdef USE_WIFI
      // Copy the fabric-provisioned Wi-Fi credentials into ESPHome's
      // SavedWifiSettings preference so the next boot's
      // WiFiComponent::start() reads them and populates sta_ dynamically —
      // without this a yaml with only `wifi: ap:` (typical for BLE
      // commissioning) has has_sta()==false at boot and never triggers
      // a station connect, even though the credentials were written to
      // nvs.net80211 by CHIP during commissioning.
      persist_ble_provisioned_wifi_creds_();
#endif
      break;
    case chip::DeviceLayer::DeviceEventType::kFabricRemoved:
      ESP_LOGW(TAG, "fabric removed — device is no longer commissioned");
      break;
    default:
      break;
  }
}

MatterComponent::MatterComponent(uint16_t vendor_id, uint16_t product_id, std::string setup_code,
                                 uint16_t discriminator, bool ble_commissioning, std::string vendor_name,
                                 std::string product_name, std::string node_label, std::string hardware_version_string,
                                 std::string software_version_string, bool composed_topology)
    : vendor_id_(vendor_id),
      product_id_(product_id),
      setup_code_(std::move(setup_code)),
      discriminator_(discriminator),
      ble_commissioning_(ble_commissioning),
      vendor_name_(std::move(vendor_name)),
      product_name_(std::move(product_name)),
      node_label_(std::move(node_label)),
      hardware_version_string_(std::move(hardware_version_string)),
      software_version_string_(std::move(software_version_string)),
      composed_topology_(composed_topology) {
  MatterComponent::global_matter_component = this;
}

void MatterComponent::write_factory_strings_() {
  // CHIP's ESP32 ConfigurationManager reads BasicInformation strings from the
  // "chip-factory" namespace of the default "nvs" partition (see ESP32Config.cpp
  // kConfigKey_VendorName / kConfigKey_ProductName / etc.). ESPHome already
  // nvs_flash_init'd the default partition before setup() runs, so we can just
  // open the namespace and write.
  //
  // Overwrite each boot: the yaml is the source of truth. Cost is one NVS
  // write per string per boot only when the value differs, since nvs_set_str
  // skips writes when the value is unchanged.
  // NodeLabel deliberately omitted — CHIP's ESP32Config keys for the
  // "chip-factory" namespace do not include a "node-label" entry, so a write
  // here is silently ignored. The initial NodeLabel value is instead passed
  // through node_config.root_node.basic_information.node_label in
  // create_root_node_(). Fabric-side renames go through CHIP's
  // AttributePersistence.
  struct Pair {
    const char *key;
    const std::string &value;
  };
  const Pair pairs[] = {
      {"vendor-name", this->vendor_name_},
      {"product-name", this->product_name_},
      {"hw-ver-str", this->hardware_version_string_},
      // CHIP uses "sw-ver-str" for the Software Version String attribute
      // (BasicInformation cluster). Matter spec 1.4 §11.1.5.5.
      {"sw-ver-str", this->software_version_string_},
  };

  bool any = false;
  for (const auto &p : pairs) {
    if (!p.value.empty()) {
      any = true;
      break;
    }
  }
  if (!any) {
    return;
  }

  nvs_handle_t handle;
  esp_err_t err = nvs_open("chip-factory", NVS_READWRITE, &handle);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "nvs_open(chip-factory) failed: %s — identity strings not applied", esp_err_to_name(err));
    return;
  }
  for (const auto &p : pairs) {
    if (p.value.empty()) {
      continue;
    }
    esp_err_t werr = nvs_set_str(handle, p.key, p.value.c_str());
    if (werr != ESP_OK) {
      ESP_LOGW(TAG, "nvs_set_str(chip-factory/%s) failed: %s", p.key, esp_err_to_name(werr));
    } else {
      ESP_LOGI(TAG, "wrote chip-factory/%s = \"%s\"", p.key, p.value.c_str());
    }
  }
  esp_err_t cerr = nvs_commit(handle);
  if (cerr != ESP_OK) {
    ESP_LOGW(TAG, "nvs_commit(chip-factory) failed: %s", esp_err_to_name(cerr));
  }
  nvs_close(handle);
}

void MatterComponent::register_endpoint_label(void *endpoint, uint16_t endpoint_id, const std::string &label) {
  if (endpoint == nullptr) {
    return;
  }
  // An empty name only means we can't populate FixedLabel/BDBI.NodeLabel —
  // it does NOT mean we should skip the bridged-device wrap. Bailing here
  // (as the previous version did) left the endpoint without a Bridged Node
  // device type or an Aggregator reparent, producing exactly the
  // half-configured bridge the setup() aggregator-fallback path treats as
  // incoherent. Continue with a synthesized label so the wrap still runs.
  std::string effective_label = label;
  if (effective_label.empty()) {
    char synthesized[16];
    std::snprintf(synthesized, sizeof(synthesized), "ep-%u", endpoint_id);
    effective_label.assign(synthesized);
    ESP_LOGW(TAG, "endpoint %u has empty name — using synthesized label '%s' for FixedLabel/BDBI", endpoint_id,
             effective_label.c_str());
  }
  // FixedLabel cluster attach is intentionally NOT done anymore. It became
  // redundant once we started attaching BridgedDeviceBasicInformation per
  // endpoint below — every modern controller that reads FixedLabel also
  // reads BDBI.NodeLabel and prefers it (longer cap, writable, integrates
  // with the Bridge topology UX). Dropping FixedLabel saves ~150 bytes of
  // heap per endpoint × 74 endpoints ≈ 11KB of internal DRAM, which we need
  // for the CHIP data-model provider's other allocations (rebuild history:
  // adding BDBI on top of FixedLabel + all remaining clusters starved
  // operator new inside std::map<EndpointId, LazyRegisteredServerCluster<
  // FixedLabelCluster>>::operator[] at fixed_label_integration.cpp:31, and
  // ESP-IDF has C++ exceptions off so bad_alloc turns into abort).
  //
  // The endpoint_labels_ vector below still gets populated so
  // DeviceInfoProvider::IterateFixedLabel can serve entries if a controller
  // asks the FixedLabel cluster on the root endpoint (which esp-matter
  // registers by default). It just isn't a per-endpoint attach anymore.

  // Per-endpoint UserLabel cluster attach is intentionally left off. On this
  // device (74 non-root endpoints, ESP32-S3 with 512KB internal SRAM), each
  // registered cluster carries CHIP data-model state (LazyRegisteredServerCluster
  // in gServers, ServerClusterInterfaceRegistry entry, PersistentStorage
  // metadata) that eats internal DRAM — including the DMA-capable region the
  // W5500 SPI driver needs for its priv RX buffer. Empirically, attaching
  // UserLabel to all 74 non-root endpoints starves that pool and either
  // corrupts the mbedtls DRBG (Crypto::GetRandU32 abort mid-enable_all) or
  // kills the link-check timer (spi_master setup_dma_priv_buffer failure).
  //
  // The UserLabel storage side (user_labels_ map, NVS load/persist,
  // DeviceInfoProvider Set/Get/Iterate hooks) is left wired in so a future
  // build with lower endpoint count — or a selective root-only attach — can
  // re-enable this by uncommenting the two lines below. As of today the
  // fabrics we target (Apple Home, Google Home, SmartLife) all ignore
  // UserLabel by design; only Home Assistant / openHAB read it, and HA is
  // already served by FixedLabel + BasicInformation.NodeLabel.
  //
  // ::esp_matter::cluster::user_label::config_t ul_config;
  // ::esp_matter::cluster::user_label::create(
  //     static_cast<::esp_matter::endpoint_t *>(endpoint), &ul_config,
  //     ::esp_matter::CLUSTER_FLAG_SERVER);

  // Matter spec 1.4 §9.7 caps FixedLabel value at 16 octets. Truncate here
  // and log once so the entity author knows to shorten the ESPHome `name:` —
  // this is a hard spec limit, not an ESP32 provider quirk.
  std::string clipped = effective_label;
  if (clipped.size() > chip::DeviceLayer::kMaxLabelValueLength) {
    ESP_LOGW(TAG, "endpoint %u name \"%s\" exceeds 16-octet FixedLabel value limit — truncating", endpoint_id,
             effective_label.c_str());
    clipped.resize(chip::DeviceLayer::kMaxLabelValueLength);
  }
  this->endpoint_labels_.emplace_back(endpoint_id, std::move(clipped));

  // Composed-device mode: skip the bridged-device wrap entirely. In composed
  // topology (Matter spec §9.5) entity endpoints attach directly to the
  // root_node and share its BasicInformation — no Aggregator to reparent
  // under, no BDBI cluster, no per-endpoint NodeLabel. Controllers fall back
  // to FixedLabel (served by our custom DeviceInfoProvider from
  // endpoint_labels_ populated just above) for naming. Motivated by
  // consumer hubs like the eWeLink Cube whose UI misbehaves with Bridge
  // topology carrying many bridged endpoints (documented separately in the
  // matter-ewelink-keepalive memory).
  if (this->composed_topology_) {
    return;
  }

  // Bridged-device wrap. Adds the Bridged Node device type (0x0013) on top
  // of whatever the endpoint already advertises (on_off_light,
  // contact_sensor, etc.), attaches a BridgedDeviceBasicInformation cluster,
  // seeds NodeLabel with the entity's ESPHome name (32-char cap — 2x what
  // FixedLabel allows), and reparents the endpoint under the Aggregator.
  //
  // Result: controllers that support Matter Bridge topology (Apple Home,
  // Google Home, Home Assistant, SmartLife, openHAB) present this endpoint
  // as its own accessory named by NodeLabel — not "Matter Accessory (43)"
  // or "%key_id:XXXX%". NodeLabel is writable+nonvolatile so subsequent
  // fabric-side renames persist across reboots (CHIP AttributePersistence
  // path, same infrastructure that handles root's BasicInformation.NodeLabel).
  ::esp_matter::endpoint_t *ep = static_cast<::esp_matter::endpoint_t *>(endpoint);
  // Bridge wrap is all-or-nothing: if the device type, BDBI cluster, or
  // reparent fails, we abort the remaining steps and log an error rather than
  // leaving the endpoint half-configured. A half-wrapped bridge is worse than
  // no wrap: fabrics see a Bridged Node device type but the BDBI cluster is
  // absent (or NodeLabel is missing), which some controllers refuse to
  // enumerate; and a wrapped endpoint that is NOT reparented under the
  // aggregator advertises Bridged semantics with no bridge in the tree.
  esp_err_t dt_err =
      ::esp_matter::endpoint::add_device_type(ep, ::esp_matter::endpoint::bridged_node::get_device_type_id(),
                                              ::esp_matter::endpoint::bridged_node::get_device_type_version());
  if (dt_err != ESP_OK) {
    ESP_LOGE(TAG, "endpoint %u: bridge wrap aborted — add Bridged Node device type failed: %s", endpoint_id,
             esp_err_to_name(dt_err));
    return;
  }
  ::esp_matter::cluster::bridged_device_basic_information::config_t bdbi_config;
  bdbi_config.reachable = true;
  ::esp_matter::cluster_t *bdbi_cluster = ::esp_matter::cluster::bridged_device_basic_information::create(
      ep, &bdbi_config, ::esp_matter::CLUSTER_FLAG_SERVER);
  if (bdbi_cluster == nullptr) {
    ESP_LOGE(TAG, "endpoint %u: bridge wrap aborted — BridgedDeviceBasicInformation cluster creation failed",
             endpoint_id);
    return;
  }
  // NodeLabel is capped at 32 chars per BasicInformation spec (§11.1.5.3),
  // and esp-matter's create_node_label rejects anything longer via
  // VerifyOrReturnValue on k_max_node_label_length.
  constexpr size_t max_node_label = 32;
  // Buffer must be writable — esp_matter_char_str stores a char* pointer.
  // Wrap in unique_ptr so the std::string sits at a stable heap address,
  // unaffected by any later push into bridged_labels_ (a bare vector of
  // std::string could realloc and invalidate SSO buffer pointers we've
  // already handed to esp_matter).
  this->bridged_labels_.push_back(std::make_unique<std::string>(effective_label.substr(0, max_node_label)));
  std::string &stored = *this->bridged_labels_.back();
  ::esp_matter::cluster::bridged_device_basic_information::attribute::create_node_label(
      bdbi_cluster,
      // The API takes `char *` but esp_matter stores it internally; passing
      // a mutable pointer into the std::string's buffer is safe as long as
      // the string isn't modified after this point.
      const_cast<char *>(stored.data()), static_cast<uint16_t>(stored.size()));
  if (this->aggregator_endpoint_ != nullptr) {
    ::esp_matter::endpoint_t *aggregator = static_cast<::esp_matter::endpoint_t *>(this->aggregator_endpoint_);
    esp_err_t pe_err = ::esp_matter::endpoint::set_parent_endpoint(ep, aggregator);
    if (pe_err != ESP_OK) {
      ESP_LOGE(TAG, "endpoint %u: bridge wrap incomplete — set_parent_endpoint(aggregator) failed: %s", endpoint_id,
               esp_err_to_name(pe_err));
    }
  }
}

void MatterComponent::install_device_info_provider_() {
  // Point the singleton provider at our vector and register it with CHIP.
  // The vector is stable for the lifetime of MatterComponent (we never erase
  // or reallocate after setup()), so raw pointer aliasing is safe.
  s_device_info_provider.labels = &this->endpoint_labels_;
  s_device_info_provider.component = this;
  ::esp_matter::set_custom_device_info_provider(&s_device_info_provider);
  ESP_LOGI(TAG,
           "registered custom DeviceInfoProvider with %u endpoint labels, "
           "%u UserLabel entries restored from NVS",
           static_cast<unsigned>(this->endpoint_labels_.size()), static_cast<unsigned>([this] {
             size_t n = 0;
             for (const auto &kv : this->user_labels_)
               n += kv.second.size();
             return n;
           }()));

  // Hook the instance info provider onto our string members so
  // BasicInformation cluster reads return the YAML-configured values instead
  // of the "TEST_VENDOR" / "TEST_PRODUCT" compile-time defaults baked into
  // GenericDeviceInstanceInfoProvider.
  s_device_instance_info_provider.vendor_id = this->vendor_id_;
  s_device_instance_info_provider.product_id = this->product_id_;
  s_device_instance_info_provider.vendor_name = &this->vendor_name_;
  s_device_instance_info_provider.product_name = &this->product_name_;
  s_device_instance_info_provider.hardware_version_string = &this->hardware_version_string_;
  ::esp_matter::set_custom_device_instance_info_provider(&s_device_instance_info_provider);
  ESP_LOGI(TAG, "registered custom DeviceInstanceInfoProvider (vendor=\"%s\" product=\"%s\")",
           this->vendor_name_.c_str(), this->product_name_.c_str());
}

// ---------------------------------------------------------------------------
// UserLabel NVS persistence
//
// Storage layout:
//   NVS namespace: "mtr-ul"
//   Key per endpoint: "ep<endpoint_id>" (max "ep65535" = 7 chars, within
//     NVS' 15-char key limit).
//   Value blob:
//     [u8 count]
//     for each entry:
//       [u8 label_len (0..16)] [label bytes]
//       [u8 value_len (0..16)] [value bytes]
//
// The Matter spec (§9.7) caps both label and value at 16 octets, so a single
// entry is at most 34 bytes and typical endpoint totals stay well under a
// few hundred bytes — comfortable for NVS blob semantics.
// ---------------------------------------------------------------------------

static constexpr const char *USER_LABEL_NVS_NAMESPACE = "mtr-ul";
static constexpr size_t USER_LABEL_MAX_OCTETS = 16;  // Matter spec §9.7

static std::string user_label_key_for(uint16_t endpoint_id) {
  char buf[16];
  std::snprintf(buf, sizeof(buf), "ep%u", static_cast<unsigned>(endpoint_id));
  return std::string(buf);
}

// Best-effort parse: on malformed blob (short read, oversized field) we bail
// out with whatever entries were successfully decoded — the endpoint just
// starts fresh instead of trapping the boot.
// Returns true on a fully-parsed blob; false when the input was truncated,
// malformed, or the declared count could not be filled. Callers use the
// return value to decide whether to log — treating a partial decode as
// authoritative would let the next fabric write persist the truncated list
// back over NVS and silently destroy the remaining entries.
static bool deserialize_user_labels(const uint8_t *buf, size_t len, std::vector<MatterComponent::UserLabelEntry> &out) {
  out.clear();
  if (buf == nullptr || len == 0) {
    return true;  // "no data" is not corruption
  }
  size_t pos = 0;
  uint8_t count = buf[pos++];
  out.reserve(count);
  for (uint8_t i = 0; i < count; i++) {
    if (pos >= len)
      return false;
    uint8_t label_len = buf[pos++];
    if (label_len > USER_LABEL_MAX_OCTETS || pos + label_len > len)
      return false;
    std::string label(reinterpret_cast<const char *>(buf + pos), label_len);
    pos += label_len;
    if (pos >= len)
      return false;
    uint8_t value_len = buf[pos++];
    if (value_len > USER_LABEL_MAX_OCTETS || pos + value_len > len)
      return false;
    std::string value(reinterpret_cast<const char *>(buf + pos), value_len);
    pos += value_len;
    out.push_back({std::move(label), std::move(value)});
  }
  return true;
}

static std::vector<uint8_t> serialize_user_labels(const std::vector<MatterComponent::UserLabelEntry> &entries) {
  std::vector<uint8_t> buf;
  const size_t n = std::min<size_t>(entries.size(), 255);
  buf.reserve(1 + n * (2 + 2 * USER_LABEL_MAX_OCTETS));
  buf.push_back(static_cast<uint8_t>(n));
  for (size_t i = 0; i < n; i++) {
    const auto &e = entries[i];
    const uint8_t label_len = static_cast<uint8_t>(std::min<size_t>(e.label.size(), USER_LABEL_MAX_OCTETS));
    buf.push_back(label_len);
    buf.insert(buf.end(), e.label.data(), e.label.data() + label_len);
    const uint8_t value_len = static_cast<uint8_t>(std::min<size_t>(e.value.size(), USER_LABEL_MAX_OCTETS));
    buf.push_back(value_len);
    buf.insert(buf.end(), e.value.data(), e.value.data() + value_len);
  }
  return buf;
}

void MatterComponent::load_user_labels_() {
  nvs_handle_t handle;
  esp_err_t err = nvs_open(USER_LABEL_NVS_NAMESPACE, NVS_READONLY, &handle);
  if (err == ESP_ERR_NVS_NOT_FOUND) {
    // Fresh device — namespace hasn't been created yet. Nothing to load.
    return;
  }
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "nvs_open(%s) failed: %s — UserLabel entries not restored", USER_LABEL_NVS_NAMESPACE,
             esp_err_to_name(err));
    return;
  }
  nvs_iterator_t it = nullptr;
  esp_err_t find_err = nvs_entry_find("nvs", USER_LABEL_NVS_NAMESPACE, NVS_TYPE_BLOB, &it);
  // ESP_ERR_NVS_NOT_FOUND is the "namespace has no matching entries" reply and
  // is the expected outcome on a device that has never written a UserLabel.
  // Any other non-OK reply is a real IDF error (corruption, storage failure)
  // that would leave user_labels_ permanently empty; log it so operators can
  // tell that apart from "no labels were ever set".
  if (find_err != ESP_OK && find_err != ESP_ERR_NVS_NOT_FOUND) {
    ESP_LOGW(TAG, "nvs_entry_find(%s) failed: %s — UserLabel entries not restored", USER_LABEL_NVS_NAMESPACE,
             esp_err_to_name(find_err));
  }
  size_t restored_entries = 0;
  while (find_err == ESP_OK && it != nullptr) {
    nvs_entry_info_t info;
    nvs_entry_info(it, &info);
    // Keys we care about start with "ep" and are followed by an integer.
    // Parsed by hand (no scanf-family calls — ~7-9 KB of flash overhead the
    // ESPHome integration guidelines rule out on embedded targets).
    uint32_t ep_id = 0;
    bool key_matches = info.key[0] == 'e' && info.key[1] == 'p' && info.key[2] != '\0';
    for (const char *p = info.key + 2; key_matches && *p != '\0'; ++p) {
      if (*p < '0' || *p > '9') {
        key_matches = false;
        break;
      }
      ep_id = ep_id * 10 + static_cast<uint32_t>(*p - '0');
      if (ep_id > UINT16_MAX) {
        key_matches = false;
        break;
      }
    }
    if (key_matches) {
      size_t blob_size = 0;
      esp_err_t sz_err = nvs_get_blob(handle, info.key, nullptr, &blob_size);
      if (sz_err != ESP_OK) {
        // Swallowing this hides genuine NVS corruption — the summary log
        // below would then report a successful restore for the remaining
        // keys while this endpoint's labels were silently lost.
        ESP_LOGW(TAG, "nvs_get_blob(%s) size query failed: %s", info.key, esp_err_to_name(sz_err));
      } else if (blob_size > 0) {
        std::vector<uint8_t> buf(blob_size);
        esp_err_t rd_err = nvs_get_blob(handle, info.key, buf.data(), &blob_size);
        if (rd_err != ESP_OK) {
          ESP_LOGW(TAG, "nvs_get_blob(%s) read failed: %s", info.key, esp_err_to_name(rd_err));
        } else {
          std::vector<MatterComponent::UserLabelEntry> entries;
          const bool ok = deserialize_user_labels(buf.data(), blob_size, entries);
          if (!ok) {
            // Partial decode — do NOT persist this endpoint back to NVS on
            // the next fabric write, or the truncated list becomes the new
            // authoritative state.
            ESP_LOGW(TAG, "nvs blob %s malformed/truncated — dropping %u partially-decoded entries", info.key,
                     static_cast<unsigned>(entries.size()));
          } else if (entries.empty()) {
            ESP_LOGW(TAG, "nvs blob %s decoded to zero UserLabel entries — corrupt or empty", info.key);
          } else {
            restored_entries += entries.size();
            this->user_labels_[static_cast<uint16_t>(ep_id)] = std::move(entries);
          }
        }
      }
    }
    find_err = nvs_entry_next(&it);
  }
  // ESP_ERR_NVS_NOT_FOUND after nvs_entry_next means the scan reached the end
  // cleanly. Anything else non-OK means iteration bailed on a real IDF error —
  // silently exiting the loop would let the summary log claim a successful
  // partial restore, which is what this branch exists to prevent.
  if (find_err != ESP_OK && find_err != ESP_ERR_NVS_NOT_FOUND) {
    ESP_LOGW(TAG, "nvs_entry_next(%s) failed mid-iteration: %s — UserLabel restore may be incomplete",
             USER_LABEL_NVS_NAMESPACE, esp_err_to_name(find_err));
  }
  if (it != nullptr) {
    nvs_release_iterator(it);
  }
  nvs_close(handle);
  if (restored_entries > 0) {
    ESP_LOGI(TAG, "restored %u UserLabel entries across %u endpoints from NVS", static_cast<unsigned>(restored_entries),
             static_cast<unsigned>(this->user_labels_.size()));
  }
}

esp_err_t MatterComponent::persist_user_labels_for_endpoint_(uint16_t endpoint_id) {
  nvs_handle_t handle;
  esp_err_t err = nvs_open(USER_LABEL_NVS_NAMESPACE, NVS_READWRITE, &handle);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "nvs_open(%s) failed: %s — endpoint %u UserLabel not persisted", USER_LABEL_NVS_NAMESPACE,
             esp_err_to_name(err), endpoint_id);
    return err;
  }
  const std::string key = user_label_key_for(endpoint_id);
  // Track the first failure so the caller (and thus the CHIP provider) can
  // return CHIP_ERROR_PERSISTED_STORAGE_FAILED. Continue with the commit so
  // the erase/set that did succeed is durable — we don't want to leave the
  // in-memory state and NVS state permanently diverged.
  esp_err_t status = ESP_OK;
  auto it = this->user_labels_.find(endpoint_id);
  if (it == this->user_labels_.end() || it->second.empty()) {
    // Erase key so the next boot sees no entries — avoids stale zero-count
    // blobs lingering.
    esp_err_t erase_err = nvs_erase_key(handle, key.c_str());
    if (erase_err != ESP_OK && erase_err != ESP_ERR_NVS_NOT_FOUND) {
      ESP_LOGW(TAG, "nvs_erase_key(%s) failed: %s", key.c_str(), esp_err_to_name(erase_err));
      status = erase_err;
    }
  } else {
    const auto blob = serialize_user_labels(it->second);
    esp_err_t werr = nvs_set_blob(handle, key.c_str(), blob.data(), blob.size());
    if (werr != ESP_OK) {
      ESP_LOGW(TAG, "nvs_set_blob(%s) failed: %s", key.c_str(), esp_err_to_name(werr));
      status = werr;
    }
  }
  esp_err_t cerr = nvs_commit(handle);
  if (cerr != ESP_OK) {
    ESP_LOGW(TAG, "nvs_commit(%s) failed: %s", USER_LABEL_NVS_NAMESPACE, esp_err_to_name(cerr));
    if (status == ESP_OK) {
      status = cerr;
    }
  }
  nvs_close(handle);
  return status;
}

const std::vector<MatterComponent::UserLabelEntry> *MatterComponent::user_labels_for(uint16_t endpoint_id) const {
  auto it = this->user_labels_.find(endpoint_id);
  return (it != this->user_labels_.end()) ? &it->second : nullptr;
}

size_t MatterComponent::user_label_count_for(uint16_t endpoint_id) const {
  auto it = this->user_labels_.find(endpoint_id);
  return (it != this->user_labels_.end()) ? it->second.size() : 0;
}

int MatterComponent::set_user_label_length(uint16_t endpoint_id, size_t new_length) {
  auto &vec = this->user_labels_[endpoint_id];
  if (new_length == 0) {
    vec.clear();
  } else {
    vec.resize(new_length);  // grows with empty entries, shrinks by dropping the tail
  }
  // Return non-zero when NVS persist fails so the CHIP provider maps it to
  // CHIP_ERROR_PERSISTED_STORAGE_FAILED — otherwise the fabric is told the
  // write is durable while NVS still has the old (or no) blob.
  return (this->persist_user_labels_for_endpoint_(endpoint_id) == ESP_OK) ? 0 : 1;
}

int MatterComponent::set_user_label_at(uint16_t endpoint_id, size_t index, const std::string &label,
                                       const std::string &value) {
  auto &vec = this->user_labels_[endpoint_id];
  if (index >= vec.size()) {
    // CHIP is expected to call SetUserLabelLength first, but be defensive —
    // grow so we don't drop the fabric's write on the floor.
    vec.resize(index + 1);
  }
  // Enforce the 16-octet cap regardless of what the fabric sent; some
  // controllers ignore constraint checks and a longer string would violate
  // Matter spec §9.7.
  vec[index].label = label.substr(0, USER_LABEL_MAX_OCTETS);
  vec[index].value = value.substr(0, USER_LABEL_MAX_OCTETS);
  return (this->persist_user_labels_for_endpoint_(endpoint_id) == ESP_OK) ? 0 : 1;
}

int MatterComponent::delete_user_label_at(uint16_t endpoint_id, size_t index) {
  auto it = this->user_labels_.find(endpoint_id);
  if (it == this->user_labels_.end() || index >= it->second.size()) {
    return 0;  // deleting a non-existent slot is idempotent success
  }
  it->second.erase(it->second.begin() + index);
  return (this->persist_user_labels_for_endpoint_(endpoint_id) == ESP_OK) ? 0 : 1;
}

void MatterComponent::setup() {
  ESP_LOGI(TAG, "setting up Matter (vendor=0x%04x product=0x%04x)", this->vendor_id_, this->product_id_);

  // Keep ESPHome's ``esp_wifi_set_storage(WIFI_STORAGE_RAM)`` in place —
  // we deliberately do NOT flip storage to FLASH here.
  //
  // Rationale: with FLASH storage, CHIP's NetworkCommissioning delegate
  // persists the BLE-provisioned credentials into the ``nvs.net80211``
  // driver namespace via ``esp_wifi_set_config``. On the next boot the
  // ESP-IDF driver auto-restores that config during ``esp_wifi_init``,
  // and CHIP's ConnectivityManagerImpl::DriveStationState sees
  // ``IsStationProvisioned() == true`` (SSID non-empty in the runtime
  // config), so it calls ``esp_wifi_connect()`` on its own. That races
  // ESPHome's wifi_component, which is also trying to connect using the
  // credentials we stashed into ESPHome preferences via
  // ``persist_ble_provisioned_wifi_creds_()``. Both stacks then fight
  // over the STA slot — ESPHome sends ``esp_wifi_disconnect`` because
  // its config differs from CHIP's runtime config, CHIP reconnects,
  // repeat. In that state ESPHome's wifi_component never fully
  // ``connects`` from its own POV, mDNS keeps advertising on the AP
  // fallback netif, and the ``.local`` name of the device is
  // unreachable from the home network.
  //
  // With RAM storage, ``esp_wifi_set_config`` during commissioning
  // affects the driver's live runtime only; ``nvs.net80211`` stays
  // empty. On the next boot ``esp_wifi_init`` finds no station config
  // to restore; ``IsStationProvisioned()`` returns false; CHIP does
  // not auto-connect. ESPHome's wifi_component reads our
  // SavedWifiSettings preference during ``start()``, populates sta_,
  // does the normal STA connect flow, wins the driver uncontended,
  // and mDNS advertises on the home network. When ``esp_matter::start()``
  // later runs its ConnectivityManagerImpl, CHIP observes the already-
  // established STA connection and joins it instead of driving a new
  // connect.

  // Write identity strings BEFORE any CHIP init runs — CHIP's ConfigurationMgr
  // reads chip-factory NVS on first attribute query and caches. Writing after
  // esp_matter::start would leave the stale TEST_VENDOR/TEST_PRODUCT strings
  // visible on the current boot.
  this->write_factory_strings_();

  // Restore UserLabel entries before endpoints are registered so the
  // DeviceInfoProvider can serve them the first time a controller asks. If
  // NVS has no data yet (fresh device) this is a no-op; existing devices
  // pick up whatever labels the fabric wrote last session.
  this->load_user_labels_();

  if (!this->create_root_node_()) {
    // Bail immediately — without a root node, every scan_and_register_* pass
    // and install_device_info_provider_ + esp_matter::start would fail
    // per-entity, drowning the single real error in dispatch noise.
    // create_root_node_ has already called mark_failed(); nothing else to do.
    return;
  }
  // Aggregator only exists in bridge topology. In composed mode the entity
  // endpoints are direct children of root_node — no wrapper endpoint sits
  // between them and node 0. Skipping the create call leaves
  // aggregator_endpoint_ nullptr, which register_endpoint_label() checks
  // when deciding whether to reparent + attach BDBI.
  if (!this->composed_topology_) {
    this->create_aggregator_endpoint_();
    if (this->aggregator_endpoint_ == nullptr) {
      // Fall back to composed topology: without an aggregator, attaching
      // Bridged Node + BDBI on each endpoint would produce a half-configured
      // bridge (device type says "sub-accessory" but nothing reparents them),
      // which controllers render as anonymous/offline entries. Composed is
      // strictly less capable but coherent, and the entity endpoints still
      // hang off root_node normally.
      ESP_LOGW(TAG, "topology=bridge requested but Aggregator creation failed — "
                    "falling back to composed (single-device) topology");
      this->composed_topology_ = true;
    }
  } else {
    ESP_LOGI(TAG, "topology=composed — skipping Aggregator + BridgedDeviceBasicInformation attach");
  }

  // Compute an upper bound on the total endpoint count so we can allocate
  // the aggregate label vectors up-front. Sums over every ESPHome list the
  // scan_and_register_* passes below iterate — filtered entries (internal
  // sensors, mode-less selects, etc.) just leave unused capacity, which
  // costs ~24 bytes each. Only paid once at setup and never resized after.
  size_t total_entity_upper_bound = 0;
#ifdef USE_SWITCH
  total_entity_upper_bound += App.get_switches().size();
#endif
#ifdef USE_BINARY_SENSOR
  total_entity_upper_bound += App.get_binary_sensors().size();
#endif
#ifdef USE_SENSOR
  total_entity_upper_bound += App.get_sensors().size();
#endif
#ifdef USE_COVER
  total_entity_upper_bound += App.get_covers().size();
#endif
#ifdef USE_FAN
  total_entity_upper_bound += App.get_fans().size();
#endif
#ifdef USE_LOCK
  total_entity_upper_bound += App.get_locks().size();
#endif
#ifdef USE_VALVE
  total_entity_upper_bound += App.get_valves().size();
#endif
#ifdef USE_SELECT
  total_entity_upper_bound += App.get_selects().size();
#endif
#ifdef USE_BUTTON
  total_entity_upper_bound += App.get_buttons().size();
#endif
#ifdef USE_LIGHT
  total_entity_upper_bound += App.get_lights().size();
#endif
#ifdef USE_CLIMATE
  total_entity_upper_bound += App.get_climates().size();
#endif
  this->endpoint_labels_.init(total_entity_upper_bound);
  this->bridged_labels_.init(total_entity_upper_bound);

  this->scan_and_register_switches_();
  this->scan_and_register_binary_sensors_();
  this->scan_and_register_sensors_();
  this->scan_and_register_covers_();
  this->scan_and_register_fans_();
  this->scan_and_register_locks_();
  this->scan_and_register_valves_();
  this->scan_and_register_selects_();
  this->scan_and_register_buttons_();
  this->scan_and_register_lights_();
  this->scan_and_register_climates_();

  // Endpoints have all been created and their names collected — install the
  // custom DeviceInfoProvider so the FixedLabel cluster on each endpoint
  // has something to serve when the controller reads LabelList. Must happen
  // before esp_matter::start(): esp_matter_providers::setup_providers()
  // runs from esp_matter::start and reads s_custom_device_info_provider.
  this->install_device_info_provider_();

  // WIFI_EVENT handler for CHIP's DeviceLayer is registered from
  // esp-matter's own ESP32Utils::InitWiFiStack (called by
  // esp_matter::start below on Wi-Fi builds) — the ESP-IDF's
  // esp_wifi_get_mode / esp_netif_get_handle_from_ifkey guards inside
  // that helper make it safe even when ESPHome's `wifi:` already
  // initialised the driver, so we do not reproduce that registration
  // here.
  esp_err_t err = ::esp_matter::start(matter_event_cb);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_matter::start failed: %s", esp_err_to_name(err));
    this->mark_failed();
    return;
  }

  this->push_initial_states_();
  this->log_onboarding_payload_();
  this->start_keepalive_ticker_();
}

void MatterComponent::loop() {
  // esp-matter runs its own tasks; nothing to pump here.
}

void MatterComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Matter:");
  ESP_LOGCONFIG(TAG, "  Vendor ID: 0x%04X", this->vendor_id_);
  ESP_LOGCONFIG(TAG, "  Product ID: 0x%04X", this->product_id_);
  ESP_LOGCONFIG(TAG, "  Discriminator: %u", this->discriminator_);
  if (!this->vendor_name_.empty()) {
    ESP_LOGCONFIG(TAG, "  Vendor Name: %s", this->vendor_name_.c_str());
  }
  if (!this->product_name_.empty()) {
    ESP_LOGCONFIG(TAG, "  Product Name: %s", this->product_name_.c_str());
  }
  if (!this->node_label_.empty()) {
    ESP_LOGCONFIG(TAG, "  Node Label: %s", this->node_label_.c_str());
  }
  ESP_LOGCONFIG(TAG, "  BLE commissioning: %s", this->ble_commissioning_ ? "enabled" : "disabled");
#ifdef USE_SWITCH
  ESP_LOGCONFIG(TAG, "  Switch endpoints: %u", static_cast<unsigned>(this->switch_endpoints_.size()));
#endif
#ifdef USE_BINARY_SENSOR
  ESP_LOGCONFIG(TAG, "  Binary sensor endpoints: %u", static_cast<unsigned>(this->binary_sensor_endpoints_.size()));
#endif
#ifdef USE_SENSOR
  ESP_LOGCONFIG(TAG, "  Sensor endpoints: %u", static_cast<unsigned>(this->sensor_endpoints_.size()));
#endif
#ifdef USE_COVER
  ESP_LOGCONFIG(TAG, "  Cover endpoints: %u", static_cast<unsigned>(this->cover_endpoints_.size()));
#endif
#ifdef USE_FAN
  ESP_LOGCONFIG(TAG, "  Fan endpoints: %u", static_cast<unsigned>(this->fan_endpoints_.size()));
#endif
#ifdef USE_LOCK
  ESP_LOGCONFIG(TAG, "  Lock endpoints: %u", static_cast<unsigned>(this->lock_endpoints_.size()));
#endif
#ifdef USE_VALVE
  ESP_LOGCONFIG(TAG, "  Valve endpoints: %u", static_cast<unsigned>(this->valve_endpoints_.size()));
#endif
#ifdef USE_SELECT
  ESP_LOGCONFIG(TAG, "  Select endpoints: %u", static_cast<unsigned>(this->select_endpoints_.size()));
#endif
#ifdef USE_BUTTON
  ESP_LOGCONFIG(TAG, "  Button endpoints: %u", static_cast<unsigned>(this->button_endpoints_.size()));
#endif
#ifdef USE_LIGHT
  ESP_LOGCONFIG(TAG, "  Light endpoints: %u", static_cast<unsigned>(this->light_endpoints_.size()));
#endif
#ifdef USE_CLIMATE
  ESP_LOGCONFIG(TAG, "  Climate endpoints: %u", static_cast<unsigned>(this->climate_endpoints_.size()));
#endif
}

float MatterComponent::get_setup_priority() const {
  // After WiFi/Ethernet (AFTER_WIFI = 250) — the entities' own constructors
  // have already populated App.get_switches() long before any setup() runs,
  // so scanning here is safe.
  return setup_priority::AFTER_WIFI - 1.0f;
}

void MatterComponent::factory_reset() {
  ESP_LOGW(TAG, "factory reset requested — wiping Matter fabric");
  ::esp_matter::factory_reset();
}

bool MatterComponent::handle_attribute_write(uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id,
                                             bool bool_value) {
  if (cluster_id != chip::app::Clusters::OnOff::Id ||
      attribute_id != chip::app::Clusters::OnOff::Attributes::OnOff::Id) {
    ESP_LOGV(TAG, "unhandled attribute write endpoint=%u cluster=0x%08lx attr=0x%08lx", endpoint_id,
             static_cast<unsigned long>(cluster_id), static_cast<unsigned long>(attribute_id));
    // Not our attribute — return `true` so the callback returns ESP_OK.
    // (This is the "we do not intercept this cluster/attribute" path.)
    return true;
  }
  // OnOff.OnOff is shared by switch endpoints AND light endpoints — the same
  // attribute id lives on both device types. Check both maps by endpoint id,
  // switch first (the cheaper lookup pattern established earlier).
#ifdef USE_SWITCH
  if (auto *wrapper = find_endpoint_by_id(this->switch_endpoints_by_id_, endpoint_id)) {
    // Suppress re-entry when we are the ones driving attribute::update()
    // from the device→fabric report path (PRE_UPDATE fires synchronously
    // during update()).
    if (wrapper->applying_report()) {
      return true;
    }
    wrapper->on_matter_write(bool_value);
    return true;
  }
#endif
#ifdef USE_LIGHT
  if (auto *wrapper = find_endpoint_by_id(this->light_endpoints_by_id_, endpoint_id)) {
    if (wrapper->applying_report()) {
      return true;
    }
    wrapper->on_matter_on_off_write(bool_value);
    return true;
  }
#endif
  // Dispatch miss: OnOff.OnOff write arrived for an endpoint we don't own.
  // Not routine — either the endpoint map is out of sync with the CHIP
  // data model, or a foreign cluster registered under the same id. Log at
  // WARN so it surfaces in default builds instead of being compiled out.
  ESP_LOGW(TAG, "OnOff write for endpoint=%u — no switch or light wrapper found", endpoint_id);
  return false;
}

bool MatterComponent::handle_cover_target_write(uint16_t endpoint_id, uint16_t percent100ths) {
#ifdef USE_COVER
  auto *wrapper = find_endpoint_by_id(this->cover_endpoints_by_id_, endpoint_id);
  if (wrapper == nullptr) {
    ESP_LOGW(TAG, "cover target write for unknown endpoint=%u", endpoint_id);
    return false;
  }
  // Same round-trip guard as the switch dispatcher: attribute::update on
  // CurrentPositionLift fires PRE_UPDATE synchronously, which would land back
  // here if the fabric wrote Target=Current simultaneously.
  if (wrapper->applying_report()) {
    return true;
  }
  wrapper->on_matter_target_write(percent100ths);
  return true;
#else
  (void) endpoint_id;
  (void) percent100ths;
  return false;
#endif
}

bool MatterComponent::handle_fan_percent_write(uint16_t endpoint_id, uint8_t percent) {
#ifdef USE_FAN
  auto *wrapper = find_endpoint_by_id(this->fan_endpoints_by_id_, endpoint_id);
  if (wrapper == nullptr) {
    ESP_LOGW(TAG, "fan percent write for unknown endpoint=%u", endpoint_id);
    return false;
  }
  if (wrapper->applying_report()) {
    return true;
  }
  wrapper->on_matter_percent_write(percent);
  return true;
#else
  (void) endpoint_id;
  (void) percent;
  return false;
#endif
}

bool MatterComponent::handle_fan_mode_write(uint16_t endpoint_id, uint8_t fan_mode) {
#ifdef USE_FAN
  auto *wrapper = find_endpoint_by_id(this->fan_endpoints_by_id_, endpoint_id);
  if (wrapper == nullptr) {
    ESP_LOGW(TAG, "fan mode write for unknown endpoint=%u", endpoint_id);
    return false;
  }
  if (wrapper->applying_report()) {
    return true;
  }
  wrapper->on_matter_fan_mode_write(fan_mode);
  return true;
#else
  (void) endpoint_id;
  (void) fan_mode;
  return false;
#endif
}

bool MatterComponent::handle_light_level_write(uint16_t endpoint_id, uint8_t level) {
#ifdef USE_LIGHT
  auto *wrapper = find_endpoint_by_id(this->light_endpoints_by_id_, endpoint_id);
  if (wrapper == nullptr) {
    ESP_LOGW(TAG, "light level write for unknown endpoint=%u", endpoint_id);
    return false;
  }
  if (wrapper->applying_report()) {
    return true;
  }
  wrapper->on_matter_level_write(level);
  return true;
#else
  (void) endpoint_id;
  (void) level;
  return false;
#endif
}

bool MatterComponent::handle_light_color_temp_write(uint16_t endpoint_id, uint16_t mireds) {
#ifdef USE_LIGHT
  auto *wrapper = find_endpoint_by_id(this->light_endpoints_by_id_, endpoint_id);
  if (wrapper == nullptr) {
    ESP_LOGW(TAG, "light color-temp write for unknown endpoint=%u", endpoint_id);
    return false;
  }
  if (wrapper->applying_report()) {
    return true;
  }
  wrapper->on_matter_color_temp_write(mireds);
  return true;
#else
  (void) endpoint_id;
  (void) mireds;
  return false;
#endif
}

bool MatterComponent::handle_climate_system_mode_write(uint16_t endpoint_id, uint8_t system_mode) {
#ifdef USE_CLIMATE
  auto *wrapper = find_endpoint_by_id(this->climate_endpoints_by_id_, endpoint_id);
  if (wrapper == nullptr) {
    ESP_LOGW(TAG, "climate SystemMode write for unknown endpoint=%u", endpoint_id);
    return false;
  }
  if (wrapper->applying_report()) {
    return true;
  }
  wrapper->on_matter_system_mode_write(system_mode);
  return true;
#else
  (void) endpoint_id;
  (void) system_mode;
  return false;
#endif
}

bool MatterComponent::handle_climate_heating_setpoint_write(uint16_t endpoint_id, int16_t hundredths) {
#ifdef USE_CLIMATE
  auto *wrapper = find_endpoint_by_id(this->climate_endpoints_by_id_, endpoint_id);
  if (wrapper == nullptr) {
    ESP_LOGW(TAG, "climate heating-setpoint write for unknown endpoint=%u", endpoint_id);
    return false;
  }
  if (wrapper->applying_report()) {
    return true;
  }
  wrapper->on_matter_heating_setpoint_write(hundredths);
  return true;
#else
  (void) endpoint_id;
  (void) hundredths;
  return false;
#endif
}

bool MatterComponent::handle_climate_cooling_setpoint_write(uint16_t endpoint_id, int16_t hundredths) {
#ifdef USE_CLIMATE
  auto *wrapper = find_endpoint_by_id(this->climate_endpoints_by_id_, endpoint_id);
  if (wrapper == nullptr) {
    ESP_LOGW(TAG, "climate cooling-setpoint write for unknown endpoint=%u", endpoint_id);
    return false;
  }
  if (wrapper->applying_report()) {
    return true;
  }
  wrapper->on_matter_cooling_setpoint_write(hundredths);
  return true;
#else
  (void) endpoint_id;
  (void) hundredths;
  return false;
#endif
}

bool MatterComponent::handle_select_current_mode_write(uint16_t endpoint_id, uint8_t mode) {
#ifdef USE_SELECT
  auto *wrapper = find_endpoint_by_id(this->select_endpoints_by_id_, endpoint_id);
  if (wrapper == nullptr) {
    ESP_LOGW(TAG, "select CurrentMode write for unknown endpoint=%u", endpoint_id);
    return false;
  }
  if (wrapper->applying_report()) {
    return true;
  }
  wrapper->on_matter_current_mode_write(mode);
  return true;
#else
  (void) endpoint_id;
  (void) mode;
  return false;
#endif
}

bool MatterComponent::handle_lock_command(uint16_t endpoint_id, bool is_lock) {
#ifdef USE_LOCK
  auto *wrapper = find_endpoint_by_id(this->lock_endpoints_by_id_, endpoint_id);
  if (wrapper == nullptr) {
    ESP_LOGW(TAG, "lock command for unknown endpoint=%u", endpoint_id);
    return false;
  }
  // No applying_report() check — DoorLock uses commands, not attribute writes,
  // for remote lock ops, and our attribute::update from device→fabric goes
  // through a different path (not this dispatcher).
  return wrapper->on_matter_command(is_lock);
#else
  (void) endpoint_id;
  (void) is_lock;
  return false;
#endif
}

void MatterComponent::start_keepalive_ticker_() {
  // Nothing to advertise if no bridged endpoints were registered — the tick
  // would loop over an empty set. Skip cleanly.
  if (this->endpoint_labels_.empty()) {
    return;
  }

  // 30-second sweep interval was picked to comfortably fit under the
  // shortest known controller-side "last-seen" threshold (~60s, per eWeLink
  // CUBE v2.10.2 change log). Every tick marshals to the CHIP PlatformManager
  // task and marks every bridged endpoint's attributes dirty; the CHIP
  // reporting engine then batches the resulting SubscriptionReports per
  // subscriber, so N endpoints do not translate to N wire messages — the
  // engine coalesces paths belonging to the same subscription into a single
  // report chunk stream.
  static constexpr uint32_t KEEPALIVE_INTERVAL_MS = 30 * 1000;

  ESP_LOGI(TAG, "keepalive: scheduling subscription refresh every %ums for %u bridged endpoints",
           static_cast<unsigned>(KEEPALIVE_INTERVAL_MS), static_cast<unsigned>(this->endpoint_labels_.size()));

  this->set_interval("matter_keepalive", KEEPALIVE_INTERVAL_MS, []() {
    // ScheduleWork hops onto the CHIP event-loop task where the stack lock
    // is held — MatterReportingAttributeChangeCallback asserts on that in
    // reporting.cpp:32 (assertChipStackLockedByCurrentThread). Calling it
    // directly from the ESPHome main loop would trip the assert on debug
    // builds and race the ReadHandler pool on release builds.
    //
    // ScheduleWork returns non-OK when CHIP's platform event queue is full
    // (CONFIG_MAX_EVENT_QUEUE_SIZE — sized to 100 in __init__.py). Dropping
    // one keepalive tick is harmless — the next 30s tick will retry — but
    // silent drops during a saturation event would look like the device is
    // going quiet on the fabric. Log so we notice.
    CHIP_ERROR err = ::chip::DeviceLayer::PlatformMgr().ScheduleWork(
        [](intptr_t) {
          auto *self = MatterComponent::instance();
          if (self == nullptr) {
            return;
          }
          self->keepalive_tick_impl_();
        },
        0);
    if (err != CHIP_NO_ERROR) {
      ESP_LOGW(TAG, "keepalive: ScheduleWork failed (queue full?): %" CHIP_ERROR_FORMAT, err.Format());
    }
  });
}

void MatterComponent::keepalive_tick_impl_() {
  // Endpoint 0 is root_node — never an entity endpoint. In bridge topology
  // endpoint 1 is the Aggregator, also skipped. endpoint_labels_ is populated
  // only from register_endpoint_label() (called for actual entity endpoints
  // by scan_and_register_*()), so 0 and (in bridge) 1 aren't in the vector
  // by construction. In composed topology the Aggregator does not exist and
  // endpoint 1 is a real entity that must be refreshed. Skip only root_node
  // unconditionally.
  size_t refreshed = 0;
  for (const auto &pair : this->endpoint_labels_) {
    const uint16_t endpoint_id = pair.first;
    if (endpoint_id == 0) {
      continue;
    }
    // The (EndpointId, EndpointChangeType) overload runs
    // Engine::OnEndpointChanged (reporting/Engine.cpp), which calls
    // SetDirty(AttributePathParams(endpointId)) — cluster+attribute
    // wildcards on that endpoint. The reporting engine then walks its
    // subscriber list and includes this endpoint in the next report window
    // for each subscription whose path filter matches — which, for wildcard
    // subscribers like eWeLink Cube and Google Home, is every subscription.
    // Spec-compliant Matter controllers (Apple Home) already keep the
    // subscription alive via MaxInterval, so an extra empty refresh here is
    // harmless — just an occasional "state unchanged" report they discard.
    // kAdded is arbitrary here (SetDirty ignores the type); using it because
    // "endpoint appeared, everything is fresh" matches the intent better than
    // kRemoved.
    ::MatterReportingAttributeChangeCallback(static_cast<chip::EndpointId>(endpoint_id),
                                             chip::app::DataModel::EndpointChangeType::kAdded);
    refreshed++;
  }
  ESP_LOGV(TAG, "keepalive: refreshed %u bridged endpoints", static_cast<unsigned>(refreshed));
}

bool MatterComponent::create_root_node_() {
  // node::create() internally creates the root_node endpoint (endpoint 0)
  // for us — see esp_matter_endpoint.cpp: it calls
  // endpoint::root_node::create() with config->root_node. Do NOT create
  // another root_node manually here or the NetworkCommissioning cluster
  // will be registered twice and VerifyOrDie fires in ServerClusterRegistry.
  ::esp_matter::node::config_t node_config;

  // Seed BasicInformation.NodeLabel with the yaml-configured value. CHIP's
  // BasicInformationCluster (BasicInformationCluster.cpp:428) loads NodeLabel
  // from AttributePersistence at Startup — if nothing persisted, mNodeLabel
  // stays empty. esp-matter's basic_information::create passes this config
  // field to create_node_label() as the attribute's initial value, which
  // esp-matter writes into CHIP storage on first boot. On subsequent boots
  // any fabric-side rename that persistence.StoreString captured wins.
  //
  // The chip-factory NVS "node-label" write in write_factory_strings_() was
  // dead code — that key is not in ESP32Config::kConfigKey_* so CHIP never
  // reads it. Configuring node_config here is the correct path.
  if (!this->node_label_.empty()) {
    const size_t max_len = sizeof(node_config.root_node.basic_information.node_label) - 1;
    const size_t copy_len = std::min(this->node_label_.size(), max_len);
    std::memcpy(node_config.root_node.basic_information.node_label, this->node_label_.data(), copy_len);
    node_config.root_node.basic_information.node_label[copy_len] = '\0';
  }

  ::esp_matter::node_t *node = ::esp_matter::node::create(&node_config, attribute_update_cb, nullptr);
  if (node == nullptr) {
    ESP_LOGE(TAG, "failed to create Matter node");
    this->mark_failed();
    return false;
  }
  return true;
}

void MatterComponent::create_aggregator_endpoint_() {
  // Aggregator endpoint (device type 0x000E, Matter spec §9.12) sits between
  // the root and every entity endpoint. Its presence signals to controllers
  // "this device is a Matter Bridge — the endpoints under me are separate
  // logical accessories, each with its own BasicInformation-like cluster".
  //
  // Without this, controllers that support Bridge topology (Apple Home,
  // Google Home, Home Assistant) fall back to naming each endpoint by
  // translation key or index because they have no idea what to call an
  // opaque sub-service of a flat Matter device. With this, they read
  // BridgedDeviceBasicInformation.NodeLabel on each child endpoint and
  // display N accessories in the app UI, each with its own name.
  ::esp_matter::node_t *node = ::esp_matter::node::get();
  if (node == nullptr) {
    ESP_LOGE(TAG, "create_aggregator_endpoint: no Matter node — root was never created");
    return;
  }
  ::esp_matter::endpoint::aggregator::config_t aggregator_config;
  ::esp_matter::endpoint_t *aggregator =
      ::esp_matter::endpoint::aggregator::create(node, &aggregator_config, ::esp_matter::ENDPOINT_FLAG_NONE, nullptr);
  if (aggregator == nullptr) {
    ESP_LOGE(TAG, "failed to create Aggregator endpoint — bridge topology will not be advertised");
    return;
  }
  this->aggregator_endpoint_ = aggregator;
  ESP_LOGI(TAG, "created Aggregator endpoint id=%u — bridging entity endpoints under it",
           ::esp_matter::endpoint::get_id(aggregator));
}

void MatterComponent::scan_and_register_switches_() {
#ifdef USE_SWITCH
  const size_t upper_bound = App.get_switches().size();
  this->switch_endpoints_.init(upper_bound);
  this->switch_endpoints_by_id_.init(upper_bound);
  size_t registered = 0;
  for (auto *sw : App.get_switches()) {
    if (sw == nullptr || sw->is_internal()) {
      continue;
    }
    auto endpoint = std::unique_ptr<MatterSwitchEndpoint>(new MatterSwitchEndpoint(sw));
    if (!endpoint->setup()) {
      continue;
    }
    uint16_t id = endpoint->endpoint_id();
    this->switch_endpoints_by_id_.emplace_back(id, endpoint.get());
    this->switch_endpoints_.push_back(std::move(endpoint));
    registered++;
  }
  ESP_LOGI(TAG, "scan_and_register_switches: %u switches registered", static_cast<unsigned>(registered));
#else
  ESP_LOGD(TAG, "scan_and_register_switches: USE_SWITCH not defined, skipping");
#endif
}

void MatterComponent::scan_and_register_binary_sensors_() {
#ifdef USE_BINARY_SENSOR
  const size_t upper_bound = App.get_binary_sensors().size();
  this->binary_sensor_endpoints_.init(upper_bound);
  size_t registered = 0;
  for (auto *bs : App.get_binary_sensors()) {
    if (bs == nullptr || bs->is_internal()) {
      continue;
    }
    auto endpoint = std::unique_ptr<MatterBinarySensorEndpoint>(new MatterBinarySensorEndpoint(bs));
    if (!endpoint->setup()) {
      continue;
    }
    this->binary_sensor_endpoints_.push_back(std::move(endpoint));
    registered++;
  }
  ESP_LOGI(TAG, "scan_and_register_binary_sensors: %u registered", static_cast<unsigned>(registered));
#else
  ESP_LOGD(TAG, "scan_and_register_binary_sensors: USE_BINARY_SENSOR not defined, skipping");
#endif
}

void MatterComponent::scan_and_register_sensors_() {
#ifdef USE_SENSOR
  const size_t upper_bound = App.get_sensors().size();
  this->sensor_endpoints_.init(upper_bound);
  size_t registered = 0;
  for (auto *s : App.get_sensors()) {
    if (s == nullptr || s->is_internal()) {
      continue;
    }
    auto endpoint = std::unique_ptr<MatterSensorEndpoint>(new MatterSensorEndpoint(s));
    if (!endpoint->detect_kind()) {
      ESP_LOGD(TAG, "sensor '%s' has no Matter cluster mapping — skipping", s->get_name().c_str());
      continue;
    }
    if (!endpoint->setup()) {
      continue;
    }
    this->sensor_endpoints_.push_back(std::move(endpoint));
    registered++;
  }
  ESP_LOGI(TAG, "scan_and_register_sensors: %u registered", static_cast<unsigned>(registered));
#else
  ESP_LOGD(TAG, "scan_and_register_sensors: USE_SENSOR not defined, skipping");
#endif
}

void MatterComponent::scan_and_register_covers_() {
#ifdef USE_COVER
  const size_t upper_bound = App.get_covers().size();
  this->cover_endpoints_.init(upper_bound);
  this->cover_endpoints_by_id_.init(upper_bound);
  size_t registered = 0;
  for (auto *c : App.get_covers()) {
    if (c == nullptr || c->is_internal()) {
      continue;
    }
    auto endpoint = std::unique_ptr<MatterCoverEndpoint>(new MatterCoverEndpoint(c));
    if (!endpoint->setup()) {
      continue;
    }
    uint16_t id = endpoint->endpoint_id();
    this->cover_endpoints_by_id_.emplace_back(id, endpoint.get());
    this->cover_endpoints_.push_back(std::move(endpoint));
    registered++;
  }
  ESP_LOGI(TAG, "scan_and_register_covers: %u registered", static_cast<unsigned>(registered));
#else
  ESP_LOGD(TAG, "scan_and_register_covers: USE_COVER not defined, skipping");
#endif
}

void MatterComponent::scan_and_register_fans_() {
#ifdef USE_FAN
  const size_t upper_bound = App.get_fans().size();
  this->fan_endpoints_.init(upper_bound);
  this->fan_endpoints_by_id_.init(upper_bound);
  size_t registered = 0;
  for (auto *f : App.get_fans()) {
    if (f == nullptr || f->is_internal()) {
      continue;
    }
    auto endpoint = std::unique_ptr<MatterFanEndpoint>(new MatterFanEndpoint(f));
    if (!endpoint->setup()) {
      continue;
    }
    uint16_t id = endpoint->endpoint_id();
    this->fan_endpoints_by_id_.emplace_back(id, endpoint.get());
    this->fan_endpoints_.push_back(std::move(endpoint));
    registered++;
  }
  ESP_LOGI(TAG, "scan_and_register_fans: %u registered", static_cast<unsigned>(registered));
#else
  ESP_LOGD(TAG, "scan_and_register_fans: USE_FAN not defined, skipping");
#endif
}

void MatterComponent::scan_and_register_locks_() {
#ifdef USE_LOCK
  const size_t upper_bound = App.get_locks().size();
  this->lock_endpoints_.init(upper_bound);
  this->lock_endpoints_by_id_.init(upper_bound);
  size_t registered = 0;
  for (auto *l : App.get_locks()) {
    if (l == nullptr || l->is_internal()) {
      continue;
    }
    auto endpoint = std::unique_ptr<MatterLockEndpoint>(new MatterLockEndpoint(l));
    if (!endpoint->setup()) {
      continue;
    }
    uint16_t id = endpoint->endpoint_id();
    this->lock_endpoints_by_id_.emplace_back(id, endpoint.get());
    this->lock_endpoints_.push_back(std::move(endpoint));
    registered++;
  }
  ESP_LOGI(TAG, "scan_and_register_locks: %u registered", static_cast<unsigned>(registered));
#else
  ESP_LOGD(TAG, "scan_and_register_locks: USE_LOCK not defined, skipping");
#endif
}

void MatterComponent::scan_and_register_valves_() {
#ifdef USE_VALVE
  const size_t upper_bound = App.get_valves().size();
  this->valve_endpoints_.init(upper_bound);
  size_t registered = 0;
  for (auto *v : App.get_valves()) {
    if (v == nullptr || v->is_internal()) {
      continue;
    }
    auto endpoint = std::unique_ptr<MatterValveEndpoint>(new MatterValveEndpoint(v));
    if (!endpoint->setup()) {
      continue;
    }
    this->valve_endpoints_.push_back(std::move(endpoint));
    registered++;
  }
  ESP_LOGI(TAG, "scan_and_register_valves: %u registered", static_cast<unsigned>(registered));
#else
  ESP_LOGD(TAG, "scan_and_register_valves: USE_VALVE not defined, skipping");
#endif
}

void MatterComponent::scan_and_register_selects_() {
#ifdef USE_SELECT
  const size_t upper_bound = App.get_selects().size();
  this->select_endpoints_.init(upper_bound);
  this->select_endpoints_by_id_.init(upper_bound);
  size_t registered = 0;
  for (auto *s : App.get_selects()) {
    if (s == nullptr || s->is_internal()) {
      continue;
    }
    auto endpoint = std::unique_ptr<MatterSelectEndpoint>(new MatterSelectEndpoint(s));
    if (!endpoint->setup()) {
      continue;
    }
    uint16_t id = endpoint->endpoint_id();
    this->select_endpoints_by_id_.emplace_back(id, endpoint.get());
    this->select_endpoints_.push_back(std::move(endpoint));
    registered++;
  }
  ESP_LOGI(TAG, "scan_and_register_selects: %u registered", static_cast<unsigned>(registered));
#else
  ESP_LOGD(TAG, "scan_and_register_selects: USE_SELECT not defined, skipping");
#endif
}

void MatterComponent::scan_and_register_buttons_() {
#ifdef USE_BUTTON
  const size_t upper_bound = App.get_buttons().size();
  this->button_endpoints_.init(upper_bound);
  size_t registered = 0;
  size_t skipped_matter_action = 0;
  for (auto *b : App.get_buttons()) {
    if (b == nullptr || b->is_internal()) {
      continue;
    }
    // `button: platform: matter` entities exist to trigger local matter-
    // management actions (open commissioning window, factory reset, ...)
    // via a normal ESPHome/HA button press. Wrapping them as Matter
    // GenericSwitch endpoints would expose a fabric-visible "click me to
    // open pairing" button, which is circular (commissioners opening a
    // window on themselves) and never what the yaml author wants.
    //
    // The CHIP/esp-matter build compiles with -fno-rtti so dynamic_cast
    // is off the table; MatterActionButton keeps a function-local static
    // registry of its own instances and exposes is_instance() for O(1)
    // membership checks.
    if (MatterActionButton::is_instance(b)) {
      skipped_matter_action++;
      continue;
    }
    auto endpoint = std::unique_ptr<MatterButtonEndpoint>(new MatterButtonEndpoint(b));
    if (!endpoint->setup()) {
      continue;
    }
    this->button_endpoints_.push_back(std::move(endpoint));
    registered++;
  }
  if (skipped_matter_action > 0) {
    ESP_LOGD(TAG, "scan_and_register_buttons: skipped %u matter action button(s)",
             static_cast<unsigned>(skipped_matter_action));
  }
  ESP_LOGI(TAG, "scan_and_register_buttons: %u registered", static_cast<unsigned>(registered));
#else
  ESP_LOGD(TAG, "scan_and_register_buttons: USE_BUTTON not defined, skipping");
#endif
}

void MatterComponent::scan_and_register_lights_() {
#ifdef USE_LIGHT
  const size_t upper_bound = App.get_lights().size();
  this->light_endpoints_.init(upper_bound);
  this->light_endpoints_by_id_.init(upper_bound);
  size_t registered = 0;
  for (auto *l : App.get_lights()) {
    if (l == nullptr || l->is_internal()) {
      continue;
    }
    auto endpoint = std::unique_ptr<MatterLightEndpoint>(new MatterLightEndpoint(l));
    if (!endpoint->setup()) {
      continue;
    }
    uint16_t id = endpoint->endpoint_id();
    this->light_endpoints_by_id_.emplace_back(id, endpoint.get());
    this->light_endpoints_.push_back(std::move(endpoint));
    registered++;
  }
  ESP_LOGI(TAG, "scan_and_register_lights: %u registered", static_cast<unsigned>(registered));
#else
  ESP_LOGD(TAG, "scan_and_register_lights: USE_LIGHT not defined, skipping");
#endif
}

void MatterComponent::scan_and_register_climates_() {
#ifdef USE_CLIMATE
  const size_t upper_bound = App.get_climates().size();
  this->climate_endpoints_.init(upper_bound);
  this->climate_endpoints_by_id_.init(upper_bound);
  size_t registered = 0;
  for (auto *c : App.get_climates()) {
    if (c == nullptr || c->is_internal()) {
      continue;
    }
    auto endpoint = std::unique_ptr<MatterClimateEndpoint>(new MatterClimateEndpoint(c));
    if (!endpoint->setup()) {
      continue;
    }
    uint16_t id = endpoint->endpoint_id();
    this->climate_endpoints_by_id_.emplace_back(id, endpoint.get());
    this->climate_endpoints_.push_back(std::move(endpoint));
    registered++;
  }
  ESP_LOGI(TAG, "scan_and_register_climates: %u registered", static_cast<unsigned>(registered));
#else
  ESP_LOGD(TAG, "scan_and_register_climates: USE_CLIMATE not defined, skipping");
#endif
}

void MatterComponent::push_initial_states_() {
#ifdef USE_SWITCH
  for (auto &endpoint : this->switch_endpoints_) {
    endpoint->push_initial_state();
  }
#endif
#ifdef USE_BINARY_SENSOR
  for (auto &endpoint : this->binary_sensor_endpoints_) {
    endpoint->push_initial_state();
  }
#endif
#ifdef USE_SENSOR
  for (auto &endpoint : this->sensor_endpoints_) {
    endpoint->push_initial_state();
  }
#endif
#ifdef USE_COVER
  for (auto &endpoint : this->cover_endpoints_) {
    endpoint->push_initial_state();
  }
#endif
#ifdef USE_FAN
  for (auto &endpoint : this->fan_endpoints_) {
    endpoint->push_initial_state();
  }
#endif
#ifdef USE_LOCK
  for (auto &endpoint : this->lock_endpoints_) {
    endpoint->push_initial_state();
  }
#endif
#ifdef USE_VALVE
  for (auto &endpoint : this->valve_endpoints_) {
    endpoint->push_initial_state();
  }
#endif
#ifdef USE_SELECT
  for (auto &endpoint : this->select_endpoints_) {
    endpoint->push_initial_state();
  }
#endif
#ifdef USE_BUTTON
  for (auto &endpoint : this->button_endpoints_) {
    endpoint->push_initial_state();
  }
#endif
#ifdef USE_LIGHT
  for (auto &endpoint : this->light_endpoints_) {
    endpoint->push_initial_state();
  }
#endif
#ifdef USE_CLIMATE
  for (auto &endpoint : this->climate_endpoints_) {
    endpoint->push_initial_state();
  }
#endif
}

void MatterComponent::log_onboarding_payload_() {
  // Build the SetupPayload and use CHIP's generators to produce the
  // formatted strings a commissioner actually accepts: an 11-digit manual
  // pairing code (with Verhoeff check digit) and the "MT:...." QR string.
  ::chip::SetupPayload payload;
  payload.version = 0;
  payload.vendorID = this->vendor_id_;
  payload.productID = this->product_id_;
  payload.commissioningFlow = ::chip::CommissioningFlow::kStandard;
  // Rendezvous flags advertise which transports a commissioner can use to
  // reach us. kBLE is added only when BLE commissioning is compiled in — a
  // controller that trusts the payload would otherwise waste airtime BLE-
  // scanning for a device that will never answer. Both flags can be OR'd:
  // controllers try BLE first for a device provisioning Wi-Fi credentials
  // and fall back to on-network for an already-joined or Ethernet device.
  ::chip::RendezvousInformationFlags flags(::chip::RendezvousInformationFlag::kOnNetwork);
  if (this->ble_commissioning_) {
    flags.Set(::chip::RendezvousInformationFlag::kBLE);
  }
  payload.rendezvousInformation.SetValue(flags);
  payload.discriminator.SetLongValue(this->discriminator_);
  payload.setUpPINCode = static_cast<uint32_t>(strtoul(this->setup_code_.c_str(), nullptr, 10));

  std::string manual_code;
  ::chip::ManualSetupPayloadGenerator manual_gen(payload);
  CHIP_ERROR manual_err = manual_gen.payloadDecimalStringRepresentation(manual_code);

  std::string qr_code;
  ::chip::QRCodeSetupPayloadGenerator qr_gen(payload);
  CHIP_ERROR qr_err = qr_gen.payloadBase38Representation(qr_code);

  ESP_LOGI(TAG, "═════════════════════════════════════════════");
  ESP_LOGI(TAG, "  Matter onboarding — commission via HA:");
  if (manual_err == CHIP_NO_ERROR) {
    ESP_LOGI(TAG, "    Manual pairing code:  %s", manual_code.c_str());
  } else {
    ESP_LOGW(TAG, "    Manual pairing code: <failed err=%s>", manual_err.AsString());
  }
  if (qr_err == CHIP_NO_ERROR) {
    ESP_LOGI(TAG, "    QR code payload:      %s", qr_code.c_str());
  } else {
    ESP_LOGW(TAG, "    QR code payload: <failed err=%s>", qr_err.AsString());
  }
  ESP_LOGI(TAG, "    Setup passcode:       %u (raw)", static_cast<unsigned>(payload.setUpPINCode));
  ESP_LOGI(TAG, "    Discriminator:        %u", this->discriminator_);
  ESP_LOGI(TAG, "    Vendor / Product:     0x%04X / 0x%04X", this->vendor_id_, this->product_id_);
  ESP_LOGI(TAG, "═════════════════════════════════════════════");
}

void MatterComponent::open_commissioning_window(uint32_t timeout_seconds) {
  // Clamp timeout to the range the Matter spec accepts for OpenCommissioning
  // Window (§11.19.9.1). Values outside [180, 900] are rejected by the CHIP
  // CommissioningWindowManager anyway; clamp early so the caller doesn't have
  // to know the exact bound.
  if (timeout_seconds < 180) {
    timeout_seconds = 180;
  } else if (timeout_seconds > 900) {
    timeout_seconds = 900;
  }

  // OpenEnhancedCommissioningWindow touches the CommissioningWindowManager,
  // the DNSSD advertiser, and the CHIP fabric table — all owned by the
  // PlatformManager event-loop task. Calling from any other thread (ESPHome
  // main loop, an API callback, a lambda in on_press) races those subsystems
  // and can trip asserts or leave the window in a half-open state. Hop over
  // via ScheduleWork so the impl runs on the CHIP task even if we're called
  // from the ESPHome main loop.
  //
  // Unlike the keepalive tick, this is a user-visible action — dropping it
  // silently would leave the operator pressing the pairing button with
  // nothing happening. Log at ERROR so a saturated event queue is
  // diagnosable.
  CHIP_ERROR err = ::chip::DeviceLayer::PlatformMgr().ScheduleWork(
      [](intptr_t arg) {
        auto *self = MatterComponent::instance();
        if (self == nullptr) {
          return;
        }
        self->open_commissioning_window_impl_(static_cast<uint32_t>(arg));
      },
      static_cast<intptr_t>(timeout_seconds));
  if (err != CHIP_NO_ERROR) {
    ESP_LOGE(TAG, "open_commissioning_window: ScheduleWork failed: %" CHIP_ERROR_FORMAT, err.Format());
  }
}

void MatterComponent::open_commissioning_window_impl_(uint32_t timeout_seconds) {
  // Passcodes reserved by Matter spec §5.1.1.6 — trivial patterns that would
  // let a casual observer guess the code. Generator loops until it produces
  // something outside this set.
  static constexpr uint32_t RESERVED_PASSCODES[] = {
      00000000, 11111111, 22222222, 33333333, 44444444, 55555555,
      66666666, 77777777, 88888888, 99999999, 12345678, 87654321,
  };
  const auto is_reserved = [](uint32_t code) {
    for (uint32_t reserved : RESERVED_PASSCODES) {
      if (code == reserved)
        return true;
    }
    return false;
  };

  // Draw a passcode in [1, 99'999'998] avoiding the reserved patterns.
  uint32_t passcode = 0;
  for (int attempt = 0; attempt < 32; attempt++) {
    uint32_t raw = 0;
    if (::chip::Crypto::DRBG_get_bytes(reinterpret_cast<uint8_t *>(&raw), sizeof(raw)) != CHIP_NO_ERROR) {
      ESP_LOGE(TAG, "DRBG_get_bytes failed while generating passcode");
      return;
    }
    passcode = (raw % 99999998U) + 1U;
    if (!is_reserved(passcode))
      break;
  }
  if (is_reserved(passcode)) {
    ESP_LOGE(TAG, "unable to generate a non-reserved passcode after 32 attempts");
    return;
  }

  // Fresh 12-bit discriminator so the new window's DNSSD/BLE advertisement
  // doesn't collide with the yaml-configured one (which the fabric still
  // remembers). Top 4 bits masked out (Matter discriminator is 12 bits).
  uint16_t discriminator = 0;
  if (::chip::Crypto::DRBG_get_bytes(reinterpret_cast<uint8_t *>(&discriminator), sizeof(discriminator)) !=
      CHIP_NO_ERROR) {
    ESP_LOGE(TAG, "DRBG_get_bytes failed while generating discriminator");
    return;
  }
  discriminator &= 0x0FFF;

  // Salt: 16 bytes, Matter spec §5.1.1.4 minimum. Iterations 15000 — CHIP's
  // OpenCommissioningWindow accepts 1000..100'000 and 15k is the common
  // esp-matter default (matches what the manufacturer setup code uses).
  uint8_t salt_bytes[16];
  if (::chip::Crypto::DRBG_get_bytes(salt_bytes, sizeof(salt_bytes)) != CHIP_NO_ERROR) {
    ESP_LOGE(TAG, "DRBG_get_bytes failed while generating salt");
    return;
  }
  ::chip::ByteSpan salt(salt_bytes, sizeof(salt_bytes));
  constexpr uint32_t iterations = 15000;

  // Derive the Spake2p verifier — the CommissioningWindowManager only stores
  // the verifier, never the passcode itself.
  ::chip::Crypto::Spake2pVerifier verifier;
  CHIP_ERROR err = verifier.Generate(iterations, salt, passcode);
  if (err != CHIP_NO_ERROR) {
    ESP_LOGE(TAG, "Spake2pVerifier::Generate failed: %s", err.AsString());
    return;
  }

  // Open the window. This CHIP build takes 7 arguments including the
  // initiating admin's fabric index and vendor id — used to gate the
  // subsequent commissioning attempt (e.g. an OpenCommissioningWindow
  // command from an existing admin remembers who asked for it). Since
  // we're triggering locally from the device itself (no fabric context
  // yet — the yaml button author might not even have a first fabric
  // when they press it), pass `kUndefinedFabricIndex` and our own
  // vendor id. The manager accepts this pair on ESP32.
  err = ::chip::Server::GetInstance().GetCommissioningWindowManager().OpenEnhancedCommissioningWindow(
      ::chip::System::Clock::Seconds32(timeout_seconds), discriminator, verifier, iterations, salt,
      ::chip::kUndefinedFabricIndex, static_cast<::chip::VendorId>(this->vendor_id_));
  if (err != CHIP_NO_ERROR) {
    ESP_LOGE(TAG, "OpenEnhancedCommissioningWindow failed: %s", err.AsString());
    return;
  }

  // Compute a pairing code + QR for the operator to type/scan in the second
  // controller's add-device flow. Same generator we use at boot — different
  // input values (fresh passcode/discriminator, kOnNetwork instead of the
  // possibly-BLE flags since the device is already on the LAN by now).
  ::chip::SetupPayload payload;
  payload.version = 0;
  payload.vendorID = this->vendor_id_;
  payload.productID = this->product_id_;
  payload.commissioningFlow = ::chip::CommissioningFlow::kStandard;
  ::chip::RendezvousInformationFlags flags(::chip::RendezvousInformationFlag::kOnNetwork);
  payload.rendezvousInformation.SetValue(flags);
  payload.discriminator.SetLongValue(discriminator);
  payload.setUpPINCode = passcode;

  std::string manual_code;
  CHIP_ERROR manual_err = ::chip::ManualSetupPayloadGenerator(payload).payloadDecimalStringRepresentation(manual_code);
  std::string qr_code;
  CHIP_ERROR qr_err = ::chip::QRCodeSetupPayloadGenerator(payload).payloadBase38Representation(qr_code);

  ESP_LOGI(TAG, "═════════════════════════════════════════════");
  ESP_LOGI(TAG, "  Enhanced Commissioning Window open for %us:", static_cast<unsigned>(timeout_seconds));
  if (manual_err == CHIP_NO_ERROR) {
    ESP_LOGI(TAG, "    Manual pairing code:  %s", manual_code.c_str());
  } else {
    ESP_LOGE(TAG, "    Manual pairing code: <failed err=%s>", manual_err.AsString());
  }
  if (qr_err == CHIP_NO_ERROR) {
    ESP_LOGI(TAG, "    QR code payload:      %s", qr_code.c_str());
  } else {
    ESP_LOGE(TAG, "    QR code payload: <failed err=%s>", qr_err.AsString());
  }
  // If both string generators failed, the operator has no way to reach the
  // window that just opened. Log at ERROR — the passcode still prints below
  // so a tech-savvy user could reconstruct the pairing string by hand, but
  // this is a bug worth surfacing loudly.
  if (manual_err != CHIP_NO_ERROR && qr_err != CHIP_NO_ERROR) {
    ESP_LOGE(TAG, "    Neither pairing representation could be generated — "
                  "operator cannot commission through the window that just opened");
  }
  ESP_LOGI(TAG, "    New passcode:         %u", static_cast<unsigned>(passcode));
  ESP_LOGI(TAG, "    New discriminator:    %u", static_cast<unsigned>(discriminator));
  ESP_LOGI(TAG, "═════════════════════════════════════════════");
}

namespace {

// Static payload pool for defer_attribute_update. Sized to comfortably absorb
// the worst plausible burst on a large bridge — a few dozen entities changing
// state in the same ESPHome scheduler tick — while staying well under 1 KB
// of BSS. Overflow is soft-fail: the caller logs and drops the update rather
// than blocking the main loop or growing on the heap after setup().
constexpr size_t ATTRIBUTE_UPDATE_POOL_SIZE = 32;

struct DeferredAttributeUpdateSlot {
  uint16_t endpoint_id;
  uint32_t cluster_id;
  uint32_t attribute_id;
  ::esp_matter_attr_val_t val;
  // Owned == true → payload is either queued in ScheduleWork or being executed
  // on the CHIP task. Released back to the pool by the ScheduleWork lambda
  // once ::esp_matter::attribute::update returns. Acquire/release ordering
  // pairs the store-with-payload (writer) against the load-then-read (worker).
  std::atomic<bool> in_use{false};
};

// BSS. Payload is written by ESPHome scheduler thread(s), read by the CHIP
// task; the atomic in_use flag is the only cross-thread synchronization,
// which is sufficient because the writer completes the payload BEFORE the
// ScheduleWork call and the worker only touches its slot AFTER dequeue.
DeferredAttributeUpdateSlot g_attribute_update_pool[ATTRIBUTE_UPDATE_POOL_SIZE];

}  // namespace

void MatterComponent::defer_attribute_update(uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id,
                                             const ::esp_matter_attr_val &val) {
  for (size_t i = 0; i < ATTRIBUTE_UPDATE_POOL_SIZE; i++) {
    bool expected = false;
    if (!g_attribute_update_pool[i].in_use.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
      continue;
    }
    auto &slot = g_attribute_update_pool[i];
    slot.endpoint_id = endpoint_id;
    slot.cluster_id = cluster_id;
    slot.attribute_id = attribute_id;
    slot.val = val;
    const CHIP_ERROR err = ::chip::DeviceLayer::PlatformMgr().ScheduleWork(
        [](intptr_t arg) {
          auto &s = g_attribute_update_pool[static_cast<size_t>(arg)];
          // Runs on the CHIP task with the stack lock already held by the
          // scheduler dispatcher; ::esp_matter::attribute::update's own
          // ScopedChipStackLock is re-entrant and returns immediately.
          const esp_err_t rc = ::esp_matter::attribute::update(s.endpoint_id, s.cluster_id, s.attribute_id, &s.val);
          if (rc != ESP_OK) {
            ESP_LOGW(TAG, "deferred attribute::update endpoint=%u cluster=0x%08lx attr=0x%08lx failed: %s",
                     s.endpoint_id, static_cast<unsigned long>(s.cluster_id),
                     static_cast<unsigned long>(s.attribute_id), esp_err_to_name(rc));
          }
          s.in_use.store(false, std::memory_order_release);
        },
        static_cast<intptr_t>(i));
    if (err != CHIP_NO_ERROR) {
      ESP_LOGW(TAG, "ScheduleWork(attribute::update) endpoint=%u cluster=0x%08lx attr=0x%08lx failed: %s", endpoint_id,
               static_cast<unsigned long>(cluster_id), static_cast<unsigned long>(attribute_id), err.AsString());
      slot.in_use.store(false, std::memory_order_release);
    }
    return;
  }
  ESP_LOGW(TAG,
           "attribute-update pool exhausted (%u in flight) — dropping update endpoint=%u cluster=0x%08lx attr=0x%08lx",
           static_cast<unsigned>(ATTRIBUTE_UPDATE_POOL_SIZE), endpoint_id, static_cast<unsigned long>(cluster_id),
           static_cast<unsigned long>(attribute_id));
}

}  // namespace esphome::matter

#endif  // USE_ESP_IDF && USE_MATTER_VARIANT_SUPPORTED
