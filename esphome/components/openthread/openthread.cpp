#include "openthread.h"

#ifdef USE_ESP_IDF
#include "openthread_esp.h"
#else
#error "OpenThread is not supported on this platform"
#endif

#include <freertos/portmacro.h>

#include <openthread/srp_client.h>
#include <openthread/srp_client_buffers.h>
#include <openthread/netdata.h>
#include <openthread/cli.h>
#include <openthread/instance.h>
#include <openthread/logging.h>
#include <openthread/tasklet.h>

#include <cstring>

#define TAG "openthread"

namespace esphome {
namespace openthread {

OpenThreadComponent *global_openthread_component = nullptr;

OpenThreadComponent::OpenThreadComponent() {
    global_openthread_component = this;
}

OpenThreadComponent::~OpenThreadComponent() {
    auto lock = EspOpenThreadLockGuard::TryAcquire(100);
    if (!lock) {
        ESP_LOGW(TAG, "Failed to acquire OpenThread lock in destructor, leaking memory");
        return;
    }
    otInstance *instance = esp_openthread_get_instance();
    otSrpClientClearHostAndServices(instance);
    otSrpClientBuffersFreeAllServices(instance);

    global_openthread_component = nullptr;
}

void OpenThreadComponent::setup() {
    ESP_LOGI("openthread", "Setting up OpenThread...");
    // Used eventfds:
    // * netif
    // * ot task queue
    // * radio driver
    // TODO: Does anything else in esphome set this up?
    esp_vfs_eventfd_config_t eventfd_config = {
        .max_fds = 3,
    };
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_vfs_eventfd_register(&eventfd_config));

    xTaskCreate([](void* arg) {
        static_cast<OpenThreadComponent*>(arg)->ot_main();
        vTaskDelete(NULL);
    }, "ot_main", 10240, this, 5, nullptr);

