#include "esphome/core/defines.h"

#ifdef USE_OPENTHREAD
#include "openthread.h"

#include <freertos/portmacro.h>

#include <openthread/srp_client.h>
#include <openthread/srp_client_buffers.h>
#include <openthread/netdata.h>
#include <openthread/cli.h>
#include <openthread/instance.h>
#include <openthread/logging.h>
#include <openthread/tasklet.h>

#include <cstring>

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#define TAG "openthread"

namespace esphome {
namespace openthread {

OpenThreadComponent *global_openthread_component = nullptr;

OpenThreadComponent::OpenThreadComponent() { global_openthread_component = this; }

OpenThreadComponent::~OpenThreadComponent() {
  auto lock = InstanceLock::try_acquire(100);
  if (!lock) {
    ESP_LOGW(TAG, "Failed to acquire OpenThread lock in destructor, leaking memory");
    return;
  }
  otInstance *instance = lock->get_instance();
  otSrpClientClearHostAndServices(instance);
  otSrpClientBuffersFreeAllServices(instance);
  global_openthread_component = nullptr;
}

bool OpenThreadComponent::is_connected() {
  auto lock = InstanceLock::try_acquire(100);
  if (!lock) {
    ESP_LOGW(TAG, "Failed to acquire OpenThread lock in is_connected");
    return false;
  }

  otInstance *instance = lock->get_instance();
  if (instance == nullptr) {
    return false;
  }

  otDeviceRole role = otThreadGetDeviceRole(instance);

  // TODO: If we're a leader, check that there is at least 1 known peer
  return role >= OT_DEVICE_ROLE_CHILD;
}

// Gets the off-mesh routable address
std::optional<otIp6Address> OpenThreadComponent::get_omr_address() {
  auto lock = InstanceLock::acquire();
  return this->get_omr_address_(lock);
}

std::optional<otIp6Address> OpenThreadComponent::get_omr_address_(std::optional<InstanceLock> &lock) {
  otNetworkDataIterator iterator = OT_NETWORK_DATA_ITERATOR_INIT;
  otInstance *instance = nullptr;

  instance = lock->get_instance();

  otBorderRouterConfig aConfig;
  if (otNetDataGetNextOnMeshPrefix(instance, &iterator, &aConfig) != OT_ERROR_NONE) {
    return std::nullopt;
  }

  const otIp6Prefix *omrPrefix = &aConfig.mPrefix;
  const otNetifAddress *unicastAddrs = otIp6GetUnicastAddresses(instance);
  for (const otNetifAddress *addr = unicastAddrs; addr; addr = addr->mNext) {
    const otIp6Address *localIp = &addr->mAddress;
    if (otIp6PrefixMatch(&omrPrefix->mPrefix, localIp)) {
      return *localIp;
    }
  }
  return {};
}

void srpCallback(otError aError, const otSrpClientHostInfo *aHostInfo, const otSrpClientService *aServices,
                 const otSrpClientService *aRemovedServices, void *aContext) {
  ESP_LOGW(TAG, "*** SRP callback *** error=%d hostInfo=%p services=%p removedServices=%p", aError, aHostInfo,
           aServices, aRemovedServices);
}

void srpStartCallback(const otSockAddr *aServerSockAddr, void *aContext) {
  ESP_LOGW(TAG, "*** SRP start callback ***");
}

void OpenThreadComponent::srp_setup_() {
  otError error;
  auto lock = InstanceLock::acquire();
  otInstance *instance = lock->get_instance();

  otSrpClientSetCallback(instance, srpCallback, nullptr);

  // set the host name
  uint16_t size;
  char *existing_host_name = otSrpClientBuffersGetHostNameString(instance, &size);
  uint16_t len = this->host_name_.size();
  if (len > size) {
    ESP_LOGW(TAG, "Hostname is too long, choose a shorter project name");
    return;
  }
  memcpy(existing_host_name, this->host_name_.c_str(), len + 1);

  error = otSrpClientSetHostName(instance, existing_host_name);
  if (error != 0) {
    ESP_LOGW(TAG, "Could not set host name with srp server");
    return;
  }

  error = otSrpClientEnableAutoHostAddress(instance);
  if (error != 0) {
    ESP_LOGW(TAG, "Could not enable auto host address");
    return;
  }

  // Copy the mdns services to our local instance so that the c_str pointers remain valid for the lifetime of this
  // component
  this->mdns_services_ = this->mdns_->get_services();
  for (const auto &service : this->mdns_services_) {
    otSrpClientBuffersServiceEntry *entry = otSrpClientBuffersAllocateService(instance);
    if (!entry) {
      ESP_LOGW(TAG, "Failed to allocate service entry");
      continue;
    }

    // Set service name
    char *string = otSrpClientBuffersGetServiceEntryServiceNameString(entry, &size);
    std::string full_service = service.service_type + "." + service.proto;
    if (full_service.size() > size) {
      ESP_LOGW(TAG, "Service name too long: %s", full_service.c_str());
      continue;
    }
    memcpy(string, full_service.c_str(), full_service.size() + 1);

    // Set instance name (using host_name)
    string = otSrpClientBuffersGetServiceEntryInstanceNameString(entry, &size);
    if (this->host_name_.size() > size) {
      ESP_LOGW(TAG, "Instance name too long: %s", this->host_name_.c_str());
      continue;
    }
    memcpy(string, this->host_name_.c_str(), this->host_name_.size() + 1);

    // Set port
    entry->mService.mPort = service.port;

    otDnsTxtEntry *mTxtEntries =
        reinterpret_cast<otDnsTxtEntry *>(this->pool_alloc_(sizeof(otDnsTxtEntry) * service.txt_records.size()));
    // Set TXT records
    entry->mService.mNumTxtEntries = service.txt_records.size();
    for (size_t i = 0; i < service.txt_records.size(); i++) {
      const auto &txt = service.txt_records[i];
      mTxtEntries[i].mKey = txt.key.c_str();
      mTxtEntries[i].mValue = reinterpret_cast<const uint8_t *>(txt.value.c_str());
      mTxtEntries[i].mValueLength = txt.value.size();
    }
    entry->mService.mTxtEntries = mTxtEntries;
    entry->mService.mNumTxtEntries = service.txt_records.size();

    // Add service
    error = otSrpClientAddService(instance, &entry->mService);
    if (error != OT_ERROR_NONE) {
      ESP_LOGW(TAG, "Failed to add service: %s", otThreadErrorToString(error));
    }
  }

  otSrpClientEnableAutoStartMode(instance, srpStartCallback, nullptr);
  ESP_LOGW(TAG, "Finished SRP setup **** ");
}

void *OpenThreadComponent::pool_alloc_(size_t size) {
  uint8_t *ptr = new uint8_t[size];
  if (ptr) {
    this->memory_pool_.emplace_back(std::unique_ptr<uint8_t[]>(ptr));
  }
  return ptr;
}

void OpenThreadComponent::set_host_name(std::string host_name) { this->host_name_ = host_name; }

void OpenThreadComponent::set_mdns(esphome::mdns::MDNSComponent *mdns) { this->mdns_ = mdns; }

}  // namespace openthread
}  // namespace esphome

#endif
