#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "socket_ble.h"

#if defined(USE_ESP32) && defined(CONFIG_BT_NIMBLE_ENABLED)
#include "esp32_sockets_l2cap_impl.h"

#include "host/ble_hs_adv.h"
#include "host/ble_gap.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include <nvs_flash.h>

#include <cstdio>
#include "esphome/core/log.h"
#include "esphome/core/application.h"

namespace esphome::socket_ble {

static const char *const TAG = "socket_ble.esp32";

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static std::array<ESP32BleL2capListenImpl, SOCKET_BLE_LISTEN_COUNT> listen_sockets_{};

// Each socket: one buffer for rx and one buffer for tx.
static constexpr int COC_BUF_COUNT = (2 * SOCKET_BLE_COUNT);
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static os_membuf_t sdu_coc_mem[OS_MEMPOOL_SIZE(COC_BUF_COUNT, SOCKET_BLE_MTU)];
static struct os_mempool sdu_coc_mbuf_mempool;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static struct os_mbuf_pool sdu_os_mbuf_pool;    // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

static SemaphoreHandle_t mutex = NULL;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

static EventGroupHandle_t event_group = NULL;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static constexpr EventBits_t EVENT_BLE_SYNCED = (1 << 0);

static uint8_t own_addr_type;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

void esp32_sockets_l2cap_init_host_task(void *param) {
  // This function will return only when nimble_port_stop() is executed
  nimble_port_run();

  nimble_port_freertos_deinit();
}

static int esp32_sockets_l2cap_gap_event(struct ble_gap_event *event, void *arg);

static constexpr ble_uuid128_t API_SERVICE_UUID =
    BLE_UUID128_INIT(0x3e, 0x80, 0x39, 0x53, 0x54, 0x45, 0x64, 0x89, 0x44, 0x44, 0x9c, 0x1c, 0x8b, 0x0d, 0x1b, 0xe5);

static void esp32_sockets_l2cap_advertise() {
  // Limit the device name to 8 bytes to fit into a 31 byte advertisement packet.
  size_t max_name_len = 8;
  size_t name_len = sizeof(BLE_DEVICE_NAME) - 1;
  struct ble_hs_adv_fields fields = {
      //  Discoverability in forthcoming advertisement & BLE-only (BR/EDR unsupported).
      .flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP,

      .uuids128 = &API_SERVICE_UUID,
      .num_uuids128 = 1,
      .uuids128_is_complete = 1,

      .name = (uint8_t *) BLE_DEVICE_NAME,
      .name_len = (uint8_t) (name_len <= max_name_len ? name_len : max_name_len),
      .name_is_complete = name_len <= max_name_len,
  };

  int err = ble_gap_adv_set_fields(&fields);
  if (err) {
    ESP_LOGE(TAG, "ble_gap_adv_set_fields() failed: %d", err);
    return;
  }

  struct ble_gap_adv_params adv_params = {
      .conn_mode = BLE_GAP_CONN_MODE_UND,
      .disc_mode = BLE_GAP_DISC_MODE_GEN,
  };
  err = ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER, &adv_params, esp32_sockets_l2cap_gap_event, NULL);
  if (err) {
    ESP_LOGE(TAG, "ble_gap_adv_start() failed: %d", err);
    return;
  }
}

static void esp32_sockets_l2cap_on_reset(int reason) { ESP_LOGE(TAG, "Resetting state; reason=%d", reason); }

static void esp32_sockets_l2cap_on_sync() {
  int err = ble_hs_util_ensure_addr(0);
  if (err) {
    ESP_LOGE(TAG, "ble_hs_util_ensure_addr() failed: %d", err);
    return;
  }

  err = ble_hs_id_infer_auto(0, &own_addr_type);
  if (err) {
    ESP_LOGE(TAG, "ble_hs_id_infer_auto() failed: %d", err);
    return;
  }

  xEventGroupSetBits(event_group, EVENT_BLE_SYNCED);

  // Begin advertising
  esp32_sockets_l2cap_advertise();
}

