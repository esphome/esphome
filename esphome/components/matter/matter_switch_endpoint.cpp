// Pull in ESPHome's USE_* macros before the ifdef guards — none of the
// other includes below reach defines.h, and log.h itself sits inside the
// guard so relying on transitivity would be circular.
#include "esphome/core/defines.h"

#ifdef USE_ESP_IDF
#ifdef USE_SWITCH

#include "matter_switch_endpoint.h"
#include "matter_component.h"

#include "esphome/core/log.h"
#include "esphome/components/switch/switch.h"

#include <esp_matter.h>

#include <app-common/zap-generated/cluster-objects.h>

namespace esphome::matter {

static const char *const TAG = "matter.switch";

MatterSwitchEndpoint::MatterSwitchEndpoint(switch_::Switch *sw) : switch_(sw) {}

bool MatterSwitchEndpoint::setup() {
  ::esp_matter::node_t *node = ::esp_matter::node::get();
  if (node == nullptr) {
    ESP_LOGE(TAG, "no Matter node available when registering switch '%s'", this->switch_->get_name().c_str());
    return false;
  }

  ::esp_matter::endpoint::on_off_plug_in_unit::config_t config;
  // Reflect the switch's current state into the endpoint config so the
  // OnOff attribute initializes correctly (avoids a brief mismatch between
  // fabric view and device state before push_initial_state() runs).
  config.on_off.on_off = this->switch_->state;

  ::esp_matter::endpoint_t *endpoint =
      ::esp_matter::endpoint::on_off_plug_in_unit::create(node, &config, ::esp_matter::ENDPOINT_FLAG_NONE, this);
  if (endpoint == nullptr) {
    ESP_LOGE(TAG, "failed to create OnOff plug endpoint for switch '%s'", this->switch_->get_name().c_str());
    return false;
  }
  this->endpoint_id_ = ::esp_matter::endpoint::get_id(endpoint);

  // Expose the ESPHome switch's name to the fabric via a FixedLabel entry.
  // Without this the controller only sees the Device Type ("On/Off Plug in
  // Unit") and falls back to synthetic labels like "Channel N".
  MatterComponent::instance()->register_endpoint_label(endpoint, this->endpoint_id_, this->switch_->get_name());

  // Bridge device → fabric: any time the ESPHome switch flips, tell Matter.
  this->switch_->add_on_state_callback([this](bool state) {
    if (this->applying_matter_write_) {
      ESP_LOGV(TAG, "device state callback suppressed (matter-driven change) endpoint=%u", this->endpoint_id_);
      return;
    }
    ESP_LOGD(TAG, "device state change → fabric: endpoint=%u state=%d switch='%s'", this->endpoint_id_,
             static_cast<int>(state), this->switch_->get_name().c_str());
    this->report_state_to_fabric_(state);
  });

  ESP_LOGI(TAG, "registered switch '%s' as Matter endpoint %u", this->switch_->get_name().c_str(), this->endpoint_id_);
  return true;
}

void MatterSwitchEndpoint::on_matter_write(bool state) {
  ESP_LOGD(TAG, "matter write endpoint=%u state=%d switch='%s'", this->endpoint_id_, static_cast<int>(state),
           this->switch_->get_name().c_str());
  // on_matter_write runs on the CHIP PlatformManager task (esp-matter invokes
  // attribute_update_cb from there), NOT the ESPHome main loop. Touching
  // Switch::turn_on/off directly off-loop races the ESPHome state machine and
  // its subscribers. Route through defer_on_main_loop so the ESPHome-side
  // call always executes on the main thread; applying_matter_write_ is
  // scoped inside the deferred lambda so both writer and reader (the state
  // callback in setup()) touch it on the main loop only — no atomic needed.
  MatterComponent::instance()->defer_on_main_loop([this, state]() {
    this->applying_matter_write_ = true;
    if (state) {
      this->switch_->turn_on();
    } else {
      this->switch_->turn_off();
    }
    this->applying_matter_write_ = false;
  });
}

void MatterSwitchEndpoint::push_initial_state() { this->report_state_to_fabric_(this->switch_->state); }

void MatterSwitchEndpoint::report_state_to_fabric_(bool state) {
  ::esp_matter_attr_val_t val = ::esp_matter_bool(state);
  // Use update() rather than report() — update() actually pushes the value
  // through the data-model provider and fires attribute-changed notifications
  // to any subscribed fabric. report() only marks the attribute dirty, which
  // relies on the fabric having an active subscription that reads the flag.
  // update() also fires PRE_UPDATE synchronously, which lands in our own
  // dispatcher — the applying_report_ flag suppresses the round-trip.
  ApplyingReportGuard applying_report_guard(this->applying_report_);
  esp_err_t err = ::esp_matter::attribute::update(this->endpoint_id_, chip::app::Clusters::OnOff::Id,
                                                  chip::app::Clusters::OnOff::Attributes::OnOff::Id, &val);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "attribute::update endpoint=%u failed: %s", this->endpoint_id_, esp_err_to_name(err));
  }
}

}  // namespace esphome::matter

#endif  // USE_SWITCH
#endif  // USE_ESP_IDF
