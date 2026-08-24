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
#ifdef USE_LIGHT

#include "matter_light_endpoint.h"
#include "matter_component.h"

#include "esphome/core/log.h"
#include "esphome/components/light/light_state.h"
#include "esphome/components/light/light_call.h"
#include "esphome/components/light/light_traits.h"
#include "esphome/components/light/color_mode.h"

#include <esp_matter.h>

#include <app-common/zap-generated/cluster-objects.h>

#include <algorithm>
#include <cmath>

namespace esphome::matter {

static const char *const TAG = "matter.light";

MatterLightEndpoint::MatterLightEndpoint(::esphome::light::LightState *light) : light_(light) {}

MatterLightEndpoint::DeviceKind MatterLightEndpoint::detect_device_kind_() const {
  auto traits = this->light_->get_traits();
  const bool has_rgb = traits.supports_color_capability(::esphome::light::ColorCapability::RGB);
  const bool has_ct = traits.supports_color_capability(::esphome::light::ColorCapability::COLOR_TEMPERATURE);
  const bool has_brightness = traits.supports_color_capability(::esphome::light::ColorCapability::BRIGHTNESS);
  if (has_rgb) {
    return DeviceKind::DEVICE_KIND_EXTENDED_COLOR;
  }
  if (has_ct) {
    return DeviceKind::DEVICE_KIND_COLOR_TEMPERATURE;
  }
  if (has_brightness) {
    return DeviceKind::DEVICE_KIND_DIMMABLE;
  }
  return DeviceKind::DEVICE_KIND_ON_OFF;
}

bool MatterLightEndpoint::setup() {
  ::esp_matter::node_t *node = ::esp_matter::node::get();
  if (node == nullptr) {
    ESP_LOGE(TAG, "no Matter node available for light '%s'", this->light_->get_name().c_str());
    return false;
  }

  this->device_kind_ = this->detect_device_kind_();
  auto traits = this->light_->get_traits();

  // Snap initial state values off remote_values so the endpoint boots with
  // the right OnOff/brightness/CT displayed to the fabric.
  const auto &rv = this->light_->remote_values;
  const bool initial_on = rv.is_on();
  // remote_values.brightness is 0..1; Matter CurrentLevel is 1..254 (0 is
  // reserved). Clamp min to 1 for lit states to avoid the "off but level
  // reports 0" ambiguity.
  uint8_t initial_level = static_cast<uint8_t>(std::clamp(std::lround(rv.get_brightness() * 254.0f), 1L, 254L));
  // Convert ESPHome's normalized color_temperature (0..1 mapping min..max
  // mireds) into an absolute mireds value.
  const float min_m = traits.get_min_mireds();
  const float max_m = traits.get_max_mireds();
  uint16_t initial_mireds = 0;
  if (max_m > min_m) {
    // remote_values.get_color_temperature() returns absolute mireds already
    // (float). Round to uint16.
    float ct = rv.get_color_temperature();
    if (ct < min_m)
      ct = min_m;
    if (ct > max_m)
      ct = max_m;
    initial_mireds = static_cast<uint16_t>(std::lround(ct));
  }

  ::esp_matter::endpoint_t *endpoint = nullptr;
  switch (this->device_kind_) {
    case DeviceKind::DEVICE_KIND_ON_OFF: {
      ::esp_matter::endpoint::on_off_light::config_t config;
      config.on_off.on_off = initial_on;
      endpoint = ::esp_matter::endpoint::on_off_light::create(node, &config, ::esp_matter::ENDPOINT_FLAG_NONE, this);
      break;
    }
    case DeviceKind::DEVICE_KIND_DIMMABLE: {
      ::esp_matter::endpoint::dimmable_light::config_t config;
      config.on_off.on_off = initial_on;
      config.level_control.current_level = initial_level;
      endpoint = ::esp_matter::endpoint::dimmable_light::create(node, &config, ::esp_matter::ENDPOINT_FLAG_NONE, this);
      break;
    }
    case DeviceKind::DEVICE_KIND_COLOR_TEMPERATURE: {
      ::esp_matter::endpoint::color_temperature_light::config_t config;
      config.on_off.on_off = initial_on;
      config.level_control.current_level = initial_level;
      // CT feature config lives at endpoint scope as a peer to color_control,
      // NOT nested inside color_control (see esp_matter_endpoint.h:300-306).
      config.color_control_color_temperature.color_temperature_mireds = initial_mireds;
      if (max_m > 0) {
        config.color_control_color_temperature.color_temp_physical_min_mireds =
            static_cast<uint16_t>(std::lround(min_m));
        config.color_control_color_temperature.color_temp_physical_max_mireds =
            static_cast<uint16_t>(std::lround(max_m));
      }
      endpoint = ::esp_matter::endpoint::color_temperature_light::create(node, &config,
                                                                         ::esp_matter::ENDPOINT_FLAG_NONE, this);
      break;
    }
    case DeviceKind::DEVICE_KIND_EXTENDED_COLOR: {
      ::esp_matter::endpoint::extended_color_light::config_t config;
      config.on_off.on_off = initial_on;
      config.level_control.current_level = initial_level;
      if (max_m > 0) {
        config.color_control_color_temperature.color_temperature_mireds = initial_mireds;
        config.color_control_color_temperature.color_temp_physical_min_mireds =
            static_cast<uint16_t>(std::lround(min_m));
        config.color_control_color_temperature.color_temp_physical_max_mireds =
            static_cast<uint16_t>(std::lround(max_m));
      }
      endpoint =
          ::esp_matter::endpoint::extended_color_light::create(node, &config, ::esp_matter::ENDPOINT_FLAG_NONE, this);
      break;
    }
  }

  if (endpoint == nullptr) {
    ESP_LOGE(TAG, "failed to create light endpoint for '%s'", this->light_->get_name().c_str());
    return false;
  }
  this->endpoint_id_ = ::esp_matter::endpoint::get_id(endpoint);

  MatterComponent::instance()->register_endpoint_label(endpoint, this->endpoint_id_, this->light_->get_name());

  this->light_->add_remote_values_listener(this);

  const char *kind_name = "on_off";
  switch (this->device_kind_) {
    case DeviceKind::DEVICE_KIND_ON_OFF:
      kind_name = "on_off";
      break;
    case DeviceKind::DEVICE_KIND_DIMMABLE:
      kind_name = "dimmable";
      break;
    case DeviceKind::DEVICE_KIND_COLOR_TEMPERATURE:
      kind_name = "color_temperature";
      break;
    case DeviceKind::DEVICE_KIND_EXTENDED_COLOR:
      kind_name = "extended_color";
      break;
  }
  ESP_LOGI(TAG, "registered light '%s' as Matter %s_light endpoint %u", this->light_->get_name().c_str(), kind_name,
           this->endpoint_id_);
  return true;
}

void MatterLightEndpoint::on_light_remote_values_update() {
  if (this->applying_matter_write_) {
    ESP_LOGV(TAG, "remote values listener suppressed (matter-driven change) endpoint=%u", this->endpoint_id_);
    return;
  }
  ESP_LOGD(TAG, "device state change → fabric: endpoint=%u on=%d br=%.2f ct=%.1f light='%s'", this->endpoint_id_,
           static_cast<int>(this->light_->remote_values.is_on()), this->light_->remote_values.get_brightness(),
           this->light_->remote_values.get_color_temperature(), this->light_->get_name().c_str());
  this->report_state_to_fabric_();
}

// All three on_matter_*_write handlers run on the CHIP PlatformManager
// task (esp-matter invokes attribute_update_cb from there), NOT the
// ESPHome main loop. Touching LightState::make_call().perform() off-loop
// races the transition state machine and eventually crashes in
// LightTransformer::setup with a null-deref (observed after a handful
// of rapid toggles on the color_temperature_light path — LevelControl's
// own tick callback re-fires Set() from the CHIP thread while the main
// loop is still animating the previous transition, and the two threads
// concurrently mutate LightState internals). Route through the
// MatterComponent's `defer_on_main_loop` so the ESPHome-side call always
// executes on the main thread. `applying_matter_write_` is set inside
// the deferred lambda too — it only exists to break the round-trip
// echo, and the listener runs on the main loop, so scoping the flag to
// the deferred block is both sufficient and correct.

void MatterLightEndpoint::on_matter_on_off_write(bool state) {
  ESP_LOGD(TAG, "matter OnOff write endpoint=%u state=%d light='%s'", this->endpoint_id_, static_cast<int>(state),
           this->light_->get_name().c_str());
  MatterComponent::instance()->defer_on_main_loop([this, state]() {
    this->applying_matter_write_ = true;
    auto call = this->light_->make_call();
    call.set_state(state);
    call.perform();
    this->applying_matter_write_ = false;
  });
}

void MatterLightEndpoint::on_matter_level_write(uint8_t level) {
  ESP_LOGD(TAG, "matter LevelControl.CurrentLevel write endpoint=%u level=%u light='%s'", this->endpoint_id_,
           static_cast<unsigned>(level), this->light_->get_name().c_str());
  // Do NOT derive on/off state from level: the OnOff cluster's Lighting
  // feature writes CurrentLevel as a side effect of On/Off transitions
  // (StartUpCurrentLevel / OnLevel semantics). Treating any level>0 write as
  // "turn on" makes Off commands immediately re-toggle to On because CHIP
  // stamps CurrentLevel back to the pre-off value right after OnOff=0.
  // MoveToLevelWithOnOff and other "with OnOff" commands cause CHIP to write
  // OnOff.OnOff separately, so our OnOff dispatch already catches that path.
  if (level == 0) {
    // A bare CurrentLevel=0 write means "no brightness set" — leave the
    // ESPHome side alone rather than driving brightness to zero (which would
    // conflict with the Lighting feature's OnLevel bookkeeping).
    return;
  }
  MatterComponent::instance()->defer_on_main_loop([this, level]() {
    this->applying_matter_write_ = true;
    auto call = this->light_->make_call();
    float brightness = static_cast<float>(level) / 254.0f;
    if (brightness > 1.0f)
      brightness = 1.0f;
    call.set_brightness_if_supported(brightness);
    call.perform();
    this->applying_matter_write_ = false;
  });
}

void MatterLightEndpoint::on_matter_color_temp_write(uint16_t mireds) {
  ESP_LOGD(TAG, "matter ColorTemperatureMireds write endpoint=%u mireds=%u light='%s'", this->endpoint_id_,
           static_cast<unsigned>(mireds), this->light_->get_name().c_str());
  MatterComponent::instance()->defer_on_main_loop([this, mireds]() {
    this->applying_matter_write_ = true;
    auto call = this->light_->make_call();
    call.set_color_temperature_if_supported(static_cast<float>(mireds));
    call.perform();
    this->applying_matter_write_ = false;
  });
}

void MatterLightEndpoint::push_initial_state() { this->report_state_to_fabric_(); }

void MatterLightEndpoint::report_state_to_fabric_() {
  const auto &rv = this->light_->remote_values;
  ApplyingReportGuard applying_report_guard(this->applying_report_);

  // OnOff.OnOff
  ::esp_matter_attr_val_t v_on = ::esp_matter_bool(rv.is_on());
  MatterComponent::instance()->defer_attribute_update(this->endpoint_id_, chip::app::Clusters::OnOff::Id,
                                                      chip::app::Clusters::OnOff::Attributes::OnOff::Id, v_on);

  if (this->device_kind_ != DeviceKind::DEVICE_KIND_ON_OFF) {
    // LevelControl.CurrentLevel handling. The Lighting feature (LT) — which
    // color_temperature_light / dimmable_light / extended_color_light all
    // set — constrains CurrentLevel to [min_level, max_level] = [1, 254]
    // (spec §1.6.6.2). Matter spec permits null while OnOff=false, but
    // esp-matter/CHIP's Lighting-feature OnOff transition handler reads
    // CurrentLevel during On→Off to save it as OnLevel (the restore-to
    // value used on the next On). If it finds null, it aborts the
    // transition with `ERR: Current Level is null` and the Off command
    // silently fails on the client side.
    //
    // Workaround: track the last observed non-zero brightness in
    // last_known_level_ and always publish a non-null CurrentLevel, even
    // while off. The fabric sees a stale-but-valid level during off; CHIP
    // gets what it needs to bookkeep the transition; and the next On
    // command still drives brightness from the ESPHome light's own
    // remote_values (we set the level via a subsequent LevelControl write
    // if the controller sends one, or leave it at the last observed value).
    float brightness = rv.get_brightness();
    if (rv.is_on() && brightness > 0.0f) {
      this->last_known_level_ = static_cast<uint8_t>(std::clamp(std::lround(brightness * 254.0f), 1L, 254L));
    }
    ::esp_matter_attr_val_t v_level = ::esp_matter_nullable_uint8(::nullable<uint8_t>(this->last_known_level_));
    MatterComponent::instance()->defer_attribute_update(this->endpoint_id_, chip::app::Clusters::LevelControl::Id,
                                                        chip::app::Clusters::LevelControl::Attributes::CurrentLevel::Id,
                                                        v_level);
  }

  if (this->device_kind_ == DeviceKind::DEVICE_KIND_COLOR_TEMPERATURE ||
      this->device_kind_ == DeviceKind::DEVICE_KIND_EXTENDED_COLOR) {
    auto traits = this->light_->get_traits();
    const float min_m = traits.get_min_mireds();
    const float max_m = traits.get_max_mireds();
    float ct = rv.get_color_temperature();
    if (max_m > min_m) {
      if (ct < min_m)
        ct = min_m;
      if (ct > max_m)
        ct = max_m;
    }
    uint16_t mireds = static_cast<uint16_t>(std::lround(ct));
    ::esp_matter_attr_val_t v_ct = ::esp_matter_uint16(mireds);
    MatterComponent::instance()->defer_attribute_update(
        this->endpoint_id_, chip::app::Clusters::ColorControl::Id,
        chip::app::Clusters::ColorControl::Attributes::ColorTemperatureMireds::Id, v_ct);
    // Color XY reporting (for extended_color_light) intentionally skipped
    // in this pass — see header comment.
  }
}

}  // namespace esphome::matter

#endif  // USE_LIGHT
#endif  // USE_ESP_IDF && USE_MATTER_VARIANT_SUPPORTED