static int esp32_sockets_l2cap_gap_event(struct ble_gap_event *event, void *arg) {
  switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
      // A new connection was established or a connection attempt failed.
      ESP_LOGD(TAG, "Connection %s; status=%d", event->connect.status == 0 ? "established" : "failed",
               event->connect.status);
      if (event->connect.status) {
        esp32_sockets_l2cap_advertise();
      }
      return 0;

    case BLE_GAP_EVENT_DISCONNECT:
      ESP_LOGD(TAG, "Disconnect; reason=%d", event->disconnect.reason);
      // Connection terminated; resume advertising.
      esp32_sockets_l2cap_advertise();
      return 0;

    case BLE_GAP_EVENT_ADV_COMPLETE:
      ESP_LOGD(TAG, "Advertise complete; reason=%d", event->adv_complete.reason);
      esp32_sockets_l2cap_advertise();
      return 0;

    default:
      return 0;
  }
}

void esp32_sockets_l2cap_init() {
  static StaticSemaphore_t mutex_buffer;
  mutex = xSemaphoreCreateRecursiveMutexStatic(&mutex_buffer);
  static StaticEventGroup_t event_group_buffer;
  event_group = xEventGroupCreateStatic(&event_group_buffer);

  int err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
  }
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "nvs_flash_init() failed: %s", esp_err_to_name(err));
    return;
  }

  err = nimble_port_init();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "nimble_port_init() failed: %d", err);
    return;
  }

  err = os_mempool_init(&sdu_coc_mbuf_mempool, COC_BUF_COUNT, SOCKET_BLE_MTU, sdu_coc_mem, "sdu_coc_mbuf_pool");
  if (err) {
    ESP_LOGE(TAG, "os_mempool_init() failed: %d", err);
    return;
  }
  err = os_mbuf_pool_init(&sdu_os_mbuf_pool, &sdu_coc_mbuf_mempool, SOCKET_BLE_MTU, COC_BUF_COUNT);
  if (err) {
    ESP_LOGE(TAG, "os_mbuf_pool_init() failed: %d", err);
    return;
  }

  ble_hs_cfg.reset_cb = esp32_sockets_l2cap_on_reset;
  ble_hs_cfg.sync_cb = esp32_sockets_l2cap_on_sync;

  ble_hs_cfg.sm_io_cap = BLE_SM_IO_CAP_NO_IO;

  err = ble_svc_gap_device_name_set(BLE_DEVICE_NAME);
  assert(err == 0);

  nimble_port_freertos_init(esp32_sockets_l2cap_init_host_task);
}

class Lock {
 public:
  Lock() { xSemaphoreTakeRecursive(mutex, portMAX_DELAY); }

  ~Lock() { xSemaphoreGiveRecursive(mutex); }

  Lock(const Lock &) = delete;
  Lock &operator=(const Lock &) = delete;
};

