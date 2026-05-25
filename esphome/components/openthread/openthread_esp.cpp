#include "esphome/core/defines.h"
#if defined(USE_OPENTHREAD) && defined(USE_ESP32)
#include <openthread/logging.h>
#include "openthread.h"

#include "esp_log.h"
#include "esp_openthread.h"
#include "esp_openthread_lock.h"

#include "esp_task_wdt.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include "esp_err.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_netif_types.h"
#include "esp_openthread_cli.h"
#include "esp_openthread_netif_glue.h"
#include "esp_vfs_eventfd.h"
#include "nvs_flash.h"

// need to add espressif/esp_ot_cli_extension component registry
#if CONFIG_OPENTHREAD_CLI_ESP_EXTENSION
#include "esp_ot_cli_extension.h"
#endif

static const char *const TAG = "openthread";

namespace esphome::openthread {

void OpenThreadComponent::setup() {
  // Used eventfds:
  // * netif
  // * ot task queue
  // * radio driver
  esp_vfs_eventfd_config_t eventfd_config = {
      .max_fds = 3,
  };
  // Network interface setup handled by network component
  ESP_ERROR_CHECK(nvs_flash_init());
  ESP_ERROR_CHECK(esp_vfs_eventfd_register(&eventfd_config));

#if CONFIG_OPENTHREAD_CLI
  ot_console_start();
  ot_register_external_commands();
#endif

  esp_openthread_config_t config = {.netif_config = ESP_NETIF_DEFAULT_OPENTHREAD(),
                                    .platform_config = {
                                        .radio_config =
                                            {
                                                .radio_mode = RADIO_MODE_NATIVE,
                                                .radio_uart_config = {},
                                            },
                                        .host_config =
                                            {
                                                // There is a conflict between esphome's logger which also
                                                // claims the usb serial jtag device.
                                                // .host_connection_mode = HOST_CONNECTION_MODE_CLI_USB,
                                                // .host_usb_config = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT(),
                                            },
                                        .port_config =
                                            {
                                                .storage_partition_name = "nvs",
                                                .netif_queue_size = 10,
                                                .task_queue_size = 10,
                                            },
                                    }};

  ESP_ERROR_CHECK(esp_openthread_start(&config));
  // Mark lock as initialized so InstanceLock callers know it's safe to acquire.
  // Must be set after esp_openthread_init() which creates the internal semaphore.
  this->lock_initialized_ = true;
  // Fetch OT instance once to avoid repeated call into OT stack
  otInstance *instance = esp_openthread_get_instance();

#if CONFIG_OPENTHREAD_CLI_ESP_EXTENSION
  esp_cli_custom_command_init();
#endif
#if CONFIG_OPENTHREAD_STATE_INDICATOR_ENABLE
  ESP_ERROR_CHECK(esp_openthread_state_indicator_init(instance));
#endif

  // lock
  esp_openthread_lock_acquire(portMAX_DELAY);
  this->apply_linkmode(instance);

  if (this->output_power_.has_value()) {
    if (const auto err = otPlatRadioSetTransmitPower(instance, *this->output_power_); err != OT_ERROR_NONE) {
      ESP_LOGE(TAG, "Failed to set power: %s", otThreadErrorToString(err));
    }
  }
  ESP_LOGI(TAG, "Activating dataset...");
  otOperationalDatasetTlvs dataset = {};

#ifndef USE_OPENTHREAD_FORCE_DATASET
  // Check if openthread has a valid dataset from a previous execution
  otError error = otDatasetGetActiveTlvs(instance, &dataset);
  if (error != OT_ERROR_NONE) {
    // Make sure the length is 0 so we fallback to the configuration
    dataset.mLength = 0;
  } else {
    ESP_LOGI(TAG, "Found existing dataset, ignoring config (force_dataset: true to override)");
  }
#endif

#ifdef USE_OPENTHREAD_TLVS
  if (dataset.mLength == 0) {
    // If we didn't have an active dataset, and we have tlvs, parse it and pass it to esp_openthread_auto_start
    size_t len = (sizeof(USE_OPENTHREAD_TLVS) - 1) / 2;
    if (len > sizeof(dataset.mTlvs)) {
      ESP_LOGW(TAG, "TLV buffer too small, truncating");
      len = sizeof(dataset.mTlvs);
    }
    parse_hex(USE_OPENTHREAD_TLVS, sizeof(USE_OPENTHREAD_TLVS) - 1, dataset.mTlvs, len);
    dataset.mLength = len;
  }
#endif

  // Pass the existing dataset, or NULL which will use the preprocessor definitions
  ESP_ERROR_CHECK(esp_openthread_auto_start(dataset.mLength > 0 ? &dataset : nullptr));

  // Register state change callback to update connected_ reactively instead of polling
  otSetStateChangedCallback(instance, OpenThreadComponent::on_state_changed_, this);

  esp_openthread_lock_release();

  ESP_LOGD(TAG, "Thread Version: %" PRIu16, otThreadGetVersion());
}

int OpenThreadComponent::openthread_stop_() {
  // Clean up - reset lock flag before deinit destroys the semaphore
  this->lock_initialized_ = false;
  int error = esp_openthread_stop();
  this->teardown_stage = OtcTeardownStage::OTC_TEARDOWN_COMPLETED;
  return error;
}

network::IPAddresses OpenThreadComponent::get_ip_addresses() {
  network::IPAddresses addresses;
  struct esp_ip6_addr if_ip6s[CONFIG_LWIP_IPV6_NUM_ADDRESSES];
  uint8_t count = 0;
  esp_netif_t *netif = esp_netif_get_default_netif();
  count = esp_netif_get_all_ip6(netif, if_ip6s);
  assert(count <= CONFIG_LWIP_IPV6_NUM_ADDRESSES);
  assert(count < addresses.size());
  for (int i = 0; i < count; i++) {
    addresses[i + 1] = network::IPAddress(&if_ip6s[i]);
  }
  return addresses;
}

// not thread safe, only use in read-only use cases
otInstance *OpenThreadComponent::get_openthread_instance_() { return esp_openthread_get_instance(); }

std::optional<InstanceLock> InstanceLock::try_acquire(int delay) {
  if (!global_openthread_component->is_lock_initialized()) {
    return {};
  }
  if (esp_openthread_lock_acquire(delay)) {
    return InstanceLock();
  }
  return {};
}

InstanceLock InstanceLock::acquire() {
  // Wait for the lock to be created by ot_main() before attempting to acquire it.
  // esp_openthread_lock_acquire() will assert-crash if called before esp_openthread_init().
  constexpr uint32_t lock_init_timeout_ms = 10000;
  uint32_t start = millis();
  while (!global_openthread_component->is_lock_initialized()) {
    if (millis() - start > lock_init_timeout_ms) {
      ESP_LOGE(TAG, "OpenThread lock not initialized after %" PRIu32 "ms, aborting", lock_init_timeout_ms);
      abort();
    }
    delay(10);
    esp_task_wdt_reset();
  }
  while (!esp_openthread_lock_acquire(100)) {
    esp_task_wdt_reset();
  }
  return InstanceLock();
}

otInstance *InstanceLock::get_instance() { return esp_openthread_get_instance(); }

InstanceLock::~InstanceLock() { esp_openthread_lock_release(); }

}  // namespace esphome::openthread
#endif
