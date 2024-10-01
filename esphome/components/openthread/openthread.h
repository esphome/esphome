#pragma once

#include "esphome/core/component.h"
#include "esphome/components/mdns/mdns_component.h"
#include "esphome/components/network/ip_address.h"

#include <openthread/thread.h>

#include <vector>
#include <optional>

namespace esphome {
namespace openthread {

class OpenThreadLockGuard;

class OpenThreadComponent : public Component {
 public:
  OpenThreadComponent();
  ~OpenThreadComponent();
  void setup() override;
  float get_setup_priority() const override { return setup_priority::WIFI; }

  void set_host_name(std::string host_name);
  void set_mdns(esphome::mdns::MDNSComponent *mdns);
  bool is_connected();
  network::IPAddresses get_ip_addresses();
  std::optional<otIp6Address> get_omr_address();
  void ot_main();

 protected:
  void srp_setup();
  std::optional<otIp6Address> get_omr_address(std::optional<OpenThreadLockGuard> &lock);
  std::string host_name;
  void *pool_alloc(size_t size);

 private:
  // void platform_init();

  esphome::mdns::MDNSComponent *mdns_{nullptr};
  std::vector<esphome::mdns::MDNSService> mdns_services_;
  std::vector<std::unique_ptr<uint8_t[]>> _memory_pool;
};

extern OpenThreadComponent *global_openthread_component;

class OpenThreadLockGuard {
 public:
  static std::optional<OpenThreadLockGuard> TryAcquire(int delay);
  static std::optional<OpenThreadLockGuard> Acquire();
  ~OpenThreadLockGuard();

  // Returns the global openthread instance guarded by this lock
  otInstance *get_instance();

 private:
  // Use a private constructor in order to force thehandling
  // of acquisition failure
  OpenThreadLockGuard() {}
};

}  // namespace openthread
}  // namespace esphome