int ESP32BleL2capListenImpl::event_cb(struct ble_l2cap_event *event, void *arg) {
  Lock lock;
  ESP32BleL2capListenImpl *listen_impl = static_cast<ESP32BleL2capListenImpl *>(arg);
  switch (event->type) {
    case BLE_L2CAP_EVENT_COC_ACCEPT: {
      struct ble_l2cap_chan *chan = event->accept.chan;
      size_t mtu = event->accept.peer_sdu_size;
      ESP32BleL2capImpl **impl_slot = nullptr;
      for (int i = 0; i < SOCKET_BLE_COUNT; i++) {
        if (listen_impl->open_channels_[i] == nullptr) {
          impl_slot = &listen_impl->open_channels_[i];
          break;
        }
      }
      if (impl_slot == nullptr) {
        ESP_LOGW(TAG, "Can't accept new L2CAP connection. No free slots");
        ble_l2cap_disconnect(chan);
      } else {
        ESP32BleL2capImpl *impl = *impl_slot = new ESP32BleL2capImpl(impl_slot, chan, mtu);
        listen_impl->pending_channel_count_++;
        ESP_LOGD(TAG, "Accepted new L2CAP connection");
        impl->sdu_rx_data_ = os_mbuf_get_pkthdr(&sdu_os_mbuf_pool, 0);
        if (impl->sdu_rx_data_ != nullptr) {
          impl->receive_in_progress_ = true;
          ble_l2cap_recv_ready(impl->chan_, impl->sdu_rx_data_);
        }
      }

      return 0;
    }
    case BLE_L2CAP_EVENT_COC_DISCONNECTED: {
      struct ble_l2cap_chan *chan = event->disconnect.chan;
      for (auto impl : listen_impl->open_channels_) {
        if (impl != nullptr && impl->chan_ == chan) {
          impl->closed_ = true;
          if (!impl->accepted_) {
            listen_impl->pending_channel_count_--;
            delete impl;
          }
          // Destructor will remove the impl from the open_channels_ array.
          break;
        }
      }
      return 0;
    }
    case BLE_L2CAP_EVENT_COC_DATA_RECEIVED: {
      struct ble_l2cap_chan *chan = event->receive.chan;
      struct os_mbuf *sdu_rx_data = event->receive.sdu_rx;
      for (auto *impl : listen_impl->open_channels_) {
        if (impl != nullptr && impl->chan_ == chan) {
          impl->receive_in_progress_ = false;
          if (impl->wake_after_recv_) {
            impl->wake_after_recv_ = false;
            App.wake_loop_threadsafe();
          }
          return 0;
        }
      }
      ESP_LOGW(TAG, "Received data on unknown channel %p", chan);
      return -1;
    }
    case BLE_L2CAP_EVENT_COC_TX_UNSTALLED: {
      struct ble_l2cap_chan *chan = event->tx_unstalled.chan;
      for (auto *impl : listen_impl->open_channels_) {
        if (impl != nullptr && impl->chan_ == chan) {
          impl->stalled_ = false;
          if (impl->wake_after_sent_) {
            impl->wake_after_sent_ = false;
            App.wake_loop_threadsafe();
          }
          return 0;
        }
      }
      ESP_LOGW(TAG, "TX unstalled on unknown channel %p", chan);
      return -1;
    }
    default:
      return 0;
  }
}

int ESP32BleL2capListenImpl::bind(const struct sockaddr_l2 *name, socklen_t addrlen) {
  if (this->bound_) {
    errno = EINVAL;
    return -1;
  }

  static bool initialized = false;
  if (!initialized) {
    initialized = true;
    esp32_sockets_l2cap_init();
  }
  EventBits_t active_event_bits =
      xEventGroupWaitBits(event_group, EVENT_BLE_SYNCED, pdFALSE, pdFALSE, 2500 / portTICK_PERIOD_MS);
  if (active_event_bits == 0) {
    ESP_LOGE(TAG, "Timeout while waiting for BLE to sync");
    errno = EBUSY;
    return -1;
  }

  int err = ble_l2cap_create_server(name->l2_psm, SOCKET_BLE_MTU, &ESP32BleL2capListenImpl::event_cb, (void *) this);
  if (err) {
    ESP_LOGE(TAG, "ble_l2cap_create_server() failed: %d", err);
    errno = EINVAL;
    return -1;
  }
  this->bound_ = true;
  return 0;
}

std::unique_ptr<ESP32BleL2capImpl> ESP32BleL2capListenImpl::accept(struct sockaddr_l2 *addr, socklen_t *addrlen) {
  Lock lock;
  if (this->pending_channel_count_ == 0) {
    errno = EWOULDBLOCK;
    return nullptr;
  }
  ESP32BleL2capImpl *impl = nullptr;
  for (auto *open_impl : this->open_channels_) {
    if (open_impl != nullptr && !open_impl->accepted_ && !open_impl->closed_) {
      impl = open_impl;
      break;
    }
  }
  if (impl == nullptr) {
    errno = EWOULDBLOCK;
    return nullptr;
  }
  impl->accepted_ = true;
  this->pending_channel_count_--;

  if (addr != nullptr && addrlen != nullptr) {
    impl->getpeername(addr, addrlen);
  }
  return std::unique_ptr<ESP32BleL2capImpl>(impl);
}

int ESP32BleL2capListenImpl::close() {
  Lock lock;
  // NimBLE doesn't provide any API to unregister a ble l2cap server. We therefore just mark the listen socket as
  // unbound and close all accepted channels.
  ESP_LOGW(TAG, "L2CAP listen sockets can't be closed dynamically");
  this->bound_ = false;
  for (auto &impl : this->open_channels_) {
    if (impl && !impl->accepted_) {
      impl->close();
    }
  }
  return 0;
}

