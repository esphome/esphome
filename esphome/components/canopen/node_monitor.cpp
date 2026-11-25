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
    this->od_.add_array(0x1016, ODA_SDO_RW, &this->hb_cons_count_, this->hb_cons_.data(), sizeof(uint32_t), n_cons);
  }
}

void CANopenComponent::config_node_monitor_(CO_config_t &cfg) {
  uint8_t n_cons = this->hb_cons_.size();
  cfg.CNT_HB_CONS = n_cons ? 1 : 0;
  cfg.CNT_ARR_1016 = n_cons;
  cfg.ENTRY_H1016 = this->od_.find(0x1016);
}

void CANopenComponent::init_node_monitor_() {
  for (uint8_t i = 0, n = this->hb_cons_.size(); i < n; i++) {
    CO_HBconsumer_t *hb = this->co_->HBcons;
    CO_HBconsumer_initCallbackHeartbeatStarted(hb, i, this, on_hb_started);
    CO_HBconsumer_initCallbackNmtChanged(hb, i, this, on_hb_nmt_changed);
    CO_HBconsumer_initCallbackTimeout(hb, i, this, on_hb_timeout);
    CO_HBconsumer_initCallbackRemoteReset(hb, i, this, on_hb_remote_reset);
  }
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
// CANopenNode heartbeat consumer callbacks (called from CO_process())
void CANopenComponent::on_hb_started(uint8_t node_id, uint8_t idx, void *object) {
  auto *self = static_cast<CANopenComponent *>(object);
  // the heartbeat's state is already stored, but CO_HBconsumer_getNmtState() only returns it once HBstate is ACTIVE,
  // which CANopenNode sets after this callback
  CO_NMT_internalState_t state = self->co_->HBcons->monitoredNodes[idx].NMTstate;
  self->node_event_(node_id, NodeEvent::NODE_EVENT_ONLINE, to_nmt_state(state));
}

void CANopenComponent::on_hb_nmt_changed(uint8_t node_id, uint8_t idx, CO_NMT_internalState_t state, void *object) {
  static_cast<CANopenComponent *>(object)->node_event_(node_id, NodeEvent::NODE_EVENT_NMT_CHANGED, to_nmt_state(state));
}

void CANopenComponent::on_hb_timeout(uint8_t node_id, uint8_t idx, void *object) {
  static_cast<CANopenComponent *>(object)->node_event_(node_id, NodeEvent::NODE_EVENT_OFFLINE,
                                                       NMTState::NMT_STATE_UNKNOWN);
}

void CANopenComponent::on_hb_remote_reset(uint8_t node_id, uint8_t idx, void *object) {
  static_cast<CANopenComponent *>(object)->node_event_(node_id, NodeEvent::NODE_EVENT_RESET,
                                                       NMTState::NMT_STATE_BOOTUP);
}
#endif  // USE_CANOPEN_NODE_MONITOR

}  // namespace esphome::canopen
