#include "esphome/core/defines.h"
#if defined(USE_OPENTHREAD) && defined(USE_ESP32)
#include <openthread/dataset.h>
#include "openthread.h"

#ifdef USE_OPENTHREAD_BORDER_ROUTER
#include "esp_openthread_border_router.h"
#include "esphome/components/wifi/wifi_component.h"
#ifndef USE_OPENTHREAD_RCP_UART
#include "esp_coexist.h"
#endif
#endif
#ifdef USE_OPENTHREAD_RCP_UART
#include "esp_openthread_spinel.h"
#endif
#include "esp_log.h"
#include "esp_openthread.h"
#include "esp_openthread_lock.h"
#include "esp_openthread_types.h"
#include "esp_task_wdt.h"
#include "esphome/components/network/util.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include "esp_err.h"
#include "esp_netif.h"
#include "esp_openthread_netif_glue.h"
#include "esp_vfs_eventfd.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

ESPHOME_LOG_TAG(TAG, "openthread");

namespace esphome::openthread {

void OpenThreadComponent::setup() {
#ifdef USE_OPENTHREAD_BORDER_ROUTER
  esp_netif_t *backbone_netif = wifi::global_wifi_component->get_esp_netif_sta();
  if (backbone_netif == nullptr) {
    ESP_LOGE(TAG, "Wi-Fi STA backbone netif is not initialized");
    this->teardown_stage_ = TeardownStage::TEARDOWN_STAGE_COMPLETED;
    this->mark_failed();
    return;
  }
  esp_openthread_set_backbone_netif(backbone_netif);
#endif

  // Used eventfds: netif, ot task queue, radio driver, and one more for a border router on the
  // native radio (the UART Spinel transport has none)
#if defined(USE_OPENTHREAD_BORDER_ROUTER) && !defined(USE_OPENTHREAD_RCP_UART)
  constexpr size_t max_eventfds = 4;
#else
  constexpr size_t max_eventfds = 3;
#endif
  esp_vfs_eventfd_config_t eventfd_config = {
      .max_fds = max_eventfds,
  };
  // Network interface setup handled by network component
  ESP_ERROR_CHECK(nvs_flash_init());
  ESP_ERROR_CHECK(esp_vfs_eventfd_register(&eventfd_config));

  // Own task instead of esp_openthread_start(): the dataset activation has to run on the task
  // that holds the OpenThread task-switching lock, and a border router must not become the
  // default netif
  if (xTaskCreate(
          [](void *arg) {
            static_cast<OpenThreadComponent *>(arg)->ot_main_();
            vTaskDelete(nullptr);
          },
          CONFIG_OPENTHREAD_TASK_NAME, CONFIG_OPENTHREAD_TASK_SIZE, this, CONFIG_OPENTHREAD_TASK_PRIORITY,
          nullptr) != pdPASS) {
    ESP_LOGE(TAG, "Failed to create OpenThread task");
    esp_vfs_eventfd_unregister();
    this->teardown_stage_ = TeardownStage::TEARDOWN_STAGE_COMPLETED;
    this->mark_failed();
  }
}

void OpenThreadComponent::ot_main_() {
  esp_openthread_platform_config_t config = {
      .radio_config =
          {
#ifdef USE_OPENTHREAD_RCP_UART
              .radio_mode = RADIO_MODE_UART_RCP,
              .radio_uart_config =
                  {
                      .port = UART_NUM_1,
                      .uart_config =
                          {
                              .baud_rate = static_cast<int>(this->rcp_baud_rate_),
                              .data_bits = UART_DATA_8_BITS,
                              .parity = UART_PARITY_DISABLE,
                              .stop_bits = UART_STOP_BITS_1,
                              .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
                              .rx_flow_ctrl_thresh = 0,
                              .source_clk = UART_SCLK_DEFAULT,
                          },
                      .rx_pin = static_cast<gpio_num_t>(this->rcp_rx_pin_),
                      .tx_pin = static_cast<gpio_num_t>(this->rcp_tx_pin_),
                  },
#else
              .radio_mode = RADIO_MODE_NATIVE,
              .radio_uart_config = {},
#endif
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
  };

#ifdef USE_OPENTHREAD_RCP_UART
  // Without a reset pin there is nothing to do on a failure beyond the Spinel driver's own recovery
  if (this->rcp_reset_pin_ != nullptr) {
    this->rcp_reset_pin_->setup();
    esp_openthread_register_rcp_failure_handler(OpenThreadComponent::rcp_failure_handler);
    esp_openthread_set_coprocessor_reset_failure_callback(OpenThreadComponent::rcp_failure_handler);
    this->reset_rcp_(false);
  }
#endif

  ESP_ERROR_CHECK(esp_openthread_init(&config));

  // Initialize the esp_netif bindings
  esp_netif_config_t netif_config = ESP_NETIF_DEFAULT_OPENTHREAD();
  esp_netif_t *netif = esp_netif_new(&netif_config);
  assert(netif != nullptr);
  ESP_ERROR_CHECK(esp_netif_attach(netif, esp_openthread_netif_glue_init(&config)));
#ifndef USE_OPENTHREAD_BORDER_ROUTER
  esp_netif_set_default_netif(netif);
#endif

  ESP_LOGD(TAG, "Thread Version: %" PRIu16, otThreadGetVersion());

  // Mark lock as initialized so InstanceLock callers know it's safe to acquire.
  // Must be set after esp_openthread_init() which creates the internal semaphore.
  this->lock_initialized_ = true;

  bool launch_mainloop = true;
#if defined(USE_OPENTHREAD_BORDER_ROUTER) && !defined(USE_OPENTHREAD_RCP_UART)
  // The native 802.15.4 radio shares the RF frontend with Wi-Fi. Coexistence has to be on before
  // the Thread network starts below, or Wi-Fi scanning and association can fail outright.
  if (const esp_err_t err = esp_coex_wifi_i154_enable(); err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to enable Wi-Fi/802.15.4 coexistence: %s", esp_err_to_name(err));
    this->mark_task_failed_();
    launch_mainloop = false;
  }
#endif

  {
    // One lock for the whole bring-up, so the main loop (actions, teardown, the SRP component)
    // cannot call into the stack half way through
    InstanceLock lock = InstanceLock::acquire();
    otInstance *instance = lock.get_instance();

#ifndef USE_OPENTHREAD_BORDER_ROUTER
    this->apply_linkmode_(instance);
#endif

    if (this->output_power_.has_value()) {
      if (const auto err = otPlatRadioSetTransmitPower(instance, *this->output_power_); err != OT_ERROR_NONE) {
        ESP_LOGE(TAG, "Failed to set power: %s", otThreadErrorToString(err));
      }
    }

    ESP_LOGI(TAG, "Activating dataset...");
    otOperationalDatasetTlvs dataset = {};
#ifndef USE_OPENTHREAD_FORCE_DATASET
    // Check if openthread has a valid dataset from a previous execution
    if (otDatasetGetActiveTlvs(instance, &dataset) != OT_ERROR_NONE) {
      // Make sure the length is 0 so we fallback to the configuration
      dataset.mLength = 0;
    } else {
      ESP_LOGI(TAG, "Found existing dataset, ignoring config (force_dataset: true to override)");
    }
#endif

#ifdef USE_OPENTHREAD_TLVS
    if (dataset.mLength == 0) {
      // If we didn't have an active dataset, and we have tlvs, parse it and pass it to esp_openthread_auto_start
      constexpr size_t len = (sizeof(USE_OPENTHREAD_TLVS) - 1) / 2;
      static_assert(len <= sizeof(dataset.mTlvs), "OpenThread dataset TLV is too long");
      parse_hex(USE_OPENTHREAD_TLVS, sizeof(USE_OPENTHREAD_TLVS) - 1, dataset.mTlvs, len);
      dataset.mLength = len;
    }
#endif

    if (dataset.mLength > 0 && !otDatasetIsValid(&dataset, true)) {
      ESP_LOGE(TAG, "Configured OpenThread Active Operational Dataset TLV is invalid");
      this->mark_task_failed_();
      launch_mainloop = false;
    } else if (launch_mainloop) {
      // esp_openthread_auto_start() enables IPv6, which (for Border Router builds) opens a UDP socket
      // via otPlatUdpSocket(). That path releases and re-acquires the OpenThread task-switching lock,
      // which asserts unless this task holds the OpenThread lock, as it does here.
      if (const esp_err_t err = esp_openthread_auto_start(dataset.mLength > 0 ? &dataset : nullptr); err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start OpenThread: %s", esp_err_to_name(err));
        this->mark_task_failed_();
        launch_mainloop = false;
      }
    }

    if (launch_mainloop) {
      // Register state change callback to update connected_ reactively instead of polling
      otError ot_err = otSetStateChangedCallback(instance, OpenThreadComponent::on_state_changed, this);
      if (ot_err != OT_ERROR_NONE) {
        ESP_LOGE(TAG, "Failed to register state change callback: %d", ot_err);
        // Without this callback connected_ would never update, so the component would look
        // healthy forever while never actually reporting the device as connected.
        this->mark_task_failed_();
        launch_mainloop = false;
      }
    }
  }

  if (launch_mainloop) {
#ifdef USE_OPENTHREAD_BORDER_ROUTER
    this->mainloop_running_ = true;
#endif
    const esp_err_t err = esp_openthread_launch_mainloop();
#ifdef USE_OPENTHREAD_BORDER_ROUTER
    this->mainloop_running_ = false;
#endif
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "OpenThread main loop failed: %s", esp_err_to_name(err));
      this->mark_task_failed_();
    }
  }