// NOLINTNEXTLINE(cert-dcl54-cpp,misc-new-delete-overloads)
void ESP32BleL2capListenImpl::operator delete(void *ptr) {
  // no-op for preventing unique_ptr deletion, listen sockets are statically allocated to keep the NimBLE L2CAP server
  // alive for the lifetime of the application.
}

bool ESP32BleL2capImpl::ready() {
  Lock lock;
  return (this->sdu_rx_data_ != nullptr && !this->receive_in_progress_) || this->closed_;
}

ssize_t ESP32BleL2capImpl::read(void *buf, size_t len) {
  Lock lock;
  if (this->closed_) {
    errno = EPIPE;
    return -1;
  }
  if (this->receive_in_progress_) {
    // No data available: mark to be woken when data arrives.
    this->wake_after_recv_ = true;
    errno = EWOULDBLOCK;
    return -1;
  } else if (this->sdu_rx_data_ == nullptr) {
    // No data available and receive buffer not allocated nor marked as ready.
    this->sdu_rx_data_ = os_mbuf_get_pkthdr(&sdu_os_mbuf_pool, 0);
    if (this->sdu_rx_data_ != nullptr) {
      this->receive_in_progress_ = true;
      this->rx_offset_ = 0;
      this->wake_after_recv_ = true;
      ble_l2cap_recv_ready(this->chan_, this->sdu_rx_data_);
    }
    errno = EWOULDBLOCK;
    return -1;
  }
  size_t total_len = os_mbuf_len(this->sdu_rx_data_);
  size_t available_len = total_len - this->rx_offset_;
  size_t to_copy = std::min(len, available_len);
  os_mbuf_copydata(this->sdu_rx_data_, this->rx_offset_, to_copy, buf);
  this->rx_offset_ += to_copy;

  if (this->rx_offset_ >= total_len) {
    os_mbuf_free(this->sdu_rx_data_);
    this->sdu_rx_data_ = os_mbuf_get_pkthdr(&sdu_os_mbuf_pool, 0);
    if (this->sdu_rx_data_ != nullptr) {
      this->receive_in_progress_ = true;
      this->rx_offset_ = 0;
      ble_l2cap_recv_ready(this->chan_, this->sdu_rx_data_);
    }
  }

  return static_cast<ssize_t>(to_copy);
}

ssize_t ESP32BleL2capImpl::write(const void *buf, size_t len) {
  Lock lock;
  if (this->closed_) {
    errno = EPIPE;
    return -1;
  }

  if (this->stalled_) {
    errno = EWOULDBLOCK;
    this->wake_after_sent_ = true;
    return -1;
  }
  struct os_mbuf *sdu_tx_data = os_mbuf_get_pkthdr(&sdu_os_mbuf_pool, 0);
  if (!sdu_tx_data) {
    errno = EWOULDBLOCK;
    this->wake_after_sent_ = true;
    return -1;
  }
  size_t to_copy = std::min(std::min(len, this->mtu_), static_cast<size_t>(SOCKET_BLE_MTU));
  int err = os_mbuf_append(sdu_tx_data, buf, to_copy);
  if (err) {
    ESP_LOGE(TAG, "os_mbuf_append() failed: %d", err);
    os_mbuf_free_chain(sdu_tx_data);
    this->wake_after_sent_ = true;
    errno = EWOULDBLOCK;
    return -1;
  }
  err = ble_l2cap_send(this->chan_, sdu_tx_data);
  if (err == BLE_HS_ESTALLED) {
    this->stalled_ = true;
  } else if (err) {
    os_mbuf_free_chain(sdu_tx_data);
    ESP_LOGE(TAG, "ble_l2cap_send() failed: %d", err);
    errno = EIO;
    return -1;
  }
  return to_copy;
}

