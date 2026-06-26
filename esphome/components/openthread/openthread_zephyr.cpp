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
  // Delegate connection status tracking to common callback
  if (global_openthread_component != nullptr) {
    OpenThreadComponent::on_state_changed(flags, global_openthread_component);
  }
  if (flags & OT_CHANGED_THREAD_ROLE) {
    otDeviceRole role = otThreadGetDeviceRole(ot_context->instance);
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
    const size_t tlv_chars = sizeof(USE_OPENTHREAD_TLVS) - 1;
    if ((tlv_chars % 2) != 0) {
      ESP_LOGE(TAG, "Invalid OpenThread TLV hex string length (must be even, got %zu)", tlv_chars);
      this->mark_failed();
      return;
    }

    size_t len = tlv_chars / 2;
    if (len > sizeof(dataset.mTlvs)) {
      ESP_LOGE(TAG, "OpenThread TLV too long (max %zu bytes, got %zu bytes)", sizeof(dataset.mTlvs), len);
      this->mark_failed();
      return;
    }

    size_t parsed = parse_hex(USE_OPENTHREAD_TLVS, tlv_chars, dataset.mTlvs, len);
    if (parsed != tlv_chars) {
      ESP_LOGE(TAG, "Invalid OpenThread TLV hex string (expected %zu hex chars, got %zu)", tlv_chars, parsed);
      this->mark_failed();
      return;
    }
    dataset.mLength = len;
  }
#endif
  if (dataset.mLength > 0) {
    otError error = otDatasetSetActiveTlvs(context->instance, &dataset);
    if (error != OT_ERROR_NONE) {
      ESP_LOGE(TAG, "Failed to set active dataset: %s", otThreadErrorToString(error));
      this->mark_failed();
      return;
    }
  }
  openthread_state_changed_cb_register(context, &ot_state_changed_cb);
  openthread_start(context);
}

void OpenThreadComponent::ot_main() {}

otInstance *OpenThreadComponent::get_openthread_instance_() { return openthread_get_default_instance(); }

int OpenThreadComponent::openthread_stop_() {
  // OT stack is intentionally left running — no Zephyr stop API. The state callback stays
  // registered but is safe (null-checks global_openthread_component). nRF52840 never
  // re-enters setup() after teardown so this is functionally correct.
  this->teardown_complete_ = true;
  return 0;
}

network::IPAddresses OpenThreadComponent::get_ip_addresses() {
  network::IPAddresses addresses;
  auto lock = InstanceLock::acquire();
  size_t addr_count = 0;
  for (const otNetifAddress *addr = otIp6GetUnicastAddresses(openthread_get_default_instance());
       addr != nullptr && addr_count + 1 < addresses.size(); addr = addr->mNext) {
    struct in6_addr ip6;
    memcpy(&ip6, addr->mAddress.mFields.m8, sizeof(ip6));
    addresses[addr_count + 1] = network::IPAddress(&ip6);
    addr_count++;
  }
  return addresses;
}

InstanceLock InstanceLock::try_acquire(int delay) {
  if (global_openthread_component == nullptr || !global_openthread_component->is_lock_initialized()) {
    return InstanceLock(false);
  }
  struct openthread_context *ot_context = openthread_get_default_context();
  if (k_mutex_lock(&ot_context->api_lock, K_MSEC(delay)) == 0) {
    return InstanceLock(true);
  }
  return InstanceLock(false);
}

InstanceLock InstanceLock::acquire() {
  struct openthread_context *ot_context = openthread_get_default_context();
  k_mutex_lock(&ot_context->api_lock, K_FOREVER);
  return InstanceLock(true);
}

otInstance *InstanceLock::get_instance() { return openthread_get_default_instance(); }

InstanceLock::~InstanceLock() {
  if (this->owns_) {
    struct openthread_context *ot_context = openthread_get_default_context();
    k_mutex_unlock(&ot_context->api_lock);
  }
}

}  // namespace esphome::openthread
#endif
