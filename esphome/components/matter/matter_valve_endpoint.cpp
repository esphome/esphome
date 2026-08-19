#include "esphome/core/defines.h"

#ifdef USE_ESP_IDF
#ifdef USE_VALVE

#include "matter_valve_endpoint.h"
#include "matter_component.h"

#include "esphome/core/log.h"
#include "esphome/components/valve/valve.h"

#include <esp_matter.h>

#include <app-common/zap-generated/cluster-objects.h>

namespace esphome {
namespace matter {

static const char *const TAG = "matter.valve";

MatterValveEndpoint::MatterValveEndpoint(::esphome::valve::Valve *valve) : valve_(valve) {}

uint8_t MatterValveEndpoint::esphome_to_matter_state_() const {
  if (this->valve_->current_operation != ::esphome::valve::VALVE_OPERATION_IDLE) {
    return 2;  // kTransitioning
  }
  if (this->valve_->position <= 0.01f) {
    return 0;  // kClosed
  }
  return 1;  // kOpen
}

bool MatterValveEndpoint::setup() {
  ::esp_matter::node_t *node = ::esp_matter::node::get();
  if (node == nullptr) {
    ESP_LOGE(TAG, "no Matter node available for valve '%s'", this->valve_->get_name().c_str());
    return false;
  }

  ::esp_matter::endpoint::water_valve::config_t config;
  config.valve_configuration_and_control.current_state = this->esphome_to_matter_state_();
  config.valve_configuration_and_control.target_state = nullptr;  // no active transition at boot
  // Store an explicit Delegate* — the field is void* and esp-matter's
  // ValveConfigurationAndControlDelegateInitCB does static_cast<Delegate*>. If
  // MatterValveEndpoint ever grows multiple bases and Delegate is not first
  // in the layout, storing a raw MatterValveEndpoint* here would give the
  // wrong offset. Casting explicitly to Delegate* now is future-proof.
  config.valve_configuration_and_control.delegate =
      static_cast<::chip::app::Clusters::ValveConfigurationAndControl::Delegate *>(this);

  ::esp_matter::endpoint_t *endpoint =
      ::esp_matter::endpoint::water_valve::create(node, &config, ::esp_matter::ENDPOINT_FLAG_NONE, this);
  if (endpoint == nullptr) {
    ESP_LOGE(TAG, "failed to create water_valve endpoint for '%s'", this->valve_->get_name().c_str());
    return false;
  }
  this->endpoint_id_ = ::esp_matter::endpoint::get_id(endpoint);

  MatterComponent::instance()->register_endpoint_label(endpoint, this->endpoint_id_, this->valve_->get_name());

  // ESPHome valve callback takes no args — re-read state on each fire.
  this->valve_->add_on_state_callback([this]() {
    ESP_LOGD(TAG, "device state change → fabric: endpoint=%u position=%.2f op=%u valve='%s'", this->endpoint_id_,
             this->valve_->position, static_cast<unsigned>(this->valve_->current_operation),
             this->valve_->get_name().c_str());
    this->report_state_to_fabric_();
  });

  ESP_LOGI(TAG, "registered valve '%s' as Matter water_valve endpoint %u", this->valve_->get_name().c_str(),
           this->endpoint_id_);
  return true;
}

::chip::app::DataModel::Nullable<::chip::Percent> MatterValveEndpoint::HandleOpenValve(
    ::chip::app::DataModel::Nullable<::chip::Percent> level) {
  (void) level;  // no Level (kLevel) feature advertised — level is null
  ESP_LOGD(TAG, "matter Open command endpoint=%u valve='%s'", this->endpoint_id_, this->valve_->get_name().c_str());
  // Delegate invoked on the CHIP task — defer the Valve::make_call so the
  // ESPHome-side call runs on the main loop. The return value is CHIP's
  // Nullable<Percent> ack, which we always report null (no percent feature).
  MatterComponent::instance()->defer_on_main_loop([this]() { this->valve_->make_call().set_command_open().perform(); });
  // Return null — signals to CHIP that we don't report a percent-level.
  // For binary valves the caller ignores the value except to check null-ness.
  return ::chip::app::DataModel::Nullable<::chip::Percent>();
}

CHIP_ERROR MatterValveEndpoint::HandleCloseValve() {
  ESP_LOGD(TAG, "matter Close command endpoint=%u valve='%s'", this->endpoint_id_, this->valve_->get_name().c_str());
  MatterComponent::instance()->defer_on_main_loop(
      [this]() { this->valve_->make_call().set_command_close().perform(); });
  return CHIP_NO_ERROR;
}

void MatterValveEndpoint::HandleRemainingDurationTick(uint32_t /*duration*/) {
  // No-op — we don't advertise the TimeSync feature and don't track OpenDuration.
}

void MatterValveEndpoint::push_initial_state() { this->report_state_to_fabric_(); }

void MatterValveEndpoint::report_state_to_fabric_() {
  uint8_t state = this->esphome_to_matter_state_();
  this->applying_report_ = true;

  ::esp_matter_attr_val_t v_current = ::esp_matter_nullable_enum8(::nullable<uint8_t>(state));
  esp_err_t err = ::esp_matter::attribute::update(
      this->endpoint_id_, chip::app::Clusters::ValveConfigurationAndControl::Id,
      chip::app::Clusters::ValveConfigurationAndControl::Attributes::CurrentState::Id, &v_current);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "attribute::update CurrentState endpoint=%u failed: %s", this->endpoint_id_, esp_err_to_name(err));
  }

  // When the valve settles at a terminal state (kOpen or kClosed), null out
  // TargetState so the fabric UI stops indicating a pending transition. CHIP's
  // UpdateCurrentState helper does this internally but requires the stack
  // lock; pushing null via esp_matter::attribute::update is equivalent for
  // our purposes and safer to call from the ESPHome loop context.
  if (state != 2) {
    ::esp_matter_attr_val_t v_target = ::esp_matter_nullable_enum8(::nullable<uint8_t>());
    esp_err_t err2 = ::esp_matter::attribute::update(
        this->endpoint_id_, chip::app::Clusters::ValveConfigurationAndControl::Id,
        chip::app::Clusters::ValveConfigurationAndControl::Attributes::TargetState::Id, &v_target);
    if (err2 != ESP_OK) {
      ESP_LOGW(TAG, "attribute::update TargetState endpoint=%u failed: %s", this->endpoint_id_, esp_err_to_name(err2));
    }
  }

  this->applying_report_ = false;
}

}  // namespace matter
}  // namespace esphome

#endif  // USE_VALVE
#endif  // USE_ESP_IDF