ssize_t ESP32BleL2capImpl::writev(const struct iovec *iov, int iovcnt) {
  Lock lock;

  if (this->closed_) {
    errno = EPIPE;
    return -1;
  }

  if (this->stalled_) {
    errno = EWOULDBLOCK;
    this->wake_after_sent_ = true;
    return -1;
  }

  struct os_mbuf *sdu_tx_data = os_mbuf_get_pkthdr(&sdu_os_mbuf_pool, 0);
  if (!sdu_tx_data) {
    errno = EWOULDBLOCK;
    this->wake_after_sent_ = true;
    return -1;
  }
  size_t available_space = std::min(this->mtu_, static_cast<size_t>(SOCKET_BLE_MTU));
  size_t total_copied = 0;
  int err;
  for (int i = 0; i < iovcnt; i++) {
    size_t to_copy = std::min(iov[i].iov_len, available_space);
    err = os_mbuf_append(sdu_tx_data, iov[i].iov_base, to_copy);
    if (err) {
      ESP_LOGE(TAG, "os_mbuf_append() failed: %d", err);
      os_mbuf_free_chain(sdu_tx_data);
      this->wake_after_sent_ = true;
      errno = EWOULDBLOCK;
      return -1;
    }
    available_space -= to_copy;
    total_copied += to_copy;
    if (available_space == 0) {
      break;
    }
  }

  err = ble_l2cap_send(this->chan_, sdu_tx_data);
  if (err == BLE_HS_ESTALLED) {
    this->stalled_ = true;
  } else if (err) {
    ESP_LOGE(TAG, "ble_l2cap_send() failed: %d", err);
    os_mbuf_free_chain(sdu_tx_data);
    errno = EIO;
    return -1;
  }
  return total_copied;
}

int ESP32BleL2capImpl::close() {
  Lock lock;
  if (this->closed_) {
    return 0;
  }
  this->closed_ = true;

  ESP_LOGD(TAG, "User closing L2CAP channel");
  ble_l2cap_disconnect(this->chan_);
  return 0;
}

size_t ESP32BleL2capImpl::getpeername_to(std::span<char, BDADDR_STR_LEN> buf) {
  Lock lock;
  uint16_t conn_handle = ble_l2cap_get_conn_handle(this->chan_);
  struct ble_gap_conn_desc desc;
  int err = ble_gap_conn_find(conn_handle, &desc);
  if (err) {
    ESP_LOGE(TAG, "ble_gap_conn_find() failed: %d", err);
    return 0;
  }
  return format_bdaddr_to(desc.peer_id_addr.val, buf);
}

int ESP32BleL2capImpl::getpeername(struct sockaddr_l2 *addr, socklen_t *addrlen) {
  Lock lock;
  *addrlen = sizeof(struct sockaddr_l2);

  memset(addr, 0, sizeof(sockaddr_l2));
  addr->l2_family = AF_BLUETOOTH;
  uint16_t conn_handle = ble_l2cap_get_conn_handle(this->chan_);
  struct ble_gap_conn_desc desc;
  int err = ble_gap_conn_find(conn_handle, &desc);
  if (err) {
    ESP_LOGE(TAG, "ble_gap_conn_find() failed: %d", err);
    errno = EBADF;
    return -1;
  }

  memcpy(&addr->l2_bdaddr, desc.peer_id_addr.val, sizeof(bdaddr_t));
  addr->l2_bdaddr_type = desc.peer_id_addr.type;
  return 0;
}

std::unique_ptr<ESP32BleL2capListenImpl> socket_ble_listen_loop_monitored(int domain, int type, int protocol) {
  if (domain != AF_BLUETOOTH) {
    ESP_LOGE(TAG, "Address family not supported on this platform, use AF_BLUETOOTH");
    errno = EAFNOSUPPORT;
    return nullptr;
  }
  if (type != SOCK_SEQPACKET) {
    ESP_LOGE(TAG, "Socket type not supported on this platform, use SOCK_SEQPACKET");
    errno = EPROTOTYPE;
    return nullptr;
  }
  if (protocol != BTPROTO_L2CAP) {
    ESP_LOGE(TAG, "Protocol not supported on this platform, use BTPROTO_L2CAP");
    errno = EPROTONOSUPPORT;
    return nullptr;
  }

  for (auto &socket : listen_sockets_) {
    if (!socket.in_use_) {
      socket.in_use_ = true;
      return std::unique_ptr<ESP32BleL2capListenImpl>{&socket};
    }
  }
  ESP_LOGE(TAG, "No more available listen sockets");
  errno = ENOMEM;
  return nullptr;
}
}  // namespace esphome::socket_ble
#endif  // USE_ESP32
