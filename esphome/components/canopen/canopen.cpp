// CANopenComponent: setup (building the OD and initialising CANopenNode), processing, our own NMT state. The other
// features are in their own files, see canopen.h.
#include "canopen.h"

#include <array>

#include "esphome/core/application.h"
#include "esphome/core/hal.h"
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
      return NMTStateStrings::get_log_str(static_cast<uint8_t>(i), NMTStateStrings::LAST_INDEX);
  }
  return NMTStateStrings::get_log_str(NMTStateStrings::LAST_INDEX, NMTStateStrings::LAST_INDEX);
}

static NMTState to_nmt_state(CO_NMT_internalState_t state) {
  switch (state) {
    case CO_NMT_INITIALIZING:
      return NMTState::NMT_STATE_BOOTUP;
    case CO_NMT_STOPPED:
      return NMTState::NMT_STATE_STOPPED;
    case CO_NMT_OPERATIONAL:
      return NMTState::NMT_STATE_OPERATIONAL;
    case CO_NMT_PRE_OPERATIONAL:
      return NMTState::NMT_STATE_PRE_OPERATIONAL;
    default:
      return NMTState::NMT_STATE_UNKNOWN;
  }
}

void CANopenComponent::setup() {
  if (this->canbus_->is_failed()) {
    this->fail_setup_(LOG_STR("CAN bus failed"));
    return;
  }
  if (this->is_client_only()) {
    this->setup_client_only_();
    return;
  }
  // USE_CANOPEN_NODE: some hub has a node id. Without it nothing references CANopenNode's node part, so it isn't
  // linked.
#ifdef USE_CANOPEN_NODE
  auto &od = this->od_;
  od.add_var(0x1001, ODA_SDO_R, &this->err_reg_, sizeof(this->err_reg_));
  od.add_var(0x1014, ODA_SDO_RW, &this->cob_emcy_, sizeof(this->cob_emcy_));
  od.add_var(0x1017, ODA_SDO_RW, &this->hb_producer_ms_, sizeof(this->hb_producer_ms_));
  this->add_device_info_od_();
#ifdef USE_CANOPEN_SDO_CLIENT
  this->add_sdo_client_od_();
#endif
  if (!od.finalize()) {
    this->fail_setup_(LOG_STR("invalid object dictionary"));
    return;
  }

  auto &cfg = this->config_;
  cfg.CNT_NMT = 1;
  cfg.ENTRY_H1017 = od.find(0x1017);
  cfg.CNT_EM = 1;
  cfg.ENTRY_H1001 = od.find(0x1001);
  cfg.ENTRY_H1014 = od.find(0x1014);
  cfg.CNT_ARR_1003 = 8;  // EM fifo size, needs >= 1
  this->config_device_info_(cfg);
#ifdef USE_CANOPEN_SDO_CLIENT
  this->config_sdo_client_(cfg);
#endif

  uint32_t heap = 0;
  this->co_ = CO_new(&cfg, &heap);
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
  uint32_t err_info = 0;
  uint16_t nmt_control = this->start_operational_ ? CO_NMT_STARTUP_TO_OPERATIONAL : 0;
  err = CO_CANopenInit(this->co_, nullptr, nullptr, od.od(), nullptr, nmt_control, 500 /* first HB, ms */, 1000, 0,
                       false, this->node_id_, &err_info);
  if (err != CO_ERROR_NO) {
    ESP_LOGE(TAG, "CO_CANopenInit failed: %d (OD 0x%04" PRIx32 ")", err, err_info);
    this->fail_setup_(LOG_STR("CO_CANopenInit failed"));
    return;
  }
  for (const FrameListener *l = this->frame_listeners_; l != nullptr; l = l->next)
    this->check_frame_listener_(l->can_id, l->mask);

  CO_CANsetNormalMode(this->co_->CANmodule);
  this->canbus_->add_frame_callback([this](const canbus::CanFrame &frame) { this->on_frame_(frame); });
  this->last_us_ = micros();
  ESP_LOGV(TAG, "CANopenNode initialized, node 0x%02x, heap %" PRIu32 " B", this->node_id_, heap);
#endif  // USE_CANOPEN_NODE
}

// Client-only mode: only the CAN module and the SDO client, without CO_CANopenInit() (it needs a node id and starts
// NMT, heartbeat, EMCY and the SDO server). The OD holds only the client's 0x1280.
void CANopenComponent::setup_client_only_() {
#ifdef USE_CANOPEN_SDO_CLIENT
  this->add_sdo_client_od_();
  if (!this->od_.finalize()) {
    this->fail_setup_(LOG_STR("invalid object dictionary"));
    return;
  }
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
  for (const FrameListener *l = this->frame_listeners_; l != nullptr; l = l->next)
    this->check_frame_listener_(l->can_id, l->mask);
  CO_CANsetNormalMode(this->co_->CANmodule);
  this->canbus_->add_frame_callback([this](const canbus::CanFrame &frame) { this->on_frame_(frame); });
  ESP_LOGV(TAG, "CANopenNode initialized, SDO client only, heap %" PRIu32 " B", heap);
#else
  // codegen enables the SDO client for a hub without node id
  this->fail_setup_(LOG_STR("no node id and no SDO client"));
#endif
}

void CANopenComponent::fail_setup_(const LogString *reason) {
  this->mark_failed(reason);
#ifdef USE_CANOPEN_SDO_CLIENT
  this->reject_queued_sdo_();
#endif
}

