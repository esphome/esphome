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

#ifdef USE_CANOPEN_NODE
// NMT reset node: once CANopenNode's queue is empty, the CAN controller's time to send what it holds (the canbus API
// only says a frame was accepted): 4 frames at 10 kbit/s, the lowest CiA 301 rate
static constexpr uint32_t RESET_TX_GRACE_MS = 60;
// and the extra time it waits at most for the queue to empty (e.g. our own broadcast reset_node)
static constexpr uint32_t RESET_FLUSH_EXTRA_MS = 80;
#endif

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

// to_nmt_state() casts: NMTState uses CANopenNode's values; CO_NMT_UNKNOWN (-1) becomes NMT_STATE_UNKNOWN (255)
static_assert(static_cast<uint8_t>(CO_NMT_INITIALIZING) == static_cast<uint8_t>(NMTState::NMT_STATE_BOOTUP));
static_assert(static_cast<uint8_t>(CO_NMT_STOPPED) == static_cast<uint8_t>(NMTState::NMT_STATE_STOPPED));
static_assert(static_cast<uint8_t>(CO_NMT_OPERATIONAL) == static_cast<uint8_t>(NMTState::NMT_STATE_OPERATIONAL));
static_assert(static_cast<uint8_t>(CO_NMT_PRE_OPERATIONAL) ==
              static_cast<uint8_t>(NMTState::NMT_STATE_PRE_OPERATIONAL));
static_assert(static_cast<uint8_t>(CO_NMT_UNKNOWN) == static_cast<uint8_t>(NMTState::NMT_STATE_UNKNOWN));

void CANopenComponent::setup() {
  if (this->canbus_->is_failed()) {
    this->fail_setup_(LOG_STR("CAN bus failed"));
    return;
  }
  // USE_CANOPEN_NODE: some hub has a node id. Without it nothing references CANopenNode's node part, so it isn't
  // linked. A hub without node id gets only the CAN module and the SDO client, whose 0x1280 is its whole OD.
  auto &od = this->od_;
  auto &cfg = this->config_;
#ifdef USE_CANOPEN_NODE
  const bool node = !this->is_client_only();
  if (node) {
    od.add_var(0x1001, ODA_SDO_R, &this->err_reg_, sizeof(this->err_reg_));
    // read only: without CO_CONFIG_EM_PROD_CONFIGURABLE a write wouldn't reach the EMCY producer
    od.add_var(0x1014, ODA_SDO_R, &this->cob_emcy_, sizeof(this->cob_emcy_));
    od.add_var(0x1017, ODA_SDO_RW, &this->hb_producer_ms_, sizeof(this->hb_producer_ms_));
    this->add_device_info_od_();
#ifdef USE_CANOPEN_NODE_MONITOR
    this->add_node_monitor_od_();
#endif
  }
#endif
#ifdef USE_CANOPEN_SDO_CLIENT
  this->add_sdo_client_od_();
#endif
  if (!od.finalize()) {
    this->fail_setup_(LOG_STR("invalid object dictionary"));
    return;
  }
#ifdef USE_CANOPEN_NODE
  if (node) {
    cfg.CNT_NMT = 1;
    cfg.ENTRY_H1017 = od.find(0x1017);
    cfg.CNT_EM = 1;
    cfg.ENTRY_H1001 = od.find(0x1001);
    cfg.ENTRY_H1014 = od.find(0x1014);
    cfg.CNT_ARR_1003 = 8;  // EM fifo size, needs >= 1
    this->config_device_info_(cfg);
#ifdef USE_CANOPEN_NODE_MONITOR
    this->config_node_monitor_(cfg);
#endif
  }
#endif
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
#ifdef USE_CANOPEN_NODE
  if (node) {
    uint32_t err_info = 0;
    uint16_t nmt_control = this->start_operational_ ? CO_NMT_STARTUP_TO_OPERATIONAL : 0;
    err = CO_CANopenInit(this->co_, nullptr, nullptr, od.od(), nullptr, nmt_control, 500 /* first HB, ms */, 1000, 0,
                         false, this->node_id_, &err_info);
    if (err != CO_ERROR_NO) {
      ESP_LOGE(TAG, "CO_CANopenInit failed: %d (OD 0x%04" PRIx32 ")", err, err_info);
      this->fail_setup_(LOG_STR("CO_CANopenInit failed"));
      return;
    }
#ifdef USE_CANOPEN_NODE_MONITOR
    this->init_node_monitor_();
#endif
    this->last_us_ = micros();
  }
#endif
#ifdef USE_CANOPEN_SDO_CLIENT
  // CO_CANopenInit() sets up the client of a node; without it the client is set up on its own
  if (this->is_client_only() && !this->init_sdo_client_()) {
    this->fail_setup_(LOG_STR("SDO client init failed"));
    return;
  }
#endif
  for (const FrameListener *l = this->frame_listeners_; l != nullptr; l = l->next)
    this->check_frame_listener_(l->can_id, l->mask);

#ifdef USE_CANOPEN_LOCAL_NODES
  if (this->next_local_ != nullptr && this->next_local_ != this)
    this->local_queue_.init(LOCAL_QUEUE_SIZE);
#endif
  CO_CANsetNormalMode(this->co_->CANmodule);
  this->canbus_->add_frame_callback([this](const canbus::CanFrame &frame) { this->on_frame_(frame); });
  ESP_LOGV(TAG, "CANopenNode initialized, node 0x%02x, heap %" PRIu32 " B", this->node_id_, heap);
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
#ifdef USE_CANOPEN_NODE_MONITOR
  this->dump_node_monitor_config_();
#endif
#endif
}

