#include "esphome/core/defines.h"
#if defined(USE_OPENTHREAD) && defined(USE_ESP32)
#include <algorithm>

#include <openthread/logging.h>
#include <openthread/dataset.h>
#include "openthread.h"

#ifdef USE_OPENTHREAD_BORDER_ROUTER
#include "esp_coexist.h"
#include "esp_openthread_border_router.h"
#include "esphome/components/wifi/wifi_component.h"
#endif
#ifdef USE_OPENTHREAD_RCP_UART
#include "driver/gpio.h"
#include "esp_openthread_spinel.h"
#endif
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
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

static const char *const TAG = "openthread";

namespace esphome::openthread {

void OpenThreadComponent::setup() {
#ifdef USE_OPENTHREAD_BORDER_ROUTER
  esp_netif_t *backbone_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
  if (backbone_netif == nullptr) {
    ESP_LOGE(TAG, "Wi-Fi STA backbone netif is not initialized");
    this->mark_failed();
    return;
  }
  esp_openthread_set_backbone_netif(backbone_netif);
#endif

  // Used eventfds:
  // * netif
  // * ot task queue
  // * radio driver
#if defined(USE_OPENTHREAD_BORDER_ROUTER) && !defined(USE_OPENTHREAD_RCP_UART)
  // * border router
  constexpr size_t max_eventfds = 4;
#elif defined(USE_OPENTHREAD_BORDER_ROUTER)
  // UART Spinel does not use an eventfd for the radio transport.
  constexpr size_t max_eventfds = 3;
#else
  constexpr size_t max_eventfds = 3;
#endif
  esp_vfs_eventfd_config_t eventfd_config = {
      .max_fds = max_eventfds,
  };
  // Network interface setup handled by network component
  ESP_ERROR_CHECK(nvs_flash_init());
  ESP_ERROR_CHECK(esp_vfs_eventfd_register(&eventfd_config));

  if (xTaskCreate(
          [](void *arg) {
            static_cast<OpenThreadComponent *>(arg)->ot_main();
            vTaskDelete(nullptr);
          },
          "ot_main", 10240, this, 5, nullptr) != pdPASS) {
    ESP_LOGE(TAG, "Failed to create OpenThread task");
    esp_vfs_eventfd_unregister();
    this->mark_failed();
  }
}

static esp_netif_t *init_openthread_netif(const esp_openthread_platform_config_t *config) {
  esp_netif_config_t cfg = ESP_NETIF_DEFAULT_OPENTHREAD();
  esp_netif_t *netif = esp_netif_new(&cfg);
  assert(netif != nullptr);
  ESP_ERROR_CHECK(esp_netif_attach(netif, esp_openthread_netif_glue_init(config)));

  return netif;
}

void OpenThreadComponent::ot_main() {
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
  if (this->rcp_reset_pin_ >= 0) {
    esp_openthread_register_rcp_failure_handler(OpenThreadComponent::rcp_failure_handler);
    esp_openthread_set_coprocessor_reset_failure_callback(OpenThreadComponent::rcp_failure_handler);
    this->reset_rcp_();
  }
#endif

  // Initialize the OpenThread stack
  // otLoggingSetLevel(OT_LOG_LEVEL_DEBG);
  ESP_ERROR_CHECK(esp_openthread_init(&config));
  // Mark lock as initialized so InstanceLock callers know it's safe to acquire.
  // Must be set after esp_openthread_init() which creates the internal semaphore.
  this->lock_initialized_ = true;

  bool launch_mainloop = true;
#if defined(USE_OPENTHREAD_BORDER_ROUTER) && !defined(USE_OPENTHREAD_RCP_UART)
  // Enable Wi-Fi/802.15.4 coexistence as soon as the native radio is initialized, before the
  // Thread network is auto-started below. The native 802.15.4 radio and Wi-Fi share the same RF
  // frontend on chips like the ESP32-C6; without this, both radios contend for the RF outside the
  // coexistence arbiter and Wi-Fi scanning/association can fail outright. Doing this only after
  // Wi-Fi has connected (as the border router used to do) is too late, since the Thread network
  // auto-starts immediately below while Wi-Fi may still be trying to connect.
  if (const esp_err_t err = esp_coex_wifi_i154_enable(); err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to enable Wi-Fi/802.15.4 coexistence: %s", esp_err_to_name(err));
    this->mark_task_failed_();
    launch_mainloop = false;
  }
#endif
  // Fetch OT instance once to avoid repeated call into OT stack
  otInstance *instance = esp_openthread_get_instance();

#if CONFIG_OPENTHREAD_STATE_INDICATOR_ENABLE
  ESP_ERROR_CHECK(esp_openthread_state_indicator_init(instance));
#endif

#if CONFIG_OPENTHREAD_LOG_LEVEL_DYNAMIC
  // The OpenThread log level directly matches ESP log level
  (void) otLoggingSetLevel(CONFIG_LOG_DEFAULT_LEVEL);
#endif
  // Initialize the OpenThread cli
#if CONFIG_OPENTHREAD_CLI
  esp_openthread_cli_init();
#endif

  // Initialize the esp_netif bindings
  this->openthread_netif_ = init_openthread_netif(&config);
#ifndef USE_OPENTHREAD_BORDER_ROUTER
  esp_netif_set_default_netif(this->openthread_netif_);
#endif

#if CONFIG_OPENTHREAD_CLI_ESP_EXTENSION
  esp_cli_custom_command_init();
#endif  // CONFIG_OPENTHREAD_CLI_ESP_EXTENSION

  ESP_LOGD(TAG, "Thread Version: %" PRIu16, otThreadGetVersion());

#ifndef USE_OPENTHREAD_BORDER_ROUTER
  this->apply_linkmode_(instance);
#endif

  if (this->output_power_.has_value()) {
    if (const auto err = otPlatRadioSetTransmitPower(instance, *this->output_power_); err != OT_ERROR_NONE) {
      ESP_LOGE(TAG, "Failed to set power: %s", otThreadErrorToString(err));
    }
  }

  // Run the main loop
#if CONFIG_OPENTHREAD_CLI
  esp_openthread_cli_create_task();
#endif
  ESP_LOGI(TAG, "Activating dataset...");
  otOperationalDatasetTlvs dataset = {};

#ifndef USE_OPENTHREAD_FORCE_DATASET
  {
    // Hold the OpenThread lock for all instance access below, matching Espressif's own
    // ot_network_auto_start() helper (see the comment on esp_openthread_auto_start() below).
    InstanceLock lock = InstanceLock::acquire();
    // Check if openthread has a valid dataset from a previous execution
    otError error = otDatasetGetActiveTlvs(instance, &dataset);
    if (error != OT_ERROR_NONE) {
      // Make sure the length is 0 so we fallback to the configuration
      dataset.mLength = 0;
    } else {
      ESP_LOGI(TAG, "Found existing dataset, ignoring config (force_dataset: true to override)");
    }
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
    // esp_openthread_auto_start() calls into otIp6SetEnabled(), which (for Border Router
    // builds) starts the DNS-SD server and opens a UDP socket via otPlatUdpSocket(). That
    // path temporarily releases and re-acquires the OpenThread task-switching lock, which
    // asserts unless this task already holds the OpenThread lock. Espressif's own examples
    // (ot_network_auto_start() in ot_examples_common) always take the lock around
    // esp_openthread_auto_start() for this reason.
    InstanceLock lock = InstanceLock::acquire();
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

  if (launch_mainloop) {
    this->ready_ = true;
    esp_openthread_launch_mainloop();
    this->ready_ = false;
  }

  // Clean up - reset lock flag before deinit destroys the semaphore
  this->lock_initialized_ = false;
  esp_openthread_deinit();
  esp_openthread_netif_glue_deinit();
  esp_netif_destroy(this->openthread_netif_);
  this->openthread_netif_ = nullptr;

  esp_vfs_eventfd_unregister();
  this->teardown_stage_ = TeardownStage::TEARDOWN_STAGE_COMPLETED;
  vTaskDelete(NULL);
}

void OpenThreadComponent::mark_task_failed_() {
  this->task_failed_ = true;
  this->defer([this]() { this->mark_failed(); });
}

#ifdef USE_OPENTHREAD_RCP_UART
void OpenThreadComponent::rcp_failure_handler() {
  if (global_openthread_component == nullptr) {
    ESP_LOGE(TAG, "RCP failure reported after OpenThread component was torn down");
    return;
  }
  global_openthread_component->reset_rcp_();
}

void OpenThreadComponent::reset_rcp_() {
  static constexpr uint8_t MAX_RESET_ATTEMPTS = 5;
  if (++this->rcp_reset_attempts_ > MAX_RESET_ATTEMPTS) {
    ESP_LOGE(TAG, "RCP failed to recover after %u reset attempts, giving up", MAX_RESET_ATTEMPTS);
    this->mark_task_failed_();
    return;
  }
  gpio_config_t reset_pin_config{};
  reset_pin_config.pin_bit_mask = 1ULL << this->rcp_reset_pin_;
  reset_pin_config.mode = GPIO_MODE_OUTPUT;
  if (const esp_err_t err = gpio_config(&reset_pin_config); err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to configure RCP reset pin: %s", esp_err_to_name(err));
    this->mark_task_failed_();
    return;
  }
  gpio_set_level(static_cast<gpio_num_t>(this->rcp_reset_pin_), this->rcp_reset_active_level_);
  vTaskDelay(pdMS_TO_TICKS(10));
  gpio_set_level(static_cast<gpio_num_t>(this->rcp_reset_pin_), !this->rcp_reset_active_level_);
  vTaskDelay(pdMS_TO_TICKS(30));
}
#endif

int OpenThreadComponent::openthread_stop_() { return esp_openthread_mainloop_exit(); }

network::IPAddresses OpenThreadComponent::get_ip_addresses() {
  network::IPAddresses addresses;
  esp_netif_t *netif = this->openthread_netif_;
  if (netif == nullptr) {
    return addresses;
  }
  struct esp_ip6_addr if_ip6s[CONFIG_LWIP_IPV6_NUM_ADDRESSES];
  uint8_t count = esp_netif_get_all_ip6(netif, if_ip6s);
  assert(count <= CONFIG_LWIP_IPV6_NUM_ADDRESSES);
  if (count > addresses.size() - 1) {
    static bool truncation_logged = false;
    if (!truncation_logged) {
      truncation_logged = true;
      ESP_LOGW(TAG, "OpenThread has %u IPv6 addresses, but only %zu fit; some are not reported", count,
               addresses.size() - 1);
    }
  }
  count = std::min<size_t>(count, addresses.size() - 1);
  for (uint8_t i = 0; i < count; i++) {
    addresses[i + 1] = network::IPAddress(&if_ip6s[i]);
  }
  return addresses;
}

#ifdef USE_OPENTHREAD_BORDER_ROUTER
void OpenThreadBorderRouterComponent::setup() {
  if (this->openthread_->is_failed()) {
    this->mark_failed();
  }
}

void OpenThreadBorderRouterComponent::loop() {
  if (this->started_ || this->is_failed()) {
    return;
  }
  if (this->openthread_->is_failed() || this->openthread_->has_task_failed()) {
    this->mark_failed();
    return;
  }
  if (!this->openthread_->is_ready() || !wifi::global_wifi_component->is_connected()) {
    return;
  }

  auto lock = InstanceLock::try_acquire(100);
  if (!lock) {
    // Each loop() call retries after a 100 ms lock-acquire timeout; warn if it keeps
    // failing for a while so a wedged OpenThread task doesn't fail silently forever.
    static constexpr uint16_t WARN_AFTER_ATTEMPTS = 20;   // ~2s
    static constexpr uint16_t FAIL_AFTER_ATTEMPTS = 100;  // ~10s
    if (++this->lock_wait_failures_ == WARN_AFTER_ATTEMPTS) {
      ESP_LOGW(TAG, "Border router init has been waiting on the OpenThread lock for %u attempts",
               this->lock_wait_failures_);
    } else if (this->lock_wait_failures_ >= FAIL_AFTER_ATTEMPTS) {
      ESP_LOGE(TAG, "Border router init could not acquire the OpenThread lock after %u attempts, giving up",
               this->lock_wait_failures_);
      this->mark_failed();
    }
    return;
  }
  // Wi-Fi/802.15.4 coexistence (native radio builds only) is enabled earlier, right after the
  // OpenThread radio is initialized in ot_main() -- see the comment there for why it can't wait
  // until Wi-Fi is connected.
  esp_err_t err = esp_openthread_border_router_init();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize OpenThread Border Router: %s", esp_err_to_name(err));
    this->mark_failed();
    return;
  }
  this->started_ = true;
}

void OpenThreadBorderRouterComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "OpenThread Border Router:\n"
                     "  Backbone: Wi-Fi STA\n"
#ifdef USE_OPENTHREAD_RCP_UART
                     "  Radio: External UART RCP\n"
#else
                     "  Radio: Native 802.15.4\n"
#endif
                     "  Startup: After Wi-Fi connection\n"
                     "  Experimental: YES");
}

