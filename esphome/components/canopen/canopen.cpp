#include "canopen.h"

#include <array>

#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include "esphome/core/progmem.h"

namespace esphome::canopen {

ESPHOME_LOG_TAG(TAG, "canopen");

// indexed by the state's position in NMT_STATES; the last entry for other values
PROGMEM_STRING_TABLE(NMTStateStrings, "bootup", "stopped", "operational", "pre-operational", "unknown");
static_assert(NMT_STATES.size() == NMTStateStrings::LAST_INDEX);

const LogString *nmt_state_to_string(NMTState state) {
  for (size_t i = 0; i < NMT_STATES.size(); i++) {
    if (NMT_STATES[i] == state)
      return NMTStateStrings::get_log_str(i);
  }
  return NMTStateStrings::get_log_str(NMTStateStrings::LAST_INDEX);
}

// No CO_CANopenInit(): it needs a node id and starts NMT, heartbeat, EMCY and the SDO server.
void CANopenComponent::setup() {
  if (this->canbus_->is_failed()) {
    this->fail_setup_(LOG_STR("CAN bus failed"));
    return;
  }
  this->add_sdo_client_od_();
  this->config_.CNT_SDO_CLI = 1;
  this->config_.ENTRY_H1280 = &this->sdo_cli_entry_;
  uint32_t heap = 0;
  this->co_ = CO_new(&this->config_, &heap);
  if (this->co_ == nullptr) {
    this->fail_setup_(LOG_STR("CO_new failed"));
    return;
  }
  CO_ReturnError_t err = CO_CANinit(this->co_, this, 0);
  if (err != CO_ERROR_NO) {
    ESP_LOGE(TAG, "CO_CANinit failed: %d", err);
    this->fail_setup_(LOG_STR("CO_CANinit failed"));
    return;
  }
  if (!this->init_sdo_client_()) {
    this->fail_setup_(LOG_STR("SDO client init failed"));
    return;
  }
  for (const FrameListener *l = this->frame_listeners_; l != nullptr; l = l->next)
    this->check_frame_listener_(l->can_id, l->mask);
  CO_CANsetNormalMode(this->co_->CANmodule);
  this->canbus_->add_frame_callback([this](const canbus::CanFrame &frame) { this->on_frame_(frame); });
  ESP_LOGV(TAG, "CANopenNode initialized, SDO client, heap %" PRIu32 " B", heap);
}

void CANopenComponent::fail_setup_(const LogString *reason) {
  this->mark_failed(reason);
  this->reject_queued_sdo_();
}

void CANopenComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "CANopen (CANopenNode):\n  SDO client: response timeout %u ms", this->response_timeout_ms_);
}

bool CANopenComponent::idle_() const {
  return this->co_->CANmodule->CANtxCount == 0 && !this->sdo_busy_ && this->sdo_queue_.empty() &&
         this->sdo_abort_guard_us_ == 0;
}

// queue_sdo_(), cancel_sdo() and CO_CANsend() enable the loop again.
void CANopenComponent::loop() {
  if (this->idle_()) {
    this->disable_loop();
    return;
  }
  // SDO client before the flush, so an unsent frame ends at its deadline instead of going out late. Time is charged
  // only here, after canbus delivered its frames: a reply may be queued behind them.
  CO_SDOclient_t *cli = this->co_->SDOclient;
  const bool unsent = cli->CANtxBuff->bufferFull;
  const uint32_t now = App.get_loop_component_start_time();
  const uint32_t elapsed = now - this->last_ms_;
  // After a capped canbus pass the reply may still wait in the controller: hold the time, but at most one response
  // timeout past the deadline so a saturated bus still times out.
  if (!this->sdo_busy_ || this->sdo_cancel_pending_ || unsent || !this->canbus_->rx_capped() ||
      cli->timeoutTimer / 1000 + elapsed >= 2u * this->response_timeout_ms_) {
    this->process_sdo_(elapsed * 1000);
    this->last_ms_ = now;
  }
  // not per received frame: on ESP32 a refused frame blocks for the enqueue timeout
  CO_CANmodule_process(this->co_->CANmodule);
  // restart the response timeout now that the queued frame went out
  if (unsent && !cli->CANtxBuff->bufferFull)
    cli->timeoutTimer = 0;
}

}  // namespace esphome::canopen
