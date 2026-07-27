#include "esphome/core/defines.h"
#if defined(USE_OPENTHREAD) && defined(USE_ZEPHYR) && !defined(USE_ZEPHYR_VARIANT_NATIVE_SIM)
#include <openthread/dataset.h>
#include <openthread/thread.h>
#include <openthread/logging.h>
#include <openthread/platform/radio.h>
#include "openthread.h"
#include "esphome/core/helpers.h"
#include <zephyr/net/openthread.h>

static const char *const TAG = "openthread";

namespace esphome::openthread {

// TODO: nrf52's bundled NCS Zephyr fork (framework-sdk-nrf, currently pinned v2.6.1-b) predates
// the context-free OpenThread L2 API mainline Zephyr (used by esp32-h2/c6) has had since v4.4.
// This #ifdef USE_NRF52 split is a temporary bridge until nrf52 upgrades past that fork (blocked
// on nrf_zigbee's own SDK pin moving first) -- rip it out once both platforms share one API.
#ifdef USE_NRF52
static void on_thread_state_changed(otChangedFlags flags, struct openthread_context *ot_context, void *user_data) {
  // Delegate connection status tracking to common callback
  if (global_openthread_component != nullptr) {
    OpenThreadComponent::on_state_changed(flags, global_openthread_component);
  }
  otInstance *instance = ot_context->instance;
#else
static void on_thread_state_changed(otChangedFlags flags, void *user_data) {
  // Delegate connection status tracking to common callback
  if (global_openthread_component != nullptr) {
    OpenThreadComponent::on_state_changed(flags, global_openthread_component);
  }
  otInstance *instance = openthread_get_default_instance();
#endif
  if (flags & OT_CHANGED_THREAD_ROLE) {
    otDeviceRole role = otThreadGetDeviceRole(instance);
    ESP_LOGI(TAG, "Thread role changed to %s", otThreadDeviceRoleToString(role));
  }
  if (flags & OT_CHANGED_THREAD_NETDATA) {
    ESP_LOGI(TAG, "Thread network data updated");
  }
  if (flags & (OT_CHANGED_THREAD_ROLE | OT_CHANGED_THREAD_NETDATA)) {
    char buf[NET_IPV6_ADDR_LEN];
    for (const otNetifAddress *addr = otIp6GetUnicastAddresses(instance); addr != nullptr; addr = addr->mNext) {
      ESP_LOGI(TAG, "  Address: %s", net_addr_ntop(AF_INET6, &addr->mAddress, buf, sizeof(buf)));
    }
  }
}

#ifdef USE_NRF52
static struct openthread_state_changed_cb ot_state_changed_cb = {.state_changed_cb = on_thread_state_changed};
#else
static struct openthread_state_changed_callback ot_state_changed_cb = {.otCallback = on_thread_state_changed};
#endif

void OpenThreadComponent::setup() {
#ifdef USE_NRF52
  struct openthread_context *ot_context = openthread_get_default_context();
  otInstance *instance = ot_context->instance;
#else
  otInstance *instance = openthread_get_default_instance();
#endif
  this->lock_initialized_ = true;
  otOperationalDatasetTlvs dataset = {};

#ifndef USE_OPENTHREAD_FORCE_DATASET
  otError error = otDatasetGetActiveTlvs(instance, &dataset);
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
    otError error = otDatasetSetActiveTlvs(instance, &dataset);
    if (error != OT_ERROR_NONE) {
      ESP_LOGE(TAG, "Failed to set active dataset: %s", otThreadErrorToString(error));
      this->mark_failed();
      return;
    }
  }
  // Without this, link mode stayed at OT's default instead of this component's config,
  // and output_power was never sent to the radio at all.
  this->apply_linkmode_(instance);

  if (this->output_power_.has_value()) {
    if (const auto err = otPlatRadioSetTransmitPower(instance, *this->output_power_); err != OT_ERROR_NONE) {
      ESP_LOGE(TAG, "Failed to set power: %s", otThreadErrorToString(err));
    }
  }

#ifdef USE_NRF52
  openthread_state_changed_cb_register(ot_context, &ot_state_changed_cb);
  openthread_start(ot_context);
#else
  openthread_state_changed_callback_register(&ot_state_changed_cb);
  openthread_run();
#endif
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
#ifdef USE_NRF52
  struct openthread_context *ot_context = openthread_get_default_context();
  if (k_mutex_lock(&ot_context->api_lock, K_MSEC(delay)) == 0) {
    return InstanceLock(true);
  }
  return InstanceLock(false);
#else
  // No timed-wait variant of the real lock is exposed publicly, so poll it for `delay` ms
  // instead -- fine given how infrequently/briefly this lock is actually contended.
  int64_t deadline = k_uptime_get() + delay;
  do {
    if (openthread_mutex_try_lock() == 0) {
      return InstanceLock(true);
    }
    k_msleep(1);
  } while (k_uptime_get() < deadline);
  return InstanceLock(false);
#endif
}

InstanceLock InstanceLock::acquire() {
#ifdef USE_NRF52
  struct openthread_context *ot_context = openthread_get_default_context();
  k_mutex_lock(&ot_context->api_lock, K_FOREVER);
#else
  openthread_mutex_lock();
#endif
  return InstanceLock(true);
}

otInstance *InstanceLock::get_instance() { return openthread_get_default_instance(); }

InstanceLock::~InstanceLock() {
  if (this->owns_) {
#ifdef USE_NRF52
    struct openthread_context *ot_context = openthread_get_default_context();
    k_mutex_unlock(&ot_context->api_lock);
#else
    openthread_mutex_unlock();
#endif
  }
}

}  // namespace esphome::openthread
#endif
