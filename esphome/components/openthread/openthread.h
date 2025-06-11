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

  bool is_connected();
  network::IPAddresses get_ip_addresses();
  std::optional<otIp6Address> get_omr_address();
  void ot_main();

 protected:
  std::optional<otIp6Address> get_omr_address_(std::optional<InstanceLock> &lock);
};

extern OpenThreadComponent *global_openthread_component;

class OpenThreadSrpComponent : public Component {
 public:
  void set_mdns(esphome::mdns::MDNSComponent *mdns);
  void set_host_name(std::string host_name);
  // This has to run after the mdns component or else no services are available to advertise
  float get_setup_priority() const override { return this->mdns_->get_setup_priority() - 1.0; }

 protected:
  void setup() override;

 private:
  std::string host_name_;
  esphome::mdns::MDNSComponent *mdns_{nullptr};
  std::vector<esphome::mdns::MDNSService> mdns_services_;
  std::vector<std::unique_ptr<uint8_t[]>> memory_pool_;
  void *pool_alloc_(size_t size);
};

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