  // Clean up - reset lock flag before deinit destroys the semaphore
  this->lock_initialized_ = false;
  esp_openthread_netif_glue_deinit();
  esp_netif_destroy(netif);
  esp_openthread_deinit();
  esp_vfs_eventfd_unregister();
  this->teardown_stage_ = TeardownStage::TEARDOWN_STAGE_COMPLETED;
}

// Runs on the OpenThread task; the component state belongs to the main loop
void OpenThreadComponent::mark_task_failed_() {
  this->defer([this]() { this->mark_failed(); });
}

#ifdef USE_OPENTHREAD_RCP_UART
void OpenThreadComponent::rcp_failure_handler() {
  if (global_openthread_component == nullptr) {
    ESP_LOGE(TAG, "RCP failure reported after OpenThread component was torn down");
    return;
  }
  global_openthread_component->reset_rcp_(true);
}

void OpenThreadComponent::reset_rcp_(bool recovery_attempt) {
  if (recovery_attempt) {
    static constexpr uint8_t MAX_RESET_ATTEMPTS = 5;
    uint8_t attempt = this->rcp_reset_attempts_;
    if (attempt > MAX_RESET_ATTEMPTS) {
      return;  // Gave up earlier; the driver keeps reporting the failure
    }
    this->rcp_reset_attempts_ = ++attempt;
    if (attempt > MAX_RESET_ATTEMPTS) {
      ESP_LOGE(TAG, "RCP failed to recover after %u reset attempts", MAX_RESET_ATTEMPTS);
      this->mark_task_failed_();
      return;
    }
    ESP_LOGW(TAG, "RCP failure detected, reset attempt %u of %u", attempt, MAX_RESET_ATTEMPTS);
  }
  this->rcp_reset_pin_->digital_write(true);
  vTaskDelay(pdMS_TO_TICKS(10));
  this->rcp_reset_pin_->digital_write(false);
  vTaskDelay(pdMS_TO_TICKS(30));
}
#endif

