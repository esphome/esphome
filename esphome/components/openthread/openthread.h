#pragma once
#include "esphome/core/defines.h"
#ifdef USE_OPENTHREAD

#include "esphome/core/component.h"
#include "esphome/components/mdns/mdns_component.h"
#include "esphome/components/network/ip_address.h"

#include <openthread/thread.h>

#include <vector>
#include <optional>

namespace esphome {
namespace openthread {

class InstanceLock;

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
  void srp_setup_();
  std::optional<otIp6Address> get_omr_address_(std::optional<InstanceLock> &lock);
  std::string host_name_;
  void *pool_alloc_(size_t size);

 private:
  esphome::mdns::MDNSComponent *mdns_{nullptr};
  std::vector<esphome::mdns::MDNSService> mdns_services_;
  std::vector<std::unique_ptr<uint8_t[]>> memory_pool_;
};

extern OpenThreadComponent *global_openthread_component;

class InstanceLock {
 public:
  static std::optional<InstanceLock> try_acquire(int delay);
  static std::optional<InstanceLock> acquire();
  ~InstanceLock();

  // Returns the global openthread instance guarded by this lock
  otInstance *get_instance();

 private:
  // Use a private constructor in order to force thehandling
  // of acquisition failure
  InstanceLock() {}
};

}  // namespace openthread
}  // namespace esphome
#endif
