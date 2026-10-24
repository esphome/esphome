#pragma once

#include "canopen.h"
#include "esphome/core/hal.h"

namespace esphome::canopen {

// Profile components inherit this alongside Component/PollingComponent and share one hub per bus.
class CANopenNode {
 public:
  CANopenNode(CANopenComponent *parent, uint8_t node) : parent_(parent), node_(node) {}

  // Call once during setup; nodes and registered callbacks must outlive the hub.
  void setup_canopen() {
    this->parent_->add_on_heartbeat_callback([this](uint8_t node, uint8_t state) {
      if (node == this->node_) {
        this->last_heartbeat_ms_ = millis();
        this->heartbeat_seen_ = true;
        this->nmt_state_ = state;
        this->heartbeat_callback_.call(state);
      }
    });
    this->parent_->add_on_emcy_callback([this](uint8_t node, const Emergency &emergency) {
      if (node == this->node_)
        this->emcy_callback_.call(emergency);
    });
  }

  bool read_sdo(uint16_t index, uint8_t subindex, SDOCallback callback, void *context) {
    return this->parent_->read_sdo(this->node_, index, subindex, callback, context);
  }
  bool write_sdo(uint16_t index, uint8_t subindex, uint32_t value, uint8_t size,
                 SDOCallback callback = nullptr, void *context = nullptr) {
    return this->parent_->write_sdo(this->node_, index, subindex, value, size, callback, context);
  }
  bool send_nmt(NMTCommand command) { return this->parent_->send_nmt(this->node_, command); }
  bool heartbeat_seen() const { return this->heartbeat_seen_; }
  uint8_t get_nmt_state() const { return this->nmt_state_; }
  uint32_t heartbeat_age(uint32_t now) const { return now - this->last_heartbeat_ms_; }
  bool is_online(uint32_t now, uint32_t timeout) const {
    return this->heartbeat_seen_ && this->heartbeat_age(now) < timeout;
  }
  template<typename F> void add_on_heartbeat_callback(F &&callback) {
    this->heartbeat_callback_.add(std::forward<F>(callback));
  }
  template<typename F> void add_on_emcy_callback(F &&callback) {
    this->emcy_callback_.add(std::forward<F>(callback));
  }

 protected:
  CANopenComponent *parent_;
  LazyCallbackManager<void(uint8_t)> heartbeat_callback_;
  LazyCallbackManager<void(const Emergency &)> emcy_callback_;
  uint32_t last_heartbeat_ms_{0};
  uint8_t node_;
  uint8_t nmt_state_{0};
  bool heartbeat_seen_{false};
};

}  // namespace esphome::canopen
