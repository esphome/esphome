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
#ifdef USE_BUTTON

#include "matter_button_endpoint.h"
#include "matter_component.h"

#include "esphome/core/log.h"
#include "esphome/components/button/button.h"

#include <esp_matter.h>

#include <app-common/zap-generated/cluster-objects.h>
#include <platform/CHIPDeviceLayer.h>

namespace esphome::matter {

static const char *const TAG = "matter.button";

namespace {

// Matter Switch cluster feature bits (from Switch/Enums.h). MS + MSR are the
// minimum for InitialPress + ShortRelease.
constexpr uint32_t FEATURE_MOMENTARY_SWITCH = 0x2;
constexpr uint32_t FEATURE_MOMENTARY_SWITCH_RELEASE = 0x4;

}  // namespace

MatterButtonEndpoint::MatterButtonEndpoint(::esphome::button::Button *button) : button_(button) {}

bool MatterButtonEndpoint::setup() {
  ::esp_matter::node_t *node = ::esp_matter::node::get();
  if (node == nullptr) {
    ESP_LOGE(TAG, "no Matter node available for button '%s'", this->button_->get_name().c_str());
    return false;
  }

  ::esp_matter::endpoint::generic_switch::config_t config;
  config.switch_cluster.number_of_positions = 2;  // released (0), pressed (1)
  config.switch_cluster.current_position = 0;     // start released
  // Switch cluster asserts VALIDATE_FEATURES_EXACT_ONE(Latching, Momentary) —
  // MS alone satisfies that. Add MSR to enable ShortRelease events.
  config.switch_cluster.feature_flags = FEATURE_MOMENTARY_SWITCH | FEATURE_MOMENTARY_SWITCH_RELEASE;

  ::esp_matter::endpoint_t *endpoint =
      ::esp_matter::endpoint::generic_switch::create(node, &config, ::esp_matter::ENDPOINT_FLAG_NONE, this);
  if (endpoint == nullptr) {
    ESP_LOGE(TAG, "failed to create generic_switch endpoint for '%s'", this->button_->get_name().c_str());
    return false;
  }
  this->endpoint_id_ = ::esp_matter::endpoint::get_id(endpoint);

  MatterComponent::instance()->register_endpoint_label(endpoint, this->endpoint_id_, this->button_->get_name());

  // Device → fabric: emit press+release cycle whenever the button fires.
  this->button_->add_on_press_callback([this]() {
    ESP_LOGD(TAG, "device press → fabric: endpoint=%u button='%s'", this->endpoint_id_,
             this->button_->get_name().c_str());
    this->emit_press_cycle_();
  });

  ESP_LOGI(TAG, "registered button '%s' as Matter generic_switch endpoint %u", this->button_->get_name().c_str(),
           this->endpoint_id_);
  return true;
}

void MatterButtonEndpoint::emit_press_cycle_() {
  const uint16_t ep = this->endpoint_id_;

  // Switch events go through CHIP's EventManagement::LogEvent which asserts
  // the CHIP stack lock. We're firing from the ESPHome loop task (interval
  // → button.press → this callback), so blocking the loop on
  // ScopedChipStackLock(portMAX_DELAY) can stall for the full duration of
  // an in-progress subscription-report encode (1–3 s on a busy bridge) —
  // long enough to trip the task watchdog on the ESPHome side. Marshal the
  // whole 4-step press/release cycle onto the CHIP task via ScheduleWork:
  // the dispatcher runs with the stack lock already held, so both
  // attribute::update() and the send_* event helpers are safe there.
  const CHIP_ERROR err = ::chip::DeviceLayer::PlatformMgr().ScheduleWork(
      [](intptr_t arg) {
        const uint16_t endpoint_id = static_cast<uint16_t>(arg);
        constexpr uint8_t pressed = 1;
        constexpr uint8_t released = 0;

        // 1) CurrentPosition = 1 (pressed) — spec §1.13.5.2
        ::esp_matter_attr_val_t v_pressed = ::esp_matter_uint8(pressed);
        esp_err_t rc =
            ::esp_matter::attribute::update(endpoint_id, chip::app::Clusters::Switch::Id,
                                            chip::app::Clusters::Switch::Attributes::CurrentPosition::Id, &v_pressed);
        if (rc != ESP_OK) {
          ESP_LOGW(TAG, "attribute::update CurrentPosition=1 endpoint=%u failed: %s", endpoint_id, esp_err_to_name(rc));
        }

        // 2) InitialPress(newPosition=1) event
        rc = ::esp_matter::cluster::switch_cluster::event::send_initial_press(endpoint_id, pressed);
        if (rc != ESP_OK) {
          ESP_LOGW(TAG, "send_initial_press endpoint=%u failed: %s", endpoint_id, esp_err_to_name(rc));
        }

        // 3) CurrentPosition = 0 (released)
        ::esp_matter_attr_val_t v_released = ::esp_matter_uint8(released);
        rc = ::esp_matter::attribute::update(endpoint_id, chip::app::Clusters::Switch::Id,
                                             chip::app::Clusters::Switch::Attributes::CurrentPosition::Id, &v_released);
        if (rc != ESP_OK) {
          ESP_LOGW(TAG, "attribute::update CurrentPosition=0 endpoint=%u failed: %s", endpoint_id, esp_err_to_name(rc));
        }

        // 4) ShortRelease(previousPosition=1) event — the emitted `newPosition`
        // parameter on send_short_release is actually the *previous* position
        // (the one held while pressed) per Matter 1.4 spec §1.13.6.4.
        rc = ::esp_matter::cluster::switch_cluster::event::send_short_release(endpoint_id, pressed);
        if (rc != ESP_OK) {
          ESP_LOGW(TAG, "send_short_release endpoint=%u failed: %s", endpoint_id, esp_err_to_name(rc));
        }
      },
      static_cast<intptr_t>(ep));
  if (err != CHIP_NO_ERROR) {
    ESP_LOGW(TAG, "ScheduleWork(button press) endpoint=%u failed: %s — dropping this event", ep, err.AsString());
  }
}

}  // namespace esphome::matter

#endif  // USE_BUTTON
#endif  // USE_ESP_IDF && USE_MATTER_VARIANT_SUPPORTED
