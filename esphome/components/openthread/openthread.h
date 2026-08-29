#pragma once
#include "esphome/core/defines.h"
#ifdef USE_OPENTHREAD

#include "esphome/components/mdns/mdns_component.h"
#include "esphome/components/network/ip_address.h"
#include "esphome/core/component.h"
#include "esphome/core/gpio.h"

#include <openthread/srp_client.h>
#include <openthread/srp_client_buffers.h>
#include <openthread/instance.h>
#include <openthread/thread.h>

#include <atomic>
#include <optional>
#include <vector>

#ifdef USE_ESP32
#include "esp_netif.h"
#endif

namespace esphome::openthread {

class InstanceLock;

enum class TeardownStage : uint8_t {
  TEARDOWN_STAGE_NOT_STARTED = 0,
  TEARDOWN_STAGE_STOP_IN_PROCESS,
  TEARDOWN_STAGE_COMPLETED,
};

template<typename... Ts> class OpenThreadComponentPollPeriodAction;

class OpenThreadComponent final : public Component {
 public:
  OpenThreadComponent();
#ifdef USE_OPENTHREAD_RCP_UART
  OpenThreadComponent(uint32_t rcp_baud_rate, int rcp_rx_pin, int rcp_tx_pin, int rcp_reset_pin,
                      bool rcp_reset_active_level);
#endif
  ~OpenThreadComponent();
  void dump_config() override;
  void setup() override;
  bool teardown() override;
  float get_setup_priority() const override {
#ifdef USE_OPENTHREAD_BORDER_ROUTER
    return setup_priority::WIFI - 1.0f;
#else
    return setup_priority::WIFI;
#endif
  }

  bool is_connected() const { return this->connected_; }
  /// Returns true once esp_openthread_init() has completed and the OT lock is usable.
  bool is_lock_initialized() const { return this->lock_initialized_; }
  bool is_ready() const { return this->ready_; }
  bool has_task_failed() const { return this->task_failed_; }
  network::IPAddresses get_ip_addresses();
  std::optional<otIp6Address> get_omr_address();
  void ot_main();
  void on_factory_reset(std::function<void()> callback);
  void defer_factory_reset_external_callback();

  /// Returns nullptr when no explicit use_address is configured and the address is
  /// derived at runtime from the device name (see network::get_use_address_to()).
  const char *get_use_address() const { return this->use_address_; }
  void set_use_address(const char *use_address) { this->use_address_ = use_address; }
#if CONFIG_OPENTHREAD_MTD
  void set_poll_period(uint32_t poll_period) { this->poll_period_ = poll_period; }
  uint32_t get_poll_period() const { return this->poll_period_; }
#endif
  void set_output_power(int8_t output_power) { this->output_power_ = output_power; }
  void set_connected(bool connected) { this->connected_ = connected; }
  static void on_state_changed(otChangedFlags flags, void *context);
#ifdef USE_OPENTHREAD_RCP_UART
  static void rcp_failure_handler();
#endif

 protected:
  // Actions re-apply link mode under the OT lock; allow them to call apply_linkmode_()
  // without exposing this lock-sensitive, raw-instance method on the public API.
  template<typename... Ts> friend class OpenThreadComponentPollPeriodAction;

  /** Apply Link Mode settings (incl poll period).
   * Callers running outside the OpenThread task must hold InstanceLock.
   * ot_main() runs on the OpenThread task itself and must not acquire the lock.
   */
  void apply_linkmode_(otInstance *instance);
  void mark_task_failed_();
#ifdef USE_OPENTHREAD_RCP_UART
  void reset_rcp_();
#endif

  std::optional<otIp6Address> get_omr_address_(InstanceLock &lock);
  otInstance *get_openthread_instance_();
  int openthread_stop_();
  std::function<void()> factory_reset_external_callback_;
#if CONFIG_OPENTHREAD_MTD
  uint32_t poll_period_{0};
#endif
  std::optional<int8_t> output_power_{};
  std::atomic<bool> lock_initialized_{false};
  std::atomic<TeardownStage> teardown_stage_{TeardownStage::TEARDOWN_STAGE_NOT_STARTED};
  std::atomic<bool> ready_{false};
  std::atomic<bool> task_failed_{false};
  std::atomic<bool> connected_{false};
#ifdef USE_ESP32
  std::atomic<esp_netif_t *> openthread_netif_{nullptr};
#endif
#ifdef USE_OPENTHREAD_RCP_UART
  uint32_t rcp_baud_rate_{0};
  int rcp_rx_pin_{-1};
  int rcp_tx_pin_{-1};
  int rcp_reset_pin_{-1};
  bool rcp_reset_active_level_{false};
  uint8_t rcp_reset_attempts_{0};
#endif

 private:
  // Stores a pointer to a string literal (static storage duration).
  // ONLY set from Python-generated code with string literals - never dynamic strings.
  const char *use_address_{nullptr};
};

extern OpenThreadComponent *global_openthread_component;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

#ifdef USE_OPENTHREAD_BORDER_ROUTER
class OpenThreadBorderRouterComponent final : public Component {
 public:
  explicit OpenThreadBorderRouterComponent(OpenThreadComponent *openthread) : openthread_(openthread) {}

  void setup() override;
  void loop() override;
  void dump_config() override;
  bool teardown() override;
  // Runs after mDNS (AFTER_CONNECTION) so the border router's own SRP/mDNS integration
  // sees a fully set up mDNS component.
  float get_setup_priority() const override { return setup_priority::AFTER_CONNECTION - 1.0f; }

 protected:
  OpenThreadComponent *openthread_;
  bool started_{false};
  uint16_t lock_wait_failures_{0};
};
#endif

#ifdef USE_OPENTHREAD_ANTENNA_SWITCH
// Configures a GPIO-based RF antenna switch (e.g. the Seeed Studio XIAO ESP32-C6's
// onboard-ceramic vs external u.FL antenna select). Runs at HARDWARE priority so the
// antenna is selected before Wi-Fi or the native 802.15.4 radio start using it.
class OpenThreadAntennaSwitchComponent final : public Component {
 public:
  OpenThreadAntennaSwitchComponent(GPIOPin *select_pin, bool external_antenna, GPIOPin *enable_pin = nullptr)
      : select_pin_(select_pin), external_antenna_(external_antenna), enable_pin_(enable_pin) {}

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

 protected:
  GPIOPin *select_pin_;
  bool external_antenna_;
  GPIOPin *enable_pin_;
};
#endif

#ifndef USE_OPENTHREAD_BORDER_ROUTER
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
#endif

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