void CANopenComponent::dump_config() {
  if (this->is_client_only()) {
#ifdef USE_CANOPEN_SDO_CLIENT
    ESP_LOGCONFIG(TAG,
                  "CANopen (CANopenNode):\n"
                  "  Mode: SDO client only (no node ID)\n"
                  "  SDO client: response timeout %u ms",
                  this->response_timeout_ms_);
#endif
    return;
  }
#ifdef USE_CANOPEN_NODE
  ESP_LOGCONFIG(TAG,
                "CANopen (CANopenNode):\n"
                "  Node ID: 0x%02x\n"
                "  Heartbeat interval: %u ms\n"
                "  NMT startup: %s",
                this->node_id_, this->hb_producer_ms_,
                this->start_operational_ ? LOG_STR_LITERAL("operational") : LOG_STR_LITERAL("pre-operational"));
  this->dump_device_info_config_();
#ifdef USE_CANOPEN_SDO_CLIENT
  ESP_LOGCONFIG(TAG, "  SDO client: response timeout %u ms", this->response_timeout_ms_);
#endif
#endif
}

// canbus::Canbus::rx_capped() came with the SDO client (PR 1); an older canbus never reports a capped pass
template<typename T> static bool rx_capped(const T *canbus) {
  if constexpr (requires { canbus->rx_capped(); }) {
    return canbus->rx_capped();
  } else {
    return false;
  }
}

bool CANopenComponent::idle_() const {
#ifdef USE_CANOPEN_SDO_CLIENT
  return this->is_client_only() && this->co_->CANmodule->CANtxCount == 0 && this->sdo_idle_();
#else
  return false;
#endif
}

// A client-only hub needs time only for a running request or an unsent frame: queue_sdo_(), cancel_sdo() and
// CO_CANsend() enable the loop again.
void CANopenComponent::loop() {
  if (this->co_ == nullptr)
    return;
  if (this->idle_()) {
    this->disable_loop();
    return;
  }
#ifdef USE_CANOPEN_SDO_CLIENT
  // SDO client first: an unsent frame ends at its deadline, before the flush could send it. Time runs only here,
  // after the canbus loop delivered its frames: a received frame may come before a queued reply.
  CO_SDOclient_t *cli = this->co_->SDOclient;
  const bool unsent = cli->CANtxBuff->bufferFull;
  const uint32_t now = App.get_loop_component_start_time();
  const uint32_t elapsed = now - this->sdo_last_ms_;
  // The reply to a sent frame may still wait in the controller behind a capped canbus pass: the time is charged once it
  // drained, but held at most one response timeout past the deadline, so a saturated bus still times out.
  if (!this->sdo_busy_ || this->sdo_cancel_pending_ || unsent || !rx_capped(this->canbus_) ||
      cli->timeoutTimer / 1000 + elapsed >= 2u * this->response_timeout_ms_) {
    this->process_sdo_(elapsed * 1000);
    this->sdo_last_ms_ = now;
  }
#endif
  // The only flush of queued frames, also for a node: CO_process() flushes too, but not while a received frame is
  // processed (in_frame_).
  CO_CANmodule_process(this->co_->CANmodule);
#ifdef USE_CANOPEN_SDO_CLIENT
  // CANopenNode started the response timeout when it queued the frame: start it again now that the frame is sent
  if (unsent && !cli->CANtxBuff->bufferFull)
    cli->timeoutTimer = 0;
#endif
  if (!this->is_client_only())
    this->process_node_();
}

void CANopenComponent::process_node_() {
#ifdef USE_CANOPEN_NODE
  uint32_t now = micros();
  uint32_t dt = now - this->last_us_;
  this->last_us_ = now;
  CO_NMT_reset_cmd_t reset = CO_process(this->co_, false, dt, nullptr);
  if (reset == CO_RESET_APP) {
    ESP_LOGW(TAG, "NMT reset node");
    App.safe_reboot();
  } else if (reset == CO_RESET_COMM) {
    ESP_LOGW(TAG, "NMT reset communication (not implemented)");
  }
  // CANopenNode's state change callback has no context pointer, so the state is compared here
  NMTState state = to_nmt_state(CO_NMT_getInternalState(this->co_->NMT));
  if (state != this->nmt_state_) {
    this->nmt_state_ = state;
    ESP_LOGI(TAG, "NMT state: %s", LOG_STR_ARG(nmt_state_to_string(state)));
    this->nmt_state_callback_.call(state);
  }
#endif  // USE_CANOPEN_NODE
}

bool CANopenComponent::set_nmt_state(NMTState state) {
#ifdef USE_CANOPEN_NODE
  if (this->co_ == nullptr || this->is_failed() || this->is_client_only())
    return false;
  CO_NMT_command_t command;
  switch (state) {
    case NMTState::NMT_STATE_OPERATIONAL:
      command = CO_NMT_ENTER_OPERATIONAL;
      break;
    case NMTState::NMT_STATE_PRE_OPERATIONAL:
      command = CO_NMT_ENTER_PRE_OPERATIONAL;
      break;
    case NMTState::NMT_STATE_STOPPED:
      command = CO_NMT_ENTER_STOPPED;
      break;
    default:
      return false;
  }
  // applied by the next CO_process()
  CO_NMT_sendInternalCommand(this->co_->NMT, command);
  return true;
#else
  return false;
#endif
}

}  // namespace esphome::canopen
