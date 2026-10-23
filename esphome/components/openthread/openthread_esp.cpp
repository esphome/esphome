#include "esphome/core/defines.h"
#if defined(USE_OPENTHREAD) && defined(USE_ESP32)
#include <openthread/dataset.h>
#include <openthread/ip6.h>
#include <openthread/tasklet.h>
#ifdef USE_OPENTHREAD_BORDER_ROUTER
#include <openthread/platform/infra_if.h>
#endif
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
#include "esphome/core/preferences.h"

#include "esp_err.h"
#include "esp_netif.h"
#include "esp_openthread_netif_glue.h"
#include "esp_vfs_eventfd.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

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
  wifi::global_wifi_component->add_connect_state_listener(this);
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
  // Network interface setup handled by network component.
  // Preferences opens NVS in app_main(); OpenThread keeps its dataset there, and esp_openthread_init()
  // aborts without it, so fail this component instead.
  if (global_preferences->nvs_handle == 0) {
    ESP_LOGE(TAG, "NVS unavailable");
    this->teardown_stage_ = TeardownStage::TEARDOWN_STAGE_COMPLETED;
    this->mark_failed();
    return;
  }
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

  otInstance *instance = esp_openthread_get_instance();
  bool launch_mainloop = true;
#if defined(USE_OPENTHREAD_BORDER_ROUTER) && !defined(USE_OPENTHREAD_RCP_UART)
  // The native 802.15.4 radio shares the RF frontend with Wi-Fi. Coexistence has to be on before
  // the Thread network starts below, or Wi-Fi scanning and association can fail outright.
  if (const esp_err_t err = esp_coex_wifi_i154_enable(); err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to enable Wi-Fi/802.15.4 coexistence: %s", esp_err_to_name(err));
    this->mark_task_failed_(LOG_STR("Coexistence could not be enabled"));
    launch_mainloop = false;
  }
#endif

  {
    // One lock for the whole bring-up, so the main loop (actions, teardown, the SRP component)
    // cannot call into the stack half way through. The lock is recursive: hold it before
    // telling the other tasks that it exists, then let the guard take over.
    esp_openthread_lock_acquire(portMAX_DELAY);
    this->lock_initialized_ = true;
    InstanceLock lock = InstanceLock::acquire();
    esp_openthread_lock_release();

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
    bool stored_dataset = false;
#ifndef USE_OPENTHREAD_FORCE_DATASET
    // Check if openthread has a valid dataset from a previous execution
    if (otDatasetGetActiveTlvs(instance, &dataset) != OT_ERROR_NONE) {
      // Make sure the length is 0 so we fallback to the configuration
      dataset.mLength = 0;
    } else {
      stored_dataset = true;
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
      if (stored_dataset) {
        ESP_LOGE(TAG, "Stored OpenThread Active Operational Dataset is invalid; set force_dataset: true to replace it");
      } else {
        ESP_LOGE(TAG, "Configured OpenThread Active Operational Dataset TLV is invalid");
      }
      this->mark_task_failed_(LOG_STR("Invalid dataset"));
      launch_mainloop = false;
    } else if (launch_mainloop) {
      // esp_openthread_auto_start() enables IPv6, which (for Border Router builds) opens a UDP socket
      // via otPlatUdpSocket(). That path releases and re-acquires the OpenThread task-switching lock,
      // which asserts unless this task holds the OpenThread lock, as it does here.
      if (const esp_err_t err = esp_openthread_auto_start(dataset.mLength > 0 ? &dataset : nullptr); err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start OpenThread: %s", esp_err_to_name(err));
        this->mark_task_failed_(LOG_STR("Start failed"));
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
        this->mark_task_failed_(LOG_STR("State callback could not be registered"));
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
      this->mark_task_failed_(LOG_STR("Main loop failed"));
    }
  }

  // Clean up. Taking the lock once more lets every guard finish; the stack is stopped and the
  // lock withdrawn while it is held.
  esp_openthread_lock_acquire(portMAX_DELAY);
  // Best effort: after a normal teardown all of this is done already
  otIp6SetAddressCallback(instance, nullptr, nullptr);
#ifdef USE_OPENTHREAD_BORDER_ROUTER
  if (this->border_router_started_) {
    (void) esp_openthread_border_router_deinit();
    this->border_router_started_ = false;
  }
#endif
  (void) otThreadSetEnabled(instance, false);
  (void) otIp6SetEnabled(instance, false);
  // Nothing runs the main loop any more, so the stack's reaction to the disables (the border
  // agent closing its sockets, for one) is run here, with the lock held
  for (uint8_t i = 0; i < 16 && otTaskletsArePending(instance); i++) {
    otTaskletsProcess(instance);
  }
  this->lock_initialized_ = false;
  this->connected_ = false;
  this->task_exited_ = true;
  esp_openthread_lock_release();
  esp_openthread_netif_glue_deinit();
  esp_netif_destroy(netif);
  // Finalizing the instance closes the sockets the stack still holds, which hands the task
  // switching lock back and forth, so it has to be held here; deinit destroys it at the end
  esp_openthread_lock_acquire(portMAX_DELAY);
  esp_openthread_deinit();
  esp_vfs_eventfd_unregister();
  this->teardown_stage_ = TeardownStage::TEARDOWN_STAGE_COMPLETED;
}

