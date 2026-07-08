#pragma once

#include "esphome/core/defines.h"
#ifdef USE_PROVISIONING
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#include <cstdint>

namespace esphome::provisioning {

// Central provisioning-window manager (EN18031). A device that ships unprovisioned
// (secure transports enabled with no credentials, configured by the controller on
// first connection) opens a provisioning window at boot. Each transport that needs
// provisioning registers as a "source" and reports its state; the device is
// considered provisioned once every registered source is provisioned.
//
// Network connectivity is a built-in source: a device with a network interface but
// no other provisioning-capable component (no api encryption, etc.) is still
// considered provisioned once it has connected via any interface -- so an
// Improv-only device reports its state correctly.
//
// If the window times out while still unprovisioned it closes: the closed state is
// RAM-only (a power cycle / reset reopens it) and the `on_timeout` automation fires.
// Components query window_pending()/closed() to suppress reboot timeouts and refuse
// further provisioning. This manager owns no transport knowledge; transports
// (api, and later mqtt/wireguard/...) drive it through the source API.
class ProvisioningManager : public Component {
 public:
  // Maximum number of provisioning sources, limited by the width of the state masks.
  static constexpr uint8_t MAX_SOURCES = 32;

  ProvisioningManager();

  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::BEFORE_CONNECTION; }

  void set_timeout(uint32_t timeout) { this->timeout_ = timeout; }

  // Register a provisioning source. Returns a bit index the source uses to report
  // its state via set_source_provisioned(). Call once, from the source's setup().
  uint8_t register_source();
  // Report whether the given source currently holds valid credentials.
  void set_source_provisioned(uint8_t source, bool provisioned) {
    if (source >= MAX_SOURCES)
      return;
    if (provisioned) {
      this->provisioned_mask_ |= (1UL << source);
    } else {
      this->provisioned_mask_ &= ~(1UL << source);
    }
  }

  // True once every registered source is provisioned. Config validation guarantees
  // at least one source, and the built-in connectivity source registers in the
  // constructor, so registered_mask_ is never zero in practice.
  bool is_provisioned() const { return (this->provisioned_mask_ & this->registered_mask_) == this->registered_mask_; }
  // True while provisioning is still pending: the device is unprovisioned, whether
  // the window is still open or has already closed. Reboot timeouts are suppressed
  // while this holds so the device never auto-reboots (and silently reopens the
  // window) while unprovisioned.
  bool window_pending() const { return !this->is_provisioned(); }
  // True once the window has expired without the device being provisioned.
  bool closed() const { return this->closed_; }

  // Register a callback fired once when the window closes (runtime expiry). Used
  // internally by transports/Improv to stop accepting provisioning. The user-facing
  // on_timeout automation is wired to get_timeout_trigger() instead.
  template<typename F> void add_on_closed_callback(F &&callback) {
    this->closed_callback_.add(std::forward<F>(callback));
  }
  Trigger<> *get_timeout_trigger() { return &this->timeout_trigger_; }

 protected:
  void close_window_();

  Trigger<> timeout_trigger_;
  LazyCallbackManager<void()> closed_callback_;
  uint32_t timeout_{0};
  uint32_t registered_mask_{0};
  uint32_t provisioned_mask_{0};
  uint8_t source_count_{0};
  bool closed_{false};
#ifdef USE_NETWORK
  // Built-in connectivity source (see loop()): registered in the constructor and
  // latched provisioned once the device has connected via any network interface.
  uint8_t network_source_{0};
#endif
};

extern ProvisioningManager *global_provisioning_manager;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esphome::provisioning
#endif  // USE_PROVISIONING
