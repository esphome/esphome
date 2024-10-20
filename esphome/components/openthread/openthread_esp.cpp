#include "esphome/core/defines.h"
#if defined(USE_OPENTHREAD) && defined(USE_ESP_IDF)
#include "openthread.h"
#include <openthread/logging.h>

#include "esp_openthread.h"
#include "esp_openthread_lock.h"
#include "esp_log.h"

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esp_task_wdt.h"

#include "esp_openthread_cli.h"
#include "esp_openthread_netif_glue.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_vfs_eventfd.h"
#include "esp_netif.h"
#include "esp_netif_types.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG "openthread"

namespace esphome {
namespace openthread {

void OpenThreadComponent::setup() {
  ESP_LOGI(TAG, "Setting up OpenThread...");
  // Used eventfds:
  // * netif
  // * ot task queue
  // * radio driver
  esp_vfs_eventfd_config_t eventfd_config = {
      .max_fds = 3,
  };
  ESP_ERROR_CHECK(nvs_flash_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_vfs_eventfd_register(&eventfd_config));

  xTaskCreate(
      [](void *arg) {
        static_cast<OpenThreadComponent *>(arg)->ot_main();
        vTaskDelete(nullptr);
      },
      "ot_main", 10240, this, 5, nullptr);

  // xTaskCreate(
  //     [](void *arg) {
  //       static_cast<OpenThreadComponent *>(arg)->srp_setup_();
  //       vTaskDelete(nullptr);
  //     },
  //     "ot_srp_setup", 10240, this, 5, nullptr);

  ESP_LOGI(TAG, "OpenThread started");
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
  };

  // Initialize the OpenThread stack
  ESP_ERROR_CHECK(esp_openthread_init(&config));

  this->srp_setup_();

#if CONFIG_OPENTHREAD_STATE_INDICATOR_ENABLE
  ESP_ERROR_CHECK(esp_openthread_state_indicator_init(esp_openthread_get_instance()));
#endif

#if CONFIG_OPENTHREAD_LOG_LEVEL_DYNAMIC
  // The OpenThread log level directly matches ESP log level
  (void) otLoggingSetLevel(CONFIG_LOG_DEFAULT_LEVEL);
#endif
  // Initialize the OpenThread cli
#if CONFIG_OPENTHREAD_CLI
  esp_openthread_cli_init();
#endif

  esp_netif_t *openthread_netif;
  // Initialize the esp_netif bindings
  openthread_netif = init_openthread_netif(&config);
  esp_netif_set_default_netif(openthread_netif);

#if CONFIG_OPENTHREAD_CLI_ESP_EXTENSION
  esp_cli_custom_command_init();
#endif  // CONFIG_OPENTHREAD_CLI_ESP_EXTENSION

  // Run the main loop
#if CONFIG_OPENTHREAD_CLI
  esp_openthread_cli_create_task();
#endif
  ESP_LOGI(TAG, "Activating dataset...");
  otOperationalDatasetTlvs dataset;
  otError error = otDatasetGetActiveTlvs(esp_openthread_get_instance(), &dataset);
  ESP_ERROR_CHECK(esp_openthread_auto_start((error == OT_ERROR_NONE) ? &dataset : NULL));

  esp_openthread_launch_mainloop();

  // Clean up
  esp_openthread_netif_glue_deinit();
  esp_netif_destroy(openthread_netif);

  esp_vfs_eventfd_unregister();
}

network::IPAddresses OpenThreadComponent::get_ip_addresses() {
  network::IPAddresses addresses;
  struct esp_ip6_addr if_ip6s[CONFIG_LWIP_IPV6_NUM_ADDRESSES];
  uint8_t count = 0;
  esp_netif_t *netif = esp_netif_get_default_netif();
  count = esp_netif_get_all_ip6(netif, if_ip6s);
  assert(count <= CONFIG_LWIP_IPV6_NUM_ADDRESSES);
  for (int i = 0; i < count; i++) {
    addresses[i + 1] = network::IPAddress(&if_ip6s[i]);
  }
  return addresses;
}

std::optional<InstanceLock> InstanceLock::try_acquire(int delay) {
  if (esp_openthread_lock_acquire(delay)) {
    return InstanceLock();
  }
  return {};
}

std::optional<InstanceLock> InstanceLock::acquire() {
  while (!esp_openthread_lock_acquire(100)) {
    esp_task_wdt_reset();
  }
  return InstanceLock();
}

otInstance *InstanceLock::get_instance() { return esp_openthread_get_instance(); }

InstanceLock::~InstanceLock() { esp_openthread_lock_release(); }

}  // namespace openthread
}  // namespace esphome
#endif
