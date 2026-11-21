// CANopenComponent: setup and processing; the other features are in their own files, see canopen.h.
#include "canopen.h"

#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome::canopen {

ESPHOME_LOG_TAG(TAG, "canopen");

// Only the CAN module and the SDO client, without CO_CANopenInit(): it needs a node id and starts NMT, heartbeat,
// EMCY and the SDO server. The OD holds only the client's 0x1280.
void CANopenComponent::setup() {
  if (this->canbus_->is_failed()) {
    this->fail_setup_(LOG_STR("CAN bus failed"));
    return;
  }
  this->add_sdo_client_od_();
  this->config_.CNT_SDO_CLI = 1;
  this->config_.ENTRY_H1280 = &this->od_entries_[0];
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
  return this->co_->CANmodule->CANtxCount == 0 && !this->sdo_busy_ && this->sdo_queue_.empty();
}

// Only a running request or an unsent frame needs time: queue_sdo_() and CO_CANsend() enable the loop again.
void CANopenComponent::loop() {
  if (this->idle_()) {
    this->disable_loop();
    return;
  }
  // SDO client first: an unsent frame ends at its deadline, before the flush could send it. Time runs only here,
  // after the canbus loop delivered its frames: a received frame may come before a queued reply.
  CO_SDOclient_t *cli = this->co_->SDOclient;
  const bool unsent = cli->CANtxBuff->bufferFull;
  const uint32_t now = App.get_loop_component_start_time();
  const uint32_t elapsed = now - this->last_ms_;
  // The reply to a sent frame may still wait in the controller behind a capped canbus pass: the time is charged once it
  // drained, but held at most one response timeout past the deadline, so a saturated bus still times out.
  if (!this->sdo_busy_ || unsent || !this->canbus_->rx_capped() ||
      cli->timeoutTimer / 1000 + elapsed >= 2u * this->response_timeout_ms_) {
    this->process_sdo_(elapsed * 1000);
    this->last_ms_ = now;
  }
  // only from here, not per received frame: on ESP32 a refused frame can block for the controller's enqueue timeout
  CO_CANmodule_process(this->co_->CANmodule);
  // CANopenNode started the response timeout when it queued the frame: start it again now that the frame is sent
  if (unsent && !cli->CANtxBuff->bufferFull)
    cli->timeoutTimer = 0;
}

}  // namespace esphome::canopen
