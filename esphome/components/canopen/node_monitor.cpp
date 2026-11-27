// Monitoring of other nodes by their heartbeat (CANopenNode heartbeat consumer, 0x1016): node events and states. The
// consumer is built only with USE_CANOPEN_NODE_MONITOR (canopen.enable_node_monitor()).
#include "canopen.h"

#include "esphome/core/log.h"
#include "esphome/core/progmem.h"

namespace esphome::canopen {

ESPHOME_LOG_TAG(TAG, "canopen");

// indexed by NodeEvent; the last entry for other values
PROGMEM_STRING_TABLE(NodeEventStrings, "online", "offline", "NMT changed", "reset", "unknown");

const LogString *node_event_to_string(NodeEvent event) {
  return NodeEventStrings::get_log_str(static_cast<uint8_t>(event), NodeEventStrings::LAST_INDEX);
}

#ifdef USE_CANOPEN_NODE_MONITOR
void CANopenComponent::monitor_node(uint8_t node_id, uint16_t timeout_ms) {
  if (this->co_ != nullptr) {
    ESP_LOGE(TAG, "monitor_node(0x%02x) after setup, ignored", node_id);
    return;
  }
  if (this->is_client_only()) {
    ESP_LOGE(TAG, "monitor_node(0x%02x): needs a CANopen node (node_id), ignored", node_id);
    return;
  }
  // a timeout of 0 or node id 0 would leave the 0x1016 entry unused
  if (node_id == 0 || node_id > 127 || node_id == this->node_id_ || timeout_ms == 0) {
    ESP_LOGE(TAG, "monitor_node(0x%02x, %u ms): needs another node's id (1-127) and a timeout, ignored", node_id,
             (unsigned) timeout_ms);
    return;
  }
  // CANopenNode rejects a node id that is in 0x1016 twice
  for (uint32_t &c : this->hb_cons_) {
    if ((c >> 16) == node_id) {
      if (timeout_ms < (c & 0xFFFF))
        c = ((uint32_t) node_id << 16) | timeout_ms;
      return;
    }
  }
  this->hb_cons_.push_back(((uint32_t) node_id << 16) | timeout_ms);
}

NMTState CANopenComponent::get_node_state(uint8_t node_id) {
  if (this->co_ == nullptr || this->hb_cons_.empty())
    return NMTState::NMT_STATE_UNKNOWN;
  int8_t idx = CO_HBconsumer_getIdxByNodeId(this->co_->HBcons, node_id);
  CO_NMT_internalState_t state;
  if (idx < 0 || CO_HBconsumer_getNmtState(this->co_->HBcons, idx, &state) != 0)
    return NMTState::NMT_STATE_UNKNOWN;
  return to_nmt_state(state);
}

bool CANopenComponent::is_node_online(uint8_t node_id) {
  if (this->co_ == nullptr || this->hb_cons_.empty())
    return false;
  int8_t idx = CO_HBconsumer_getIdxByNodeId(this->co_->HBcons, node_id);
  return idx >= 0 && CO_HBconsumer_getState(this->co_->HBcons, idx) == CO_HBconsumer_ACTIVE;
}

void CANopenComponent::add_node_monitor_od_() {
  uint8_t n_cons = this->hb_cons_.size();
  if (n_cons) {
    this->hb_cons_count_ = n_cons;
    // read only: without CO_CONFIG_FLAG_OD_DYNAMIC a write wouldn't reach the consumer
    this->od_.add_array(0x1016, ODA_SDO_R, &this->hb_cons_count_, this->hb_cons_.data(), sizeof(uint32_t), n_cons);
  }
}

void CANopenComponent::config_node_monitor_(CO_config_t &cfg) {
  uint8_t n_cons = this->hb_cons_.size();
  cfg.CNT_HB_CONS = n_cons ? 1 : 0;
  cfg.CNT_ARR_1016 = n_cons;
  cfg.ENTRY_H1016 = this->od_.find(0x1016);
}

void CANopenComponent::init_node_monitor_() {
  CO_HBconsumer_t *hb = this->co_->HBcons;
  if (hb == nullptr)
    return;  // no monitored nodes
  // CO_process() runs the consumer only while this node is pre-operational or operational, and clears every entry
  // when it stops (no events; ONLINE again for each node after a start). CiA 301 keeps error control running in
  // stopped, so process_node_monitor_() runs it instead, as (pre-)operational after initialisation: CO_process() skips
  // it with the count 0 (read there through config_; allocation and init are done).
  this->config_.CNT_HB_CONS = 0;
  // The entries are as a first pass would leave them (state unknown, no frame), so that pass doesn't handle the end of
  // initialisation as a state change, which would drop a heartbeat received before it (from the bus, or a local
  // node's boot-up queued before our first loop()).
  hb->NMTisPreOrOperationalPrev = true;
  this->pending_node_events_.init(2 * this->hb_cons_.size());
  for (uint8_t i = 0, n = this->hb_cons_.size(); i < n; i++) {
    CO_HBconsumer_initCallbackHeartbeatStarted(hb, i, this, on_hb_started);
    CO_HBconsumer_initCallbackNmtChanged(hb, i, this, on_hb_nmt_changed);
    CO_HBconsumer_initCallbackTimeout(hb, i, this, on_hb_timeout);
    CO_HBconsumer_initCallbackRemoteReset(hb, i, this, on_hb_remote_reset);
  }
}

void CANopenComponent::process_node_monitor_(uint32_t dt_us) {
  if (this->co_->HBcons != nullptr)
    CO_HBconsumer_process(this->co_->HBcons, true, dt_us, nullptr);
}

void CANopenComponent::dump_node_monitor_config_() {
  for (uint32_t c : this->hb_cons_) {
    ESP_LOGCONFIG(TAG, "  Heartbeat client: node 0x%02x, timeout %u ms", (unsigned) (c >> 16), (unsigned) (c & 0xFFFF));
  }
}
#endif  // USE_CANOPEN_NODE_MONITOR

void CANopenComponent::node_event_(uint8_t node_id, NodeEvent event, NMTState state) {
  if (event == NodeEvent::NODE_EVENT_OFFLINE) {
    ESP_LOGW(TAG, "node 0x%02x offline (heartbeat timeout)", node_id);
  } else {
    ESP_LOGI(TAG, "node 0x%02x %s, NMT state: %s", node_id, LOG_STR_ARG(node_event_to_string(event)),
             LOG_STR_ARG(nmt_state_to_string(state)));
  }
  this->node_event_callback_.call(node_id, event, state);
}

#ifdef USE_CANOPEN_NODE_MONITOR
// CANopenNode heartbeat consumer callbacks (called from CO_process(), before the consumer stores the new state): they
// only record the event, dispatch_node_events_() runs the user callbacks after CO_process().
// push_back() ignores an event beyond the capacity, which the 2 per node and pass can't reach.
void CANopenComponent::on_hb_started(uint8_t node_id, uint8_t idx, void *object) {
  auto *self = static_cast<CANopenComponent *>(object);
  // the heartbeat's state is already stored (CO_HBconsumer_getNmtState() returns it once HBstate is ACTIVE)
  CO_NMT_internalState_t state = self->co_->HBcons->monitoredNodes[idx].NMTstate;
  self->pending_node_events_.push_back({node_id, NodeEvent::NODE_EVENT_ONLINE, to_nmt_state(state)});
}

void CANopenComponent::on_hb_nmt_changed(uint8_t node_id, uint8_t idx, CO_NMT_internalState_t state, void *object) {
  static_cast<CANopenComponent *>(object)->pending_node_events_.push_back(
      {node_id, NodeEvent::NODE_EVENT_NMT_CHANGED, to_nmt_state(state)});
}

void CANopenComponent::on_hb_timeout(uint8_t node_id, uint8_t idx, void *object) {
  static_cast<CANopenComponent *>(object)->pending_node_events_.push_back(
      {node_id, NodeEvent::NODE_EVENT_OFFLINE, NMTState::NMT_STATE_UNKNOWN});
}

void CANopenComponent::on_hb_remote_reset(uint8_t node_id, uint8_t idx, void *object) {
  static_cast<CANopenComponent *>(object)->pending_node_events_.push_back(
      {node_id, NodeEvent::NODE_EVENT_RESET, NMTState::NMT_STATE_BOOTUP});
}

void CANopenComponent::dispatch_node_events_() {
  for (const PendingNodeEvent &e : this->pending_node_events_)
    this->node_event_(e.node_id, e.event, e.state);
  this->pending_node_events_.clear();
}
#endif  // USE_CANOPEN_NODE_MONITOR

}  // namespace esphome::canopen