bool OpenThreadBorderRouterComponent::teardown() {
  if (!this->started_) {
    return true;
  }
  auto lock = InstanceLock::try_acquire(100);
  if (!lock) {
    return false;
  }
  if (const esp_err_t err = esp_openthread_border_router_deinit(); err != ESP_OK) {
    ESP_LOGW(TAG, "Failed to deinitialize OpenThread Border Router: %s", esp_err_to_name(err));
  }
  this->started_ = false;
  return true;
}
#endif

#ifdef USE_OPENTHREAD_ANTENNA_SWITCH
void OpenThreadAntennaSwitchComponent::setup() {
  this->select_pin_->setup();
  // Some boards (e.g. Seeed Studio XIAO ESP32-C6) gate the RF switch behind a separate
  // enable line that must be driven active before the antenna-select pin has any effect.
  // The reference vendor example waits after enabling the switch before driving the
  // select pin, so mirror that here for reliable switching.
  if (this->enable_pin_ != nullptr) {
    this->enable_pin_->setup();
    this->enable_pin_->digital_write(true);
    delay(100);  // NOLINT - matches vendor RF-switch enable sequencing, runs once at setup
  }
  this->select_pin_->digital_write(this->external_antenna_);
}

void OpenThreadAntennaSwitchComponent::dump_config() {
  ESP_LOGCONFIG(TAG,
                "OpenThread Antenna Switch:\n"
                "  Selected: %s antenna",
                this->external_antenna_ ? "external" : "onboard");
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
