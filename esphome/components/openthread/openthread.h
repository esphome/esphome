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

#include <optional>
#include <vector>

namespace esphome::openthread {

class InstanceLock;

class OpenThreadComponent : public Component {
 public:
  OpenThreadComponent();
  ~OpenThreadComponent();
  void dump_config() override;
  void setup() override;
  bool teardown() override;
  float get_setup_priority() const override { return setup_priority::WIFI; }

  bool is_connected() const { return this->connected_; }
  network::IPAddresses get_ip_addresses();
  std::optional<otIp6Address> get_omr_address();
  void ot_main();
  void on_factory_reset(std::function<void()> callback);
  void defer_factory_reset_external_callback();

  const char *get_use_address() const;
  void set_use_address(const char *use_address);
#if CONFIG_OPENTHREAD_MTD
  void set_poll_period(uint32_t poll_period) { this->poll_period_ = poll_period; }
#endif
  void set_output_power(int8_t output_power) { this->output_power_ = output_power; }

 protected:
  std::optional<otIp6Address> get_omr_address_(InstanceLock &lock);
  static void on_state_changed_(otChangedFlags flags, void *context);
  std::function<void()> factory_reset_external_callback_;
#if CONFIG_OPENTHREAD_MTD
  uint32_t poll_period_{0};
#endif
  std::optional<int8_t> output_power_{};
  bool teardown_started_{false};
  bool teardown_complete_{false};
  bool connected_{false};

 private:
  // Stores a pointer to a string literal (static storage duration).
  // ONLY set from Python-generated code with string literals - never dynamic strings.
  const char *use_address_{""};
};

extern OpenThreadComponent *global_openthread_component;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

class OpenThreadSrpComponent : public Component {
 public:
  void set_mdns(esphome::mdns::MDNSComponent *mdns);
  // This has to run after the mdns component or else no services are available to advertise
  float get_setup_priority() const override { return this->mdns_->get_setup_priority() - 1.0; }
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

class InstanceLock {
 public:
  static std::optional<InstanceLock> try_acquire(int delay);
  static InstanceLock acquire();
  ~InstanceLock();

  // Returns the global openthread instance guarded by this lock
  otInstance *get_instance();

 private:
  // Use a private constructor in order to force the handling
  // of acquisition failure
  InstanceLock() {}
};

}  // namespace esphome::openthread
#endif
