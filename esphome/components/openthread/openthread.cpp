#include "esphome/core/defines.h"
#ifdef USE_OPENTHREAD
#include "openthread.h"

#include <openthread/child_supervision.h>
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

namespace esphome::openthread {

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
  if (this->poll_period_ > 0) {
    ESP_LOGCONFIG(TAG, "  Device is configured as Sleepy End Device (SED)");
    uint32_t duration = this->poll_period_ / 1000;
    ESP_LOGCONFIG(TAG, "  Poll Period: %" PRIu32 "s", duration);
  } else {
    ESP_LOGCONFIG(TAG, "  Device is configured as Minimal End Device (MED)");
  }
#endif
  if (this->output_power_.has_value()) {
    ESP_LOGCONFIG(TAG, "  Output power: %" PRId8 "dBm", *this->output_power_);
  }
}

void OpenThreadComponent::on_state_changed_(otChangedFlags flags, void *context) {
  if (flags & OT_CHANGED_THREAD_ROLE) {
    auto *self = static_cast<OpenThreadComponent *>(context);
    // This runs on the OpenThread task thread with the OT lock held,
    // so we can safely call otThreadGetDeviceRole directly.
    otInstance *instance = self->get_openthread_instance_();
    otDeviceRole role = otThreadGetDeviceRole(instance);
    self->connected_ = role >= OT_DEVICE_ROLE_CHILD;
    self->publish_state(role);
  }
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
  const auto &host_name = App.get_name();
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
    entry->mService.mPort = service.port.value();

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
  switch (this->teardown_stage_) {
    case OtcTeardownStage::OTC_TEARDOWN_NOT_STARTED: {
      // start tearing down
      this->teardown_stage_ = OtcTeardownStage::OTC_TEARDOWN_STARTED;
      ESP_LOGV(TAG, "Clear Srp");
      auto lock = InstanceLock::try_acquire(100);
      if (!lock) {
        ESP_LOGW(TAG, "Failed to acquire OpenThread lock during teardown, leaking memory");
        return true;
      }
      otInstance *instance = lock->get_instance();
      otSrpClientClearHostAndServices(instance);
      otSrpClientBuffersFreeAllServices(instance);
#ifdef USE_OPENTHREAD_GRACEFUL_DETACH_ON_SHUTDOWN
      otThreadDetachGracefully(instance, OpenThreadComponent::detach_callback, this);
#else
      // skip graceful detach, parent will not remove child from its child table
      this->teardown_stage_ = OtcTeardownStage::OTC_TEARDOWN_DETACH_COMPLETED;
#endif
    } break;
    case OtcTeardownStage::OTC_TEARDOWN_STARTED:
      // waiting on callback
      break;
    case OtcTeardownStage::OTC_TEARDOWN_DETACH_COMPLETED: {
      this->teardown_stage_ = OtcTeardownStage::OTC_TEARDOWN_STOP_STARTED;
      auto lock = InstanceLock::try_acquire(100);
      if (!lock) {
        ESP_LOGW(TAG, "Failed to acquire OpenThread lock during teardown, leaking memory");
        return true;
      }
      otInstance *instance = lock->get_instance();
      otThreadSetEnabled(instance, false);
      otIp6SetEnabled(instance, false);
    } break;
    case OtcTeardownStage::OTC_TEARDOWN_STOP_STARTED: {
      // stop openthread
      this->teardown_stage_ = OtcTeardownStage::OTC_TEARDOWN_STOP_IN_PROCESS;
      global_openthread_component = nullptr;
      ESP_LOGV(TAG, "Stop Openthread");
      int error = this->openthread_stop_();
      if (error != 0) {
        ESP_LOGW(TAG, "Failed attempt to stop openthread %d", error);
        return true;
      }
    } break;
    case OtcTeardownStage::OTC_TEARDOWN_STOP_IN_PROCESS:
      // waiting on openthread stop
      break;
    case OtcTeardownStage::OTC_TEARDOWN_COMPLETED:
      ESP_LOGV(TAG, "OpenthreadComponent Teardown Complete");
      return true;
  }
  return false;
}

#ifdef USE_OPENTHREAD_GRACEFUL_DETACH_ON_SHUTDOWN
void OpenThreadComponent::detach_callback(void *context) {
  OpenThreadComponent *obj = (OpenThreadComponent *) context;
  obj->teardown_stage_ = OtcTeardownStage::OTC_TEARDOWN_DETACH_COMPLETED;
}
#endif

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

void OpenThreadComponent::apply_linkmode(otInstance *instance) {
  otLinkModeConfig link_mode_config{};
#if CONFIG_OPENTHREAD_FTD
  link_mode_config.mRxOnWhenIdle = true;
  link_mode_config.mDeviceType = true;
  link_mode_config.mNetworkData = true;
#elif CONFIG_OPENTHREAD_MTD
  if (this->poll_period_ > 0) {
    if (otLinkSetPollPeriod(instance, this->poll_period_) != OT_ERROR_NONE) {
      ESP_LOGE(TAG, "Failed to set pollperiod");
    }
    ESP_LOGD(TAG, "Link Polling Period: %" PRIu32, otLinkGetPollPeriod(instance));
  }

  uint16_t poll_period_sec = (this->poll_period_ + 500) / 1000;
  // Minimums match OpenThread defaults: src/core/config/mle.h OPENTHREAD_CONFIG_MLE_CHILD_TIMEOUT_DEFAULT
  otThreadSetChildTimeout(instance, std::max(poll_period_sec * 4, 240));
  // Minimums match OpenThread defaults: src/core/config/child_supervision.h
  // OPENTHREAD_CONFIG_CHILD_SUPERVISION_CHECK_TIMEOUT
  otChildSupervisionSetCheckTimeout(instance, std::max(poll_period_sec * 2, 190));
  // Minimums match OpenThread defaults: src/core/config/child_supervision.h
  // OPENTHREAD_CONFIG_CHILD_SUPERVISION_INTERVAL
  otChildSupervisionSetInterval(instance, std::max((uint16_t) (poll_period_sec * 3 / 2), (uint16_t) 129));
  ESP_LOGD(TAG, "Child Timeout: %d sec, Child Supervision Check Timeout: %d sec, Child Supervision Interval: %d sec",
           otThreadGetChildTimeout(instance), otChildSupervisionGetCheckTimeout(instance),
           otChildSupervisionGetInterval(instance));

  link_mode_config.mRxOnWhenIdle = this->poll_period_ == 0;
  link_mode_config.mDeviceType = false;
  link_mode_config.mNetworkData = false;
#endif

  if (otThreadSetLinkMode(instance, link_mode_config) != OT_ERROR_NONE) {
    ESP_LOGE(TAG, "Failed to set linkmode");
  }
#ifdef ESPHOME_LOG_HAS_DEBUG  // Fetch link mode from OT only when DEBUG
  link_mode_config = otThreadGetLinkMode(instance);
  ESP_LOGD(TAG, "Link Mode Device Type: %s, Network Data: %s, RX On When Idle: %s",
           TRUEFALSE(link_mode_config.mDeviceType), TRUEFALSE(link_mode_config.mNetworkData),
           TRUEFALSE(link_mode_config.mRxOnWhenIdle));
#endif
}

void OpenThreadComponent::publish_state(otDeviceRole role) {
  ESP_LOGD(TAG, "Publish State: %d", role);
  this->state_callbacks_.call(role);
  this->full_state_callbacks_.call(this->active_role_, role);
  this->active_role_ = role;
}

}  // namespace esphome::openthread
#endif