int OpenThreadComponent::openthread_stop_() { return esp_openthread_mainloop_exit(); }

network::IPAddresses OpenThreadComponent::get_ip_addresses() {
  network::IPAddresses addresses;
  if (esp_netif_t *netif = esp_openthread_get_netif(); netif != nullptr) {
    network::fill_ip6_addresses(netif, addresses);
  }
  return addresses;
}

#ifdef USE_OPENTHREAD_BORDER_ROUTER
void OpenThreadComponent::loop() {
  // Thread starts on its own; border routing waits for the Wi-Fi backbone to have an address
  if (!this->mainloop_running_ || !wifi::global_wifi_component->is_connected()) {
    return;
  }
  // Never wait for the lock on the main loop; the OpenThread task releases it between events
  auto lock = InstanceLock::try_acquire(0);
  if (!lock) {
    return;
  }
  if (const esp_err_t err = esp_openthread_border_router_init(); err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize OpenThread Border Router: %s", esp_err_to_name(err));
    this->mark_failed();
    return;
  }
  this->border_router_started_ = true;
  this->disable_loop();
}
#endif

// not thread safe, only use in read-only use cases
otInstance *OpenThreadComponent::get_openthread_instance_() { return esp_openthread_get_instance(); }

InstanceLock InstanceLock::try_acquire(int delay) {
  if (global_openthread_component == nullptr || !global_openthread_component->is_lock_initialized()) {
    return InstanceLock(false);
  }
  return InstanceLock(esp_openthread_lock_acquire(delay));
}

InstanceLock InstanceLock::acquire() {
  // teardown() clears global_openthread_component before the stack fully stops; a caller
  // racing teardown would otherwise dereference a null pointer below.
  if (global_openthread_component == nullptr) {
    ESP_LOGE(TAG, "OpenThread component torn down, cannot acquire instance lock");
    abort();
  }
  // Wait for the lock to be created before attempting to acquire it.
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
  return InstanceLock(true);
}

otInstance *InstanceLock::get_instance() { return esp_openthread_get_instance(); }

InstanceLock::~InstanceLock() {
  if (this->owns_) {
    esp_openthread_lock_release();
  }
}

}  // namespace esphome::openthread
#endif
