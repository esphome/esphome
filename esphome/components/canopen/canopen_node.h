#pragma once

#include "esphome/core/defines.h"

#include "canopen.h"
#include "types.h"

namespace esphome::canopen {

// Base class for components that drive one other node on the bus (device profiles, e.g. CiA 402 drives). Filters
// the hub's events to that node and forwards SDO / NMT calls with its node id. The node is monitored when codegen
// registers it with canopen.register_node(..., heartbeat_timeout=...).
class CANopenNode {
 public:
  CANopenNode(CANopenComponent *parent, uint8_t node_id);
  virtual ~CANopenNode() = default;

  CANopenComponent *get_canopen() const { return this->canopen_; }
  uint8_t get_node_id() const { return this->node_id_; }

  // state from the node's last heartbeat (false / UNKNOWN if it isn't monitored)
  bool is_online() const { return this->canopen_->is_node_online(this->node_id_); }
  NMTState get_nmt_state() const { return this->canopen_->get_node_state(this->node_id_); }
  bool is_operational() const { return this->is_online() && this->get_nmt_state() == NMTState::NMT_STATE_OPERATIONAL; }

  // The default SDO context is this CANopenNode subobject, which isn't the address of a subclass that has other
  // bases (e.g. `class Drive : public Component, public CANopenNode`): get the subclass back with
  // CANopenNode::from_context<Drive>(context), not a direct static_cast from void *.
  template<typename T> static T *from_context(void *context) {
    return static_cast<T *>(static_cast<CANopenNode *>(context));
  }

#ifdef USE_CANOPEN_SDO_CLIENT
  // CANopenComponent's SDO calls for this node. A context of nullptr means `this`, so cancel_sdo() drops them.
  template<typename T> bool read_sdo(uint16_t index, uint8_t sub, SDOCallback callback, void *context = nullptr) {
    return this->canopen_->read_sdo<T>(this->node_id_, index, sub, callback, this->context_(context));
  }
  template<typename T>
  bool write_sdo(uint16_t index, uint8_t sub, T value, SDOCallback callback = nullptr, void *context = nullptr) {
    return this->canopen_->write_sdo<T>(this->node_id_, index, sub, value, callback, this->context_(context));
  }
  // data must stay valid until the callback
  bool read_sdo_data(uint16_t index, uint8_t sub, uint8_t *data, size_t capacity, SDOCallback callback,
                     void *context = nullptr) {
    return this->canopen_->read_sdo_data(this->node_id_, index, sub, data, capacity, callback, this->context_(context));
  }
  // data must stay valid until the callback (not copied)
  bool write_sdo_data(uint16_t index, uint8_t sub, const uint8_t *data, size_t len, SDOCallback callback = nullptr,
                      void *context = nullptr) {
    return this->canopen_->write_sdo_data(this->node_id_, index, sub, data, len, callback, this->context_(context));
  }
  // drops the queued requests of context (default: this) and aborts its transfer in flight, without callbacks
  void cancel_sdo(void *context = nullptr) { this->canopen_->cancel_sdo(this->context_(context)); }
#endif
#ifdef USE_CANOPEN_NMT_MASTER
  bool send_nmt(NMTCommand command) { return this->canopen_->send_nmt(this->node_id_, command); }
#endif

 protected:
  // Events of this node, see NodeEvent. on_node_nmt_changed() also follows on_node_online() (unknown -> state) and
  // on_node_offline() (state -> unknown): act on online / reset, use nmt_changed as state information.
  virtual void on_node_online() {}
  virtual void on_node_offline() {}
  virtual void on_node_nmt_changed(NMTState state) {}
  // boot-up message: the node restarted and lost the configuration it got at runtime
  virtual void on_node_reset() {}
  virtual void on_node_emcy(const Emergency &emergency) {}

  void *context_(void *context) { return context != nullptr ? context : this; }

  CANopenComponent *canopen_;
  uint8_t node_id_;
};

}  // namespace esphome::canopen
