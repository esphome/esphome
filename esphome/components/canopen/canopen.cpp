// CANopenComponent: setup (initialising CANopenNode's CAN module and SDO client) and processing. The other features
// are in their own files, see canopen.h.
#include "canopen.h"

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::canopen {

ESPHOME_LOG_TAG(TAG, "canopen");

// Only the CAN module and the SDO client, without CO_CANopenInit(): it needs a node id and starts NMT, heartbeat,
// EMCY and the SDO server. The OD holds only the client's 0x1280.
void CANopenComponent::setup() {
  this->add_sdo_client_od_();
  this->config_sdo_client_(this->config_);
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
  this->last_us_ = micros();
  ESP_LOGV(TAG, "CANopenNode initialized, SDO client, heap %" PRIu32 " B", heap);
}

void CANopenComponent::fail_setup_(const LogString *reason) {
  this->mark_failed(reason);
  this->reject_queued_sdo_();
}

void CANopenComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "CANopen (CANopenNode):\n  SDO client: response timeout %u ms", this->response_timeout_ms_);
}

void CANopenComponent::loop() { this->process_(); }

void CANopenComponent::process_() {
  if (this->co_ == nullptr || this->is_failed())
    return;
  // nothing to do without a request or a frame to send
  if (this->co_->CANmodule->CANtxCount == 0 && !this->sdo_busy_ && this->sdo_count_ == 0)
    return;
  uint32_t now = micros();
  uint32_t dt = now - this->last_us_;
  this->last_us_ = now;
  // The SDO client first: a request whose frame still waits at its deadline ends there, before the flush could send
  // it. CO_process() would process the node's objects, which aren't initialised.
  this->process_sdo_(dt);
  CO_CANmodule_process(this->co_->CANmodule);
}

}  // namespace esphome::canopen
