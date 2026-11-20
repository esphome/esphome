#pragma once

#include <cstdint>

namespace esphome::genibus_ble {

/// The connection bookkeeping of GenibusBLE, free of ESP-IDF types so it can be tested on the host.
/// Once notifications of the GENI characteristic are enabled the link settles for a while. It comes up when it has
/// settled and the connection is encrypted, in either order. A fault (pairing or GATT setup failed) is reported once
/// per series; the series ends when the link comes up.
class LinkState {
 public:
  void set_handles(uint16_t handle, uint16_t notify_descriptor) {
    this->handle_ = handle;
    this->notify_descriptor_ = notify_descriptor;
  }
  uint16_t handle() const { return this->handle_; }
  bool is_up() const { return this->up_; }
  /// Whether the link waits to come up: notifications are enabled, settling or encryption may still be pending.
  bool is_settling() const { return this->settling_; }
  /// Whether the status warning is the transport's own and not one the bus set.
  bool has_fault() const { return this->fault_; }
  /// Whether handle is the GENI characteristic of this connection.
  bool is_ours(uint16_t handle) const { return this->handle_ != 0 && handle == this->handle_; }
  /// Whether handle is the notification descriptor of the GENI characteristic of this connection.
  bool is_our_descriptor(uint16_t handle) const {
    return this->notify_descriptor_ != 0 && handle == this->notify_descriptor_;
  }
  /// Whether a notification of handle carries GENIbus bytes.
  bool accepts(uint16_t handle) const { return this->up_ && this->is_ours(handle); }
  /// Whether a connection is being set up or is up: from the service discovery until down().
  bool is_active() const { return this->handle_ != 0 || this->settling_ || this->up_; }
  /// Whether the link may come up with up(): it has settled and the connection is encrypted.
  bool is_ready() const { return this->settling_ && this->settled_ && this->encrypted_; }

  /// Notifications are enabled; the link starts to settle. Returns false when the link is already up, so a repeated
  /// write of the descriptor does not report the connection twice.
  bool settle() {
    if (this->up_)
      return false;
    this->settling_ = true;
    this->settled_ = false;
    return true;
  }
  /// The settling time is over. Returns whether the link is ready to come up.
  bool settled() {
    if (!this->settling_)
      return false;
    this->settled_ = true;
    return this->is_ready();
  }
  /// The connection is encrypted. Returns whether the link is ready to come up.
  bool encrypted() {
    this->encrypted_ = true;
    return this->is_ready();
  }
  /// Bring a ready link up. Returns true when it ends a fault series, so the transport clears its warning.
  bool up() {
    if (!this->is_ready())
      return false;
    this->settling_ = false;
    this->up_ = true;
    const bool had_fault = this->fault_;
    this->fault_ = false;
    return had_fault;
  }
  /// Take the link down and forget the connection; DISCONNECT and CLOSE may both call it. Returns true only for the
  /// call that ends an up link, after is_up() has turned false, so the transport may end the bus's requests.
  bool down() {
    this->handle_ = 0;
    this->notify_descriptor_ = 0;
    this->settling_ = false;
    this->settled_ = false;
    this->encrypted_ = false;
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
  uint16_t notify_descriptor_{0};
  bool settling_{false};
  bool settled_{false};
  bool encrypted_{false};
  bool up_{false};
  bool fault_{false};
};

}  // namespace esphome::genibus_ble