    xTaskCreate([](void* arg) {
        static_cast<OpenThreadComponent*>(arg)->srp_setup();
        vTaskDelete(NULL);
    }, "ot_srp_setup", 10240, this, 5, nullptr);
    ESP_LOGI("openthread", "OpenThread started");
}

static esp_netif_t *init_openthread_netif(const esp_openthread_platform_config_t *config)
{
    esp_netif_config_t cfg = ESP_NETIF_DEFAULT_OPENTHREAD();
    esp_netif_t *netif = esp_netif_new(&cfg);
    assert(netif != NULL);
    ESP_ERROR_CHECK(esp_netif_attach(netif, esp_openthread_netif_glue_init(config)));

    return netif;
}

void OpenThreadComponent::ot_main() {

    esp_openthread_platform_config_t config = {
        .radio_config = {
            .radio_mode = RADIO_MODE_NATIVE,
            .radio_uart_config = {},
        },
        .host_config = {
          // There is a conflict between esphome's logger which also
          // claims the usb serial jtag device.
          // .host_connection_mode = HOST_CONNECTION_MODE_CLI_USB,
          // .host_usb_config = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT(),
        },
        .port_config = {
          .storage_partition_name = "nvs",
          .netif_queue_size = 10,
          .task_queue_size = 10,
        },
    };

   // Initialize the OpenThread stack
    ESP_ERROR_CHECK(esp_openthread_init(&config));

#if CONFIG_OPENTHREAD_STATE_INDICATOR_ENABLE
    ESP_ERROR_CHECK(esp_openthread_state_indicator_init(esp_openthread_get_instance()));
#endif

#if CONFIG_OPENTHREAD_LOG_LEVEL_DYNAMIC
    // The OpenThread log level directly matches ESP log level
    (void)otLoggingSetLevel(CONFIG_LOG_DEFAULT_LEVEL);
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
#endif // CONFIG_OPENTHREAD_CLI_ESP_EXTENSION

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

bool OpenThreadComponent::is_connected() {
    otInstance *instance = esp_openthread_get_instance();
    if (instance == nullptr) {
        return false;
    }

    otDeviceRole role = otThreadGetDeviceRole(instance);

    // TODO: If we're a leader, check that there is at least 1 known peer
    return role >= OT_DEVICE_ROLE_CHILD;
}

// TODO: This gets used by mqtt in order to register the device's IP. Likely it doesn't
// make sense to return thread-local addresses, since they can't be reached from outside the thread network.
// It could make more sense to return the off-mesh-routable address instead.
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

// Gets the off-mesh routable address
std::optional<otIp6Address> OpenThreadComponent::get_omr_address() {
    auto lock = EspOpenThreadLockGuard::Acquire();
    return this->get_omr_address(lock);
}

std::optional<otIp6Address> OpenThreadComponent::get_omr_address(std::optional<EspOpenThreadLockGuard> &lock) {
    otNetworkDataIterator iterator = OT_NETWORK_DATA_ITERATOR_INIT;
    otInstance *instance = nullptr;

    instance = esp_openthread_get_instance();

    otBorderRouterConfig aConfig;
    while (otNetDataGetNextOnMeshPrefix(instance, &iterator, &aConfig) != OT_ERROR_NONE) {
        lock.reset();
        vTaskDelay(100);
        lock = EspOpenThreadLockGuard::TryAcquire(portMAX_DELAY);
        if (!lock) {
            ESP_LOGW("OT SRP", "Could not re-acquire lock");
            return {};
        }
    };
    const otIp6Prefix * omrPrefix = &aConfig.mPrefix;

    char addressAsString[40];
    otIp6PrefixToString(omrPrefix, addressAsString, 40);
    ESP_LOGW("OT SRP", "USING omr prefix %s", addressAsString);

    const otNetifAddress *unicastAddrs = otIp6GetUnicastAddresses(instance);
    for (const otNetifAddress *addr = unicastAddrs; addr; addr = addr->mNext){
        const otIp6Address *localIp = &addr->mAddress;
        if (otIp6PrefixMatch(&omrPrefix->mPrefix, localIp)) {
            otIp6AddressToString(localIp, addressAsString, 40);
            ESP_LOGW("OT SRP", "USING %s for SRP address", addressAsString);
            return *localIp;
        }
    }
    ESP_LOGW("OT SRP", "Could not find the OMR address");
    return {};
}

void OpenThreadComponent::srp_setup(){
    otError error;
    otInstance *instance = nullptr;
    auto lock = EspOpenThreadLockGuard::Acquire();
    instance = esp_openthread_get_instance();

    // set the host name
    uint16_t size;
    char *existing_host_name = otSrpClientBuffersGetHostNameString(instance, &size);
    uint16_t len = host_name.size();
    if (len > size) {
        ESP_LOGW("OT SRP", "Hostname is too long, choose a shorter project name");
        return;
    }
    memcpy(existing_host_name, host_name.c_str(), len + 1);

    error = otSrpClientSetHostName(instance, existing_host_name);
    if (error != 0) {
        ESP_LOGW("OT SRP", "Could not set host name with srp server");
        return;
    }

    uint8_t arrayLength;
    otIp6Address *hostAddressArray = otSrpClientBuffersGetHostAddressesArray(instance, &arrayLength);

    const std::optional<otIp6Address> localIp = this->get_omr_address(lock);
    if (!localIp) {
        ESP_LOGW("OT SRP", "Could not get local IP address");
        return;
    }
    memcpy(hostAddressArray, &*localIp, sizeof(localIp));

    error = otSrpClientSetHostAddresses(instance, hostAddressArray, 1);
    if (error != 0){
        ESP_LOGW("OT SRP", "Could not set ip address with srp server");
        return;
    }

    // Copy the mdns services to our local instance so that the c_str pointers remain valid for the lifetime of this component
    this->mdns_services_ = this->mdns_->get_services();
    for (const auto& service : this->mdns_services_) {
        otSrpClientBuffersServiceEntry *entry = otSrpClientBuffersAllocateService(instance);
        if (!entry) {
            ESP_LOGW("OT SRP", "Failed to allocate service entry");
            continue;
        }

        // Set service name
        char *string = otSrpClientBuffersGetServiceEntryServiceNameString(entry, &size);
        std::string full_service = service.service_type + "." + service.proto;
        if (full_service.size() > size) {
            ESP_LOGW("OT SRP", "Service name too long: %s", full_service.c_str());
            continue;
        }
        memcpy(string, full_service.c_str(), full_service.size() + 1);

        // Set instance name (using host_name)
        string = otSrpClientBuffersGetServiceEntryInstanceNameString(entry, &size);
        if (this->host_name.size() > size) {
            ESP_LOGW("OT SRP", "Instance name too long: %s", this->host_name.c_str());
            continue;
        }
        memcpy(string, this->host_name.c_str(), this->host_name.size() + 1);

        // Set port
        entry->mService.mPort = service.port;

        otDnsTxtEntry *mTxtEntries = reinterpret_cast<otDnsTxtEntry*>(this->pool_alloc(sizeof(otDnsTxtEntry) * service.txt_records.size()));
        // Set TXT records
        entry->mService.mNumTxtEntries = service.txt_records.size();
        for (size_t i = 0; i < service.txt_records.size(); i++) {
            const auto& txt = service.txt_records[i];
            mTxtEntries[i].mKey = txt.key.c_str();
            mTxtEntries[i].mValue = reinterpret_cast<const uint8_t*>(txt.value.c_str());
            mTxtEntries[i].mValueLength = txt.value.size();
        }
        entry->mService.mTxtEntries = mTxtEntries;
        entry->mService.mNumTxtEntries = service.txt_records.size();

        // Add service
        error = otSrpClientAddService(instance, &entry->mService);
        if (error != OT_ERROR_NONE) {
            ESP_LOGW("OT SRP", "Failed to add service: %s", otThreadErrorToString(error));
        }
    }

    otSrpClientEnableAutoStartMode(instance, nullptr, nullptr);
}

void *OpenThreadComponent::pool_alloc(size_t size) {
    uint8_t* ptr = new uint8_t[size];
    if (ptr) {
        this->_memory_pool.emplace_back(std::unique_ptr<uint8_t[]>(ptr));
    }
    return ptr;
}

void OpenThreadComponent::set_host_name(std::string host_name){
    this->host_name = host_name;
}

void OpenThreadComponent::set_mdns(esphome::mdns::MDNSComponent *mdns) {
    this->mdns_ = mdns;
}



}  // namespace openthread
}  // namespace esphome
