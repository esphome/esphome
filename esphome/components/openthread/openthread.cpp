#include "esphome/core/defines.h"
#ifdef USE_OPENTHREAD
#include "openthread.h"

#include <freertos/portmacro.h>

#include <openthread/cli.h>
#include <openthread/instance.h>
#include <openthread/logging.h>
#include <openthread/netdata.h>
#include <openthread/srp_client.h>
#include <openthread/srp_client_buffers.h>
#include <openthread/tasklet.h>

#include <cstring>

#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

static const char *const TAG = "openthread";

namespace esphome {
namespace openthread {

OpenThreadComponent *global_openthread_component =  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
    nullptr;                                        // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

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
  InstanceLock lock = InstanceLock::acquire();
  return this->get_omr_address_(lock);
}

std::optional<otIp6Address> OpenThreadComponent::get_omr_address_(InstanceLock &lock) {
  otNetworkDataIterator iterator = OT_NETWORK_DATA_ITERATOR_INIT;
  otInstance *instance = nullptr;

  instance = lock.get_instance();

  otBorderRouterConfig config;
  if (otNetDataGetNextOnMeshPrefix(instance, &iterator, &config) != OT_ERROR_NONE) {
    return std::nullopt;
  }

  const otIp6Prefix *omr_prefix = &config.mPrefix;
  const otNetifAddress *unicast_addresses = otIp6GetUnicastAddresses(instance);
  for (const otNetifAddress *addr = unicast_addresses; addr; addr = addr->mNext) {
    const otIp6Address *local_ip = &addr->mAddress;
    if (otIp6PrefixMatch(&omr_prefix->mPrefix, local_ip)) {
      return *local_ip;
    }
  }
  return {};
}

void srp_callback(otError err, const otSrpClientHostInfo *host_info, const otSrpClientService *services,
                  const otSrpClientService *removed_services, void *context) {
  if (err != 0) {
    ESP_LOGW(TAG, "SRP client reported an error: %s", otThreadErrorToString(err));
    for (const otSrpClientHostInfo *host = host_info; host; host = nullptr) {
      ESP_LOGW(TAG, "  Host: %s", host->mName);
    }
    for (const otSrpClientService *service = services; service; service = service->mNext) {
      ESP_LOGW(TAG, "  Service: %s", service->mName);
    }
  }
}

void srp_start_callback(const otSockAddr *server_socket_address, void *context) {
  ESP_LOGI(TAG, "SRP client has started");
}

void OpenThreadSrpComponent::setup() {
  otError error;
  InstanceLock lock = InstanceLock::acquire();
  otInstance *instance = lock.get_instance();

  otSrpClientSetCallback(instance, srp_callback, nullptr);

  // set the host name
  uint16_t size;
  char *existing_host_name = otSrpClientBuffersGetHostNameString(instance, &size);
  const std::string &host_name = App.get_name();
  uint16_t host_name_len = host_name.size();
  if (host_name_len > size) {
    ESP_LOGW(TAG, "Hostname is too long, choose a shorter project name");
    return;
  }
  memset(existing_host_name, 0, size);
  memcpy(existing_host_name, host_name.c_str(), host_name_len);

  error = otSrpClientSetHostName(instance, existing_host_name);
  if (error != 0) {
    ESP_LOGW(TAG, "Could not set host name");
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
  ESP_LOGD(TAG, "Setting up SRP services. count = %d\n", this->mdns_services_.size());
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
    if (host_name_len > size) {
      ESP_LOGW(TAG, "Instance name too long: %s", host_name.c_str());
      continue;
    }
    memset(string, 0, size);
    memcpy(string, host_name.c_str(), host_name_len);

    // Set port
    entry->mService.mPort = const_cast<TemplatableValue<uint16_t> &>(service.port).value();

    otDnsTxtEntry *txt_entries =
        reinterpret_cast<otDnsTxtEntry *>(this->pool_alloc_(sizeof(otDnsTxtEntry) * service.txt_records.size()));
    // Set TXT records
    entry->mService.mNumTxtEntries = service.txt_records.size();
    for (size_t i = 0; i < service.txt_records.size(); i++) {
      const auto &txt = service.txt_records[i];
      auto value = const_cast<TemplatableValue<std::string> &>(txt.value).value();
      txt_entries[i].mKey = strdup(txt.key.c_str());
      txt_entries[i].mValue = reinterpret_cast<const uint8_t *>(strdup(value.c_str()));
      txt_entries[i].mValueLength = value.size();
    }
    entry->mService.mTxtEntries = txt_entries;
    entry->mService.mNumTxtEntries = service.txt_records.size();

    // Add service
    error = otSrpClientAddService(instance, &entry->mService);
    if (error != OT_ERROR_NONE) {
      ESP_LOGW(TAG, "Failed to add service: %s", otThreadErrorToString(error));
    }
    ESP_LOGD(TAG, "Added service: %s", full_service.c_str());
  }

  otSrpClientEnableAutoStartMode(instance, srp_start_callback, nullptr);
  ESP_LOGD(TAG, "Finished SRP setup");
}

void *OpenThreadSrpComponent::pool_alloc_(size_t size) {
  uint8_t *ptr = new uint8_t[size];
  this->memory_pool_.emplace_back(std::unique_ptr<uint8_t[]>(ptr));
  return ptr;
}

void OpenThreadSrpComponent::set_mdns(esphome::mdns::MDNSComponent *mdns) { this->mdns_ = mdns; }

}  // namespace openthread
}  // namespace esphome

#endif
