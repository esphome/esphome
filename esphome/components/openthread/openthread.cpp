#include "esphome/core/defines.h"
#ifdef USE_OPENTHREAD
#include "openthread.h"
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)
#include "esp_openthread.h"
#endif

#include <freertos/portmacro.h>

#include <openthread/cli.h>
#include <openthread/instance.h>
#include <openthread/logging.h>
#include <openthread/netdata.h>
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

void OpenThreadComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Open Thread:");
#if CONFIG_OPENTHREAD_FTD
  ESP_LOGCONFIG(TAG, "  Device Type: FTD");
#elif CONFIG_OPENTHREAD_MTD
  ESP_LOGCONFIG(TAG, "  Device Type: MTD");
  // TBD: Synchronized Sleepy End Device
  if (this->poll_period > 0) {
    ESP_LOGCONFIG(TAG, "  Device is configured as Sleepy End Device (SED)");
    uint32_t duration = this->poll_period / 1000;
    ESP_LOGCONFIG(TAG, "  Poll Period: %" PRIu32 "s", duration);
  } else {
    ESP_LOGCONFIG(TAG, "  Device is configured as Minimal End Device (MED)");
  }
#endif
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

void OpenThreadComponent::defer_factory_reset_external_callback() {
  ESP_LOGD(TAG, "Defer factory_reset_external_callback_");
  this->defer([this]() { this->factory_reset_external_callback_(); });
}

void OpenThreadSrpComponent::srp_callback(otError err, const otSrpClientHostInfo *host_info,
                                          const otSrpClientService *services,
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

void OpenThreadSrpComponent::srp_start_callback(const otSockAddr *server_socket_address, void *context) {
  ESP_LOGI(TAG, "SRP client has started");
}

void OpenThreadSrpComponent::srp_factory_reset_callback(otError err, const otSrpClientHostInfo *host_info,
                                                        const otSrpClientService *services,
                                                        const otSrpClientService *removed_services, void *context) {
  OpenThreadComponent *obj = (OpenThreadComponent *) context;
  if (err == OT_ERROR_NONE && removed_services != NULL && host_info != NULL &&
      host_info->mState == OT_SRP_CLIENT_ITEM_STATE_REMOVED) {
    ESP_LOGD(TAG, "Successful Removal SRP Host and Services");
  } else if (err != OT_ERROR_NONE) {
    // Handle other SRP client events or errors
    ESP_LOGW(TAG, "SRP client event/error: %s", otThreadErrorToString(err));
  }
  obj->defer_factory_reset_external_callback();
}

void OpenThreadSrpComponent::setup() {
  otError error;
  InstanceLock lock = InstanceLock::acquire();
  otInstance *instance = lock.get_instance();

  otSrpClientSetCallback(instance, OpenThreadSrpComponent::srp_callback, nullptr);

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

  // Get mdns services and copy their data (strings are copied with strdup below)
  const auto &mdns_services = this->mdns_->get_services();
  ESP_LOGD(TAG, "Setting up SRP services. count = %d\n", mdns_services.size());
  for (const auto &service : mdns_services) {
    otSrpClientBuffersServiceEntry *entry = otSrpClientBuffersAllocateService(instance);
    if (!entry) {
      ESP_LOGW(TAG, "Failed to allocate service entry");
      continue;
    }

    // Set service name
    char *string = otSrpClientBuffersGetServiceEntryServiceNameString(entry, &size);
    std::string full_service = std::string(MDNS_STR_ARG(service.service_type)) + "." + MDNS_STR_ARG(service.proto);
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
      // Value is either a compile-time string literal in flash or a pointer to dynamic_txt_values_
      // OpenThread SRP client expects the data to persist, so we strdup it
      const char *value_str = MDNS_STR_ARG(txt.value);
      txt_entries[i].mKey = MDNS_STR_ARG(txt.key);
      txt_entries[i].mValue = reinterpret_cast<const uint8_t *>(strdup(value_str));
      txt_entries[i].mValueLength = strlen(value_str);
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

  otSrpClientEnableAutoStartMode(instance, OpenThreadSrpComponent::srp_start_callback, nullptr);
  ESP_LOGD(TAG, "Finished SRP setup");
}

void *OpenThreadSrpComponent::pool_alloc_(size_t size) {
  uint8_t *ptr = new uint8_t[size];
  this->memory_pool_.emplace_back(std::unique_ptr<uint8_t[]>(ptr));
  return ptr;
}

void OpenThreadSrpComponent::set_mdns(esphome::mdns::MDNSComponent *mdns) { this->mdns_ = mdns; }

bool OpenThreadComponent::teardown() {
  if (!this->teardown_started_) {
    this->teardown_started_ = true;
    ESP_LOGD(TAG, "Clear Srp");
    auto lock = InstanceLock::try_acquire(100);
    if (!lock) {
      ESP_LOGW(TAG, "Failed to acquire OpenThread lock during teardown, leaking memory");
      return true;
    }
    otInstance *instance = lock->get_instance();
    otSrpClientClearHostAndServices(instance);
    otSrpClientBuffersFreeAllServices(instance);
    global_openthread_component = nullptr;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)
    ESP_LOGD(TAG, "Exit main loop ");
    int error = esp_openthread_mainloop_exit();
    if (error != ESP_OK) {
      ESP_LOGW(TAG, "Failed attempt to stop main loop %d", error);
      this->teardown_complete_ = true;
    }
#else
    this->teardown_complete_ = true;
#endif
  }
  return this->teardown_complete_;
}

void OpenThreadComponent::on_factory_reset(std::function<void()> callback) {
  factory_reset_external_callback_ = callback;
  ESP_LOGD(TAG, "Start Removal SRP Host and Services");
  otError error;
  InstanceLock lock = InstanceLock::acquire();
  otInstance *instance = lock.get_instance();
  otSrpClientSetCallback(instance, OpenThreadSrpComponent::srp_factory_reset_callback, this);
  error = otSrpClientRemoveHostAndServices(instance, true, true);
  if (error != OT_ERROR_NONE) {
    ESP_LOGW(TAG, "Failed to Remove SRP Host and Services");
    return;
  }
  ESP_LOGD(TAG, "Waiting on Confirmation Removal SRP Host and Services");
}

// set_use_address() is guaranteed to be called during component setup by Python code generation,
// so use_address_ will always be valid when get_use_address() is called - no fallback needed.
const char *OpenThreadComponent::get_use_address() const { return this->use_address_; }

void OpenThreadComponent::set_use_address(const char *use_address) { this->use_address_ = use_address; }

}  // namespace openthread
}  // namespace esphome

#endif
