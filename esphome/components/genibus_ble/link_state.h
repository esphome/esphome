#pragma once

#include <cstdint>

namespace esphome::genibus_ble {

/// The connection bookkeeping of GenibusBLE, free of ESP-IDF types so it can be tested on the host.
/// Once notifications of the GENI characteristic are registered the link settles for a while, then it is up. A fault
/// (pairing or GATT setup failed) is reported once per series; the series ends when the link comes up.
class LinkState {
 public:
  void set_handle(uint16_t handle) { this->handle_ = handle; }
  uint16_t handle() const { return this->handle_; }
  bool is_up() const { return this->up_; }
  /// Whether notifications are registered and the link waits to come up.
  bool is_settling() const { return this->settling_; }
  /// Whether the status warning is the transport's own and not one the bus set.
  bool has_fault() const { return this->fault_; }
  /// Whether handle is the GENI characteristic of this connection.
  bool is_ours(uint16_t handle) const { return this->handle_ != 0 && handle == this->handle_; }
  /// Whether a notification of handle carries GENIbus bytes.
  bool accepts(uint16_t handle) const { return this->up_ && this->is_ours(handle); }

  /// Notifications are registered; the link comes up with up() once it has settled. Returns false when the link is
  /// already up, so a repeated registration does not report the connection twice.
  bool settle() {
    if (this->up_)
      return false;
    this->settling_ = true;
    return true;
  }
  /// Bring a settling link up. Returns true when it ends a fault series, so the transport clears its warning.
  bool up() {
    if (!this->settling_)
      return false;
    this->settling_ = false;
    this->up_ = true;
    const bool had_fault = this->fault_;
    this->fault_ = false;
    return had_fault;
  }
  /// Take the link down; DISCONNECT and CLOSE may both call it. Returns true only for the call that ends an up link,
  /// after is_up() has turned false, so the transport may end the bus's requests.
  bool down() {
    this->handle_ = 0;
    this->settling_ = false;
    if (!this->up_)
      return false;
    this->up_ = false;
    return true;
  }
  /// Record a fault. Returns true for the first fault of a series, the one to warn about.
  bool fault() {
    const bool first = !this->fault_;
    this->fault_ = true;
    return first;
  }

 protected:
  uint16_t handle_{0};
  bool settling_{false};
  bool up_{false};
  bool fault_{false};
};

}  // namespace esphome::genibus_ble
