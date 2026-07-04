#pragma once
#include "esphome/core/defines.h"
#ifdef USE_OPENTHREAD

#include "esphome/components/mdns/mdns_component.h"
#include "esphome/components/network/ip_address.h"
#include "esphome/core/component.h"

#include <openthread/srp_client.h>
#include <openthread/srp_client_buffers.h>
#include <openthread/instance.h>
#include <openthread/thread.h>

#include <atomic>
#include <optional>
#include <vector>

namespace esphome::openthread {

class InstanceLock;

template<typename... Ts> class OpenThreadComponentPollPeriodAction;

class OpenThreadComponent final : public Component {
 public:
  OpenThreadComponent();
  ~OpenThreadComponent();
  void dump_config() override;
  void setup() override;
  bool teardown() override;
  float get_setup_priority() const override { return setup_priority::WIFI; }

  bool is_connected() const { return this->connected_; }
  /// Returns true once esp_openthread_init() has completed and the OT lock is usable.
  bool is_lock_initialized() const { return this->lock_initialized_; }
  network::IPAddresses get_ip_addresses();
  std::optional<otIp6Address> get_omr_address();
  void ot_main();
  void on_factory_reset(std::function<void()> callback);
  void defer_factory_reset_external_callback();

  const char *get_use_address() const { return this->use_address_; }
  void set_use_address(const char *use_address) { this->use_address_ = use_address; }
#if CONFIG_OPENTHREAD_MTD
  void set_poll_period(uint32_t poll_period) { this->poll_period_ = poll_period; }
  uint32_t get_poll_period() const { return this->poll_period_; }
#endif
  void set_output_power(int8_t output_power) { this->output_power_ = output_power; }
  void set_connected(bool connected) { this->connected_ = connected; }
  static void on_state_changed(otChangedFlags flags, void *context);

 protected:
  // Actions re-apply link mode under the OT lock; allow them to call apply_linkmode_()
  // without exposing this lock-sensitive, raw-instance method on the public API.
  template<typename... Ts> friend class OpenThreadComponentPollPeriodAction;

  /** Apply Link Mode settings (incl poll period).
   * Callers running outside the OpenThread task must hold InstanceLock.
   * ot_main() runs on the OpenThread task itself and must not acquire the lock.
   */
  void apply_linkmode_(otInstance *instance);

  std::optional<otIp6Address> get_omr_address_(InstanceLock &lock);
  otInstance *get_openthread_instance_();
  int openthread_stop_();
  std::function<void()> factory_reset_external_callback_;
#if CONFIG_OPENTHREAD_MTD
  uint32_t poll_period_{0};
#endif
  std::optional<int8_t> output_power_{};
  std::atomic<bool> lock_initialized_{false};
  bool teardown_started_{false};
  bool teardown_complete_{false};
  bool connected_{false};

 private:
  // Stores a pointer to a string literal (static storage duration).
  // ONLY set from Python-generated code with string literals - never dynamic strings.
  const char *use_address_{""};
};

extern OpenThreadComponent *global_openthread_component;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

class OpenThreadSrpComponent final : public Component {
 public:
  void set_mdns(esphome::mdns::MDNSComponent *mdns);
  // This has to run after the mdns component or else no services are available to advertise
  float get_setup_priority() const override { return this->mdns_->get_setup_priority() - 1.0f; }
  void setup() override;
  static void srp_callback(otError err, const otSrpClientHostInfo *host_info, const otSrpClientService *services,
                           const otSrpClientService *removed_services, void *context);
  static void srp_start_callback(const otSockAddr *server_socket_address, void *context);
  static void srp_factory_reset_callback(otError err, const otSrpClientHostInfo *host_info,
                                         const otSrpClientService *services, const otSrpClientService *removed_services,
                                         void *context);

 protected:
  esphome::mdns::MDNSComponent *mdns_{nullptr};
  std::vector<std::unique_ptr<uint8_t[]>> memory_pool_;
  void *pool_alloc_(size_t size);
};

// RAII guard for the OpenThread API lock. Modeled on std::unique_lock: the
// guard may or may not own the lock (try_acquire can fail), so check it with
// operator bool before use. Non-copyable and non-movable: the factories return
// by value via guaranteed copy elision, so a guard is never duplicated and the
// lock is released exactly once, when the owning guard goes out of scope.
class InstanceLock {
 public:
  // May fail to acquire within delay ms; check the returned guard with operator bool.
  static InstanceLock try_acquire(int delay);
  // Blocks until the lock is held.
  static InstanceLock acquire();
  InstanceLock(const InstanceLock &) = delete;
  InstanceLock(InstanceLock &&) = delete;
  InstanceLock &operator=(const InstanceLock &) = delete;
  InstanceLock &operator=(InstanceLock &&) = delete;
  ~InstanceLock();

  explicit operator bool() const { return this->owns_; }

  // Returns the global openthread instance. Only valid on an owning guard
  // (operator bool is true); the instance must not be used without the lock held.
  otInstance *get_instance();

 private:
  explicit InstanceLock(bool owns) : owns_(owns) {}
  bool owns_;
};

}  // namespace esphome::openthread
#endif
