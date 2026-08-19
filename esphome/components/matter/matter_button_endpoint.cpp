#include "esphome/core/defines.h"

#ifdef USE_ESP_IDF
#ifdef USE_BUTTON

#include "matter_button_endpoint.h"
#include "matter_component.h"

#include "esphome/core/log.h"
#include "esphome/components/button/button.h"

#include <esp_matter.h>

#include <app-common/zap-generated/cluster-objects.h>

namespace esphome {
namespace matter {

static const char *const TAG = "matter.button";

namespace {

// Matter Switch cluster feature bits (from Switch/Enums.h). MS + MSR are the
// minimum for InitialPress + ShortRelease.
constexpr uint32_t kFeatureMomentarySwitch = 0x2;
constexpr uint32_t kFeatureMomentarySwitchRelease = 0x4;

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
  config.switch_cluster.feature_flags = kFeatureMomentarySwitch | kFeatureMomentarySwitchRelease;

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
  constexpr uint8_t kPressed = 1;
  constexpr uint8_t kReleased = 0;

  // Switch events go through CHIP's EventManagement::LogEvent which asserts
  // the CHIP stack lock. We're firing from the ESPHome loop task (interval
  // → button.press → this callback), NOT from the CHIP task, so we must take
  // the lock ourselves — matter_binary_sensor_endpoint uses the same
  // ScopedChipStackLock around its own event pushes for the same reason.
  // Attribute updates via esp_matter::attribute::update() take the lock
  // internally, but event helpers do not.
  ::esp_matter::lock::ScopedChipStackLock stack_lock(portMAX_DELAY);

  // 1) CurrentPosition = 1 (pressed) — spec §1.13.5.2
  ::esp_matter_attr_val_t v_pressed = ::esp_matter_uint8(kPressed);
  esp_err_t err = ::esp_matter::attribute::update(
      ep, chip::app::Clusters::Switch::Id, chip::app::Clusters::Switch::Attributes::CurrentPosition::Id, &v_pressed);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "attribute::update CurrentPosition=1 endpoint=%u failed: %s", ep, esp_err_to_name(err));
  }

  // 2) InitialPress(newPosition=1) event
  err = ::esp_matter::cluster::switch_cluster::event::send_initial_press(ep, kPressed);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "send_initial_press endpoint=%u failed: %s", ep, esp_err_to_name(err));
  }

  // 3) CurrentPosition = 0 (released)
  ::esp_matter_attr_val_t v_released = ::esp_matter_uint8(kReleased);
  err = ::esp_matter::attribute::update(ep, chip::app::Clusters::Switch::Id,
                                        chip::app::Clusters::Switch::Attributes::CurrentPosition::Id, &v_released);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "attribute::update CurrentPosition=0 endpoint=%u failed: %s", ep, esp_err_to_name(err));
  }

  // 4) ShortRelease(previousPosition=1) event — the emitted `newPosition`
  // parameter on send_short_release is actually the *previous* position (the
  // one held while pressed) per Matter 1.4 spec §1.13.6.4. The helper's
  // parameter name is `previous_position` in esp_matter_event.h.
  err = ::esp_matter::cluster::switch_cluster::event::send_short_release(ep, kPressed);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "send_short_release endpoint=%u failed: %s", ep, esp_err_to_name(err));
  }
}

}  // namespace matter
}  // namespace esphome

#endif  // USE_BUTTON
#endif  // USE_ESP_IDF
