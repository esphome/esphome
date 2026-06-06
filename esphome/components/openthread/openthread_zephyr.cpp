#include "esphome/core/defines.h"
#if defined(USE_OPENTHREAD) && defined(USE_NRF52)
#include <openthread/dataset.h>
#include <openthread/thread.h>
#include <openthread/logging.h>
#include "openthread.h"
#include "esphome/core/helpers.h"
#include <zephyr/net/openthread.h>

static const char *const TAG = "openthread";

namespace esphome::openthread {

static void on_thread_state_changed(otChangedFlags flags, struct openthread_context *ot_context, void *user_data) {
  if (flags & OT_CHANGED_THREAD_ROLE) {
    otDeviceRole role = otThreadGetDeviceRole(ot_context->instance);
    if (global_openthread_component != nullptr) {
      global_openthread_component->set_connected(role >= OT_DEVICE_ROLE_CHILD);
    }
    ESP_LOGI(TAG, "Thread role changed to %s", otThreadDeviceRoleToString(role));
  }
  if (flags & OT_CHANGED_THREAD_NETDATA) {
    ESP_LOGI(TAG, "Thread network data updated");
  }
  if (flags & (OT_CHANGED_THREAD_ROLE | OT_CHANGED_THREAD_NETDATA)) {
    char buf[NET_IPV6_ADDR_LEN];
    for (const otNetifAddress *addr = otIp6GetUnicastAddresses(ot_context->instance); addr != nullptr;
         addr = addr->mNext) {
      ESP_LOGI(TAG, "  Address: %s", net_addr_ntop(AF_INET6, &addr->mAddress, buf, sizeof(buf)));
    }
  }
}

static struct openthread_state_changed_cb ot_state_changed_cb = {.state_changed_cb = on_thread_state_changed};

void OpenThreadComponent::setup() {
  struct openthread_context *context = openthread_get_default_context();
  this->lock_initialized_ = true;
  otOperationalDatasetTlvs dataset = {};

#ifndef USE_OPENTHREAD_FORCE_DATASET
  otError error = otDatasetGetActiveTlvs(context->instance, &dataset);
  if (error != OT_ERROR_NONE) {
    dataset.mLength = 0;
  } else {
    ESP_LOGI(TAG, "Found existing dataset, ignoring config (force_dataset: true to override)");
  }
#endif

#ifdef USE_OPENTHREAD_TLVS
  if (dataset.mLength == 0) {
    size_t len = (sizeof(USE_OPENTHREAD_TLVS) - 1) / 2;
    if (len > sizeof(dataset.mTlvs)) {
      ESP_LOGW(TAG, "TLV buffer too small, truncating");
      len = sizeof(dataset.mTlvs);
    }
    size_t parsed = parse_hex(USE_OPENTHREAD_TLVS, sizeof(USE_OPENTHREAD_TLVS) - 1, dataset.mTlvs, len);
    if (parsed != 2 * len) {
      ESP_LOGE(TAG, "Invalid OpenThread TLV hex string (expected %zu hex chars, got %zu)", 2 * len, parsed);
      return;
    }
    dataset.mLength = len;
  }
#endif
  if (dataset.mLength > 0) {
    otError error = otDatasetSetActiveTlvs(context->instance, &dataset);
    if (error != OT_ERROR_NONE) {
      ESP_LOGE(TAG, "Failed to set active dataset: %s", otThreadErrorToString(error));
      return;
    }
  }
  openthread_state_changed_cb_register(context, &ot_state_changed_cb);
  openthread_start(context);
}

void OpenThreadComponent::ot_main() {}

network::IPAddresses OpenThreadComponent::get_ip_addresses() {
  network::IPAddresses addresses;
  size_t addr_count = 0;
  for (const otNetifAddress *addr = otIp6GetUnicastAddresses(openthread_get_default_instance());
       addr != nullptr && addr_count + 1 < addresses.size(); addr = addr->mNext) {
    addresses[addr_count + 1] = network::IPAddress(reinterpret_cast<const struct in6_addr *>(&addr->mAddress));
    addr_count++;
  }
  return addresses;
}

std::optional<InstanceLock> InstanceLock::try_acquire(int delay) {
  if (global_openthread_component == nullptr || !global_openthread_component->is_lock_initialized()) {
    return {};
  }
  struct openthread_context *ot_context = openthread_get_default_context();
  if (k_mutex_lock(&ot_context->api_lock, K_MSEC(delay)) == 0) {
    return InstanceLock();
  }
  return {};
}

InstanceLock InstanceLock::acquire() {
  if (global_openthread_component == nullptr || !global_openthread_component->is_lock_initialized()) {
    ESP_LOGE(TAG, "OpenThread lock not initialized, aborting");
    abort();
  }
  struct openthread_context *ot_context = openthread_get_default_context();
  while (k_mutex_lock(&ot_context->api_lock, K_MSEC(100)) != 0) {
  }
  return InstanceLock();
}

otInstance *InstanceLock::get_instance() { return openthread_get_default_instance(); }

InstanceLock::~InstanceLock() {
  struct openthread_context *ot_context = openthread_get_default_context();
  k_mutex_unlock(&ot_context->api_lock);
}

}  // namespace esphome::openthread
#endif