// Runs on the OpenThread task; the component state belongs to the main loop, and the log goes
// out from there too, where an abort that follows on the task cannot swallow it
void OpenThreadComponent::mark_task_failed_(const LogString *reason) {
  this->defer([this, reason]() {
    ESP_LOGE(TAG, "OpenThread task failed: %s", LOG_STR_ARG(reason));
    this->mark_failed();
  });
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
    // An RCP that ran long enough since its last reset gets its attempts back; one that keeps failing does not
    const uint32_t now = millis();
    if (attempt != 0 && now - this->last_rcp_reset_ms_ > RCP_RESET_FORGET_MS) {
      attempt = 0;
    }
    this->rcp_reset_attempts_ = ++attempt;
    if (attempt > MAX_RESET_ATTEMPTS) {
      ESP_LOGE(TAG, "RCP failed to recover after %u reset attempts", MAX_RESET_ATTEMPTS);
      this->mark_task_failed_(LOG_STR("RCP does not recover"));
      return;
    }
    ESP_LOGW(TAG, "RCP failure detected, reset attempt %u of %u", attempt, MAX_RESET_ATTEMPTS);
    this->last_rcp_reset_ms_ = now;
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
#if USE_NETWORK_IPV6
  // The task's clean-up destroys the netif under the lock, so the netif is only read while it is held;
  // the OpenThread task releases the lock between events, so the wait is short
  auto lock = InstanceLock::try_acquire(10);
  if (!lock) {
    ESP_LOGV(TAG, "Instance lock busy, no addresses this time");
    return addresses;
  }
  if (esp_netif_t *netif = esp_openthread_get_netif(); netif != nullptr) {
    network::fill_ip6_addresses(netif, addresses);
  }
#endif
  return addresses;
}

#ifdef USE_OPENTHREAD_BORDER_ROUTER
static constexpr uint32_t BORDER_ROUTER_RETRY_MS = 10000;