bool CANopenComponent::idle_() const {
#ifdef USE_CANOPEN_SDO_CLIENT
#ifdef USE_CANOPEN_LOCAL_NODES
  if (!this->local_queue_.empty())
    return false;
#endif
  return this->is_client_only() && this->co_->CANmodule->CANtxCount == 0 && this->sdo_idle_();
#else
  return false;
#endif
}

// A client-only hub needs time only for a running request or an unsent frame: queue_sdo_(), cancel_sdo() and
// CO_CANsend() enable the loop again.
void CANopenComponent::loop() {
  if (this->idle_()) {
    this->disable_loop();
    return;
  }
#ifdef USE_CANOPEN_LOCAL_NODES
  // frames other local nodes sent since the canbus loop delivered the bus's (on_frame_() delivers the older ones first)
  this->deliver_local_();
#endif
#ifdef USE_CANOPEN_SDO_CLIENT
  // SDO client first: an unsent frame ends at its deadline, before the flush could send it. Time runs only here,
  // after the canbus loop delivered its frames: a received frame may come before a queued reply.
  CO_SDOclient_t *cli = this->co_->SDOclient;
  const bool unsent = cli->CANtxBuff->bufferFull;
  const uint32_t now = App.get_loop_component_start_time();
  const uint32_t elapsed = now - this->sdo_last_ms_;
  // The reply to a sent frame may still wait in the controller behind a capped canbus pass: the time is charged once it
  // drained, but held at most one response timeout past the deadline, so a saturated bus still times out.
  if (!this->sdo_busy_ || this->sdo_cancel_pending_ || unsent || !this->canbus_->rx_capped() ||
      cli->timeoutTimer / 1000 + elapsed >= 2u * this->response_timeout_ms_) {
    this->process_sdo_(elapsed * 1000);
    this->sdo_last_ms_ = now;
  }
#endif
  // The only flush of queued frames, also for a node: CO_process() doesn't flush (hold_flush_).
  CO_CANmodule_process(this->co_->CANmodule);
#ifdef USE_CANOPEN_SDO_CLIENT
  // CANopenNode started the response timeout when it queued the frame: start it again now that the frame is sent
  if (unsent && !cli->CANtxBuff->bufferFull)
    cli->timeoutTimer = 0;
#endif
  if (!this->is_client_only()) {
    this->hold_flush_ = true;
    this->process_node_();
    this->hold_flush_ = false;
  }
}

void CANopenComponent::process_node_() {
#ifdef USE_CANOPEN_NODE
  this->processing_ = true;
  uint32_t now = micros();
  uint32_t dt = now - this->last_us_;
  this->last_us_ = now;
  CO_NMT_reset_cmd_t reset = CO_process(this->co_, false, dt, nullptr);
#ifdef USE_CANOPEN_NODE_MONITOR
  this->process_node_monitor_(dt);
  this->dispatch_node_events_();
#endif
  const uint32_t now_ms = App.get_loop_component_start_time();
  if (reset == CO_RESET_APP && !this->reset_pending_) {
    ESP_LOGW(TAG, "NMT reset node");
    this->reset_pending_ = true;
    this->reset_deadline_ms_ = now_ms + RESET_TX_GRACE_MS + RESET_FLUSH_EXTRA_MS;
  } else if (reset == CO_RESET_COMM) {
    ESP_LOGW(TAG, "NMT reset communication (not implemented)");
  }
  // The reboot waits for the queued frames (loop() flushes them before it processes the node), then for the controller
  // to send them: a reset_node to all nodes from us would otherwise reboot us before its frame is on the bus.
  if (this->reset_pending_) {
    if (this->co_->CANmodule->CANtxCount == 0 &&
        static_cast<int32_t>(now_ms + RESET_TX_GRACE_MS - this->reset_deadline_ms_) < 0)
      this->reset_deadline_ms_ = now_ms + RESET_TX_GRACE_MS;
    if (static_cast<int32_t>(now_ms - this->reset_deadline_ms_) >= 0)
      App.safe_reboot();
  }
  // CANopenNode's state change callback has no context pointer, so the state is compared here
  NMTState state = to_nmt_state(CO_NMT_getInternalState(this->co_->NMT));
  if (state != this->nmt_state_) {
    this->nmt_state_ = state;
    ESP_LOGI(TAG, "NMT state: %s", LOG_STR_ARG(nmt_state_to_string(state)));
    this->nmt_state_callback_.call(state);
  }
  this->processing_ = false;
#endif  // USE_CANOPEN_NODE
}

#ifdef USE_CANOPEN_NODE
bool CANopenComponent::nmt_command_pending_() const { return this->co_->NMT->internalCommand != CO_NMT_NO_COMMAND; }

void CANopenComponent::apply_nmt_command_() {
  if (this->processing_)
    return;  // an automation the processing runs: applied by the next one
  // as everywhere the node is processed, without flushing (also when called while a received frame is handled)
  const bool held = this->hold_flush_;
  this->hold_flush_ = true;
  this->process_node_();
  this->hold_flush_ = held;
}
#endif

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
  if (this->nmt_command_pending_()) {
    ESP_LOGW(TAG, "NMT state %s rejected: the previous command is still pending",
             LOG_STR_ARG(nmt_state_to_string(state)));
    return false;
  }
  CO_NMT_sendInternalCommand(this->co_->NMT, command);
  this->apply_nmt_command_();
  return true;
#else
  return false;
#endif
}

}  // namespace esphome::canopen