void OpenThreadComponent::loop() {
  // Thread starts on its own. Border routing starts once the Wi-Fi backbone has an address. When the
  // backbone goes away the stack is told so it pauses routing on that interface; when it comes back
  // the border router is created afresh, since the port's infra interface keeps the socket and the
  // addresses of the backbone as it was. The Wi-Fi listener enables the loop on every link change
  if (!this->mainloop_running_) {
    return;
  }
  const bool backbone_up = wifi::global_wifi_component->is_connected();
  if (backbone_up == this->backbone_up_) {
    this->disable_loop();
    return;
  }
  // Never wait for the lock on the main loop; the OpenThread task releases it between events
  auto lock = InstanceLock::try_acquire(0);
  if (!lock) {
    return;
  }
  auto retry_later = [this](const LogString *reason) {
    this->status_set_error(reason);
    this->disable_loop();
    this->set_timeout(BORDER_ROUTER_RETRY_MS, [this]() { this->enable_loop(); });
  };
  if (!backbone_up) {
    if (this->border_router_started_) {
      ESP_LOGI(TAG, "Wi-Fi backbone lost, border routing pauses");
      esp_netif_t *backbone = esp_openthread_get_backbone_netif();
      // A refused report is not retried: the border router is created afresh when the backbone returns
      if (otPlatInfraIfStateChanged(lock.get_instance(), esp_netif_get_netif_impl_index(backbone), false) !=
          OT_ERROR_NONE) {
        ESP_LOGW(TAG, "Stack did not take the backbone loss, routes stay until Wi-Fi returns");
      }
    }
    this->backbone_up_ = false;
    this->disable_loop();
    return;
  }
  if (this->border_router_started_) {
    ESP_LOGI(TAG, "Wi-Fi backbone back, border routing restarts");
    // The old router stays marked as started until it is gone, so a retry tears it down first
    if (const esp_err_t err = esp_openthread_border_router_deinit(); err != ESP_OK) {
      ESP_LOGE(TAG, "Failed to deinitialize OpenThread Border Router: %s", esp_err_to_name(err));
      retry_later(LOG_STR("Border router deinit failed"));
      return;
    }
    this->border_router_started_ = false;
  }
  if (const esp_err_t err = esp_openthread_border_router_init(); err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize OpenThread Border Router: %s", esp_err_to_name(err));
    retry_later(LOG_STR("Border router init failed"));
    return;
  }
  this->status_clear_error();
  this->border_router_started_ = true;
  this->backbone_up_ = true;
  ESP_LOGI(TAG, "Border routing started");
  this->disable_loop();
}
#endif

// not thread safe, only use in read-only use cases
otInstance *OpenThreadComponent::get_openthread_instance_() { return esp_openthread_get_instance(); }

InstanceLock InstanceLock::try_acquire(int delay) {
  if (global_openthread_component == nullptr || !global_openthread_component->is_lock_initialized()) {
    return InstanceLock(false);
  }
  if (!esp_openthread_lock_acquire(delay)) {
    return InstanceLock(false);
  }
  // The clean-up may have released the lock to a waiter just before deinit destroys it
  if (global_openthread_component->is_task_exited()) {
    esp_openthread_lock_release();
    return InstanceLock(false);
  }
  return InstanceLock(true);
}

// The lock went away with the task, so waiting for it could never end
static void abort_if_task_exited() {
  if (global_openthread_component->is_task_exited()) {
    ESP_LOGE(TAG, "OpenThread task has exited, cannot acquire instance lock");
    abort();
  }
}

InstanceLock InstanceLock::acquire() {
  // teardown() clears global_openthread_component before the stack fully stops; a caller
  // racing teardown would otherwise dereference a null pointer below.
  if (global_openthread_component == nullptr) {
    ESP_LOGE(TAG, "OpenThread component torn down, cannot acquire instance lock");
    abort();
  }
  // A failed setup() never creates the lock; say so now instead of after the wait below
  if (global_openthread_component->is_failed() && !global_openthread_component->is_lock_initialized()) {
    ESP_LOGE(TAG, "Setup failed, cannot acquire instance lock");
    abort();
  }
  // Wait for the lock to be created before attempting to acquire it.
  // esp_openthread_lock_acquire() will assert-crash if called before esp_openthread_init().
  constexpr uint32_t lock_init_timeout_ms = 10000;
  uint32_t start = millis();
  while (!global_openthread_component->is_lock_initialized()) {
    abort_if_task_exited();
    if (millis() - start > lock_init_timeout_ms) {
      ESP_LOGE(TAG, "OpenThread lock not initialized after %" PRIu32 "ms, aborting", lock_init_timeout_ms);
      abort();
    }
    delay(10);
    esp_task_wdt_reset();
  }
  while (!esp_openthread_lock_acquire(100)) {
    abort_if_task_exited();
    esp_task_wdt_reset();
  }
  // The clean-up may have released the lock to this waiter just before deinit destroys it
  if (global_openthread_component->is_task_exited()) {
    esp_openthread_lock_release();
    abort_if_task_exited();
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
