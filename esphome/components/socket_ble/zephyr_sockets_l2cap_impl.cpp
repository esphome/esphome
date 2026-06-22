#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "socket_ble.h"

#ifdef USE_ZEPHYR
#include "zephyr_sockets_l2cap_impl.h"

#include <cstdio>
#include <zephyr/bluetooth/l2cap.h>
#include "esphome/core/log.h"
#include "esphome/core/application.h"

namespace esphome::socket_ble {

static const char *const TAG = "socket_ble.zephyr";

K_MUTEX_DEFINE(mutex);  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

class Lock {
 public:
  Lock() { k_mutex_lock(&mutex, K_FOREVER); }

  ~Lock() { k_mutex_unlock(&mutex); }

  Lock(const Lock &) = delete;
  Lock &operator=(const Lock &) = delete;
};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static std::array<ZephyrBleL2capListenImpl, SOCKET_BLE_LISTEN_COUNT> listen_sockets_{};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
NET_BUF_POOL_FIXED_DEFINE(send_data_pool, SOCKET_BLE_COUNT, BT_L2CAP_SDU_BUF_SIZE(SOCKET_BLE_MTU),
                          CONFIG_BT_CONN_TX_USER_DATA_SIZE, NULL);
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
NET_BUF_POOL_FIXED_DEFINE(recv_data_pool, SOCKET_BLE_COUNT, SOCKET_BLE_MTU, 8, NULL);

net_buf *ZephyrBleL2capImpl::alloc_buf_cb(struct bt_l2cap_chan *chan) {
  // Blocking (K_FOREVER) until a buffer is available is not supported by fixed pools, so we retry after a short delay
  // if the first allocation fails.
  net_buf *b = net_buf_alloc(&recv_data_pool, K_FOREVER);
  if (!b) {
    k_msleep(10);
    b = net_buf_alloc(&recv_data_pool, K_FOREVER);
    if (!b) {
      ESP_LOGE(TAG, "Failed to allocate net_buf for L2CAP channel");
    }
  }
  return b;
}

void ZephyrBleL2capImpl::sent_cb(struct bt_l2cap_chan *chan) {
  Lock lock;
  struct bt_l2cap_le_chan *le_chan = BT_L2CAP_LE_CHAN(chan);
  ZephyrBleL2capImpl *impl = CONTAINER_OF(le_chan, ZephyrBleL2capImpl, le_chan_);
  if (impl->wake_after_sent_) {
    impl->wake_after_sent_ = false;
    App.wake_loop_threadsafe();
  }
}

int ZephyrBleL2capImpl::recv_cb(struct bt_l2cap_chan *chan, struct net_buf *buf) {
  Lock lock;
  struct bt_l2cap_le_chan *le_chan = BT_L2CAP_LE_CHAN(chan);
  ZephyrBleL2capImpl *impl = CONTAINER_OF(le_chan, ZephyrBleL2capImpl, le_chan_);
  net_buf_slist_put(&impl->queued_rx_bufs_, buf);
  if (impl->wake_after_recv_) {
    impl->wake_after_recv_ = false;
    App.wake_loop_threadsafe();
  }
  return -EINPROGRESS;
}

void ZephyrBleL2capImpl::connected_cb(struct bt_l2cap_chan *chan) { ESP_LOGD(TAG, "L2CAP channel connected"); }

void ZephyrBleL2capImpl::disconnected_cb(struct bt_l2cap_chan *chan) {
  struct bt_l2cap_le_chan *le_chan = BT_L2CAP_LE_CHAN(chan);
  ZephyrBleL2capImpl *impl = CONTAINER_OF(le_chan, ZephyrBleL2capImpl, le_chan_);

  {
    Lock lock;
    if (impl->closed_) {
      return;
    }
    impl->closed_ = true;

    ESP_LOGD(TAG, "L2CAP channel disconnected");

    if (impl->rx_buf_ != nullptr) {
      bt_l2cap_chan_recv_complete(&impl->le_chan_.chan, impl->rx_buf_);
      impl->rx_buf_ = nullptr;
    }
    struct net_buf *rx_buf;
    while ((rx_buf = net_buf_slist_get(&impl->queued_rx_bufs_)) != nullptr) {
      bt_l2cap_chan_recv_complete(&impl->le_chan_.chan, rx_buf);
    }
  }

  App.wake_loop_threadsafe();
}

void ZephyrBleL2capImpl::released_cb(struct bt_l2cap_chan *chan) {
  struct bt_l2cap_le_chan *le_chan = BT_L2CAP_LE_CHAN(chan);
  ZephyrBleL2capImpl *impl = CONTAINER_OF(le_chan, ZephyrBleL2capImpl, le_chan_);

  Lock lock;
  impl->closed_ = true;
  bool free_impl = !impl->referenced_;
  if (free_impl) {
    ::operator delete(impl);
  } else {
    impl->released_ = true;
  }
}

// NOLINTNEXTLINE(cert-dcl54-cpp,misc-new-delete-overloads)
void ZephyrBleL2capImpl::operator delete(void *ptr) {
  ZephyrBleL2capImpl *impl = static_cast<ZephyrBleL2capImpl *>(ptr);
  Lock lock;
  if (!impl->closed_) {
    impl->close();
  }
  bool free_impl = impl->released_;
  if (free_impl) {
    ::operator delete(ptr);
  } else {
    impl->referenced_ = false;
  }
}

int ZephyrBleL2capListenImpl::bind(const struct sockaddr_l2 *name, socklen_t addrlen) {
  if (this->bound_) {
    errno = EINVAL;
    return -1;
  }

  struct bt_l2cap_server *server = &this->server_;
  server->psm = name->l2_psm;
  server->sec_level = BT_SECURITY_L1;
  server->accept = ZephyrBleL2capListenImpl::accept_cb;
  int err = bt_l2cap_server_register(server);
  if (err < 0) {
    ESP_LOGE(TAG, "bt_l2cap_server_register() failed: %d", err);
    errno = EINVAL;
    return -1;
  }
  this->bound_ = true;
  return 0;
}

int ZephyrBleL2capListenImpl::accept_cb(struct bt_conn *conn, struct bt_l2cap_server *server,
                                        struct bt_l2cap_chan **chan) {
  {
    Lock lock;
    ZephyrBleL2capListenImpl *listen_impl = CONTAINER_OF(server, ZephyrBleL2capListenImpl, server_);
    if (!listen_impl->bound_) {
      ESP_LOGW(TAG, "Received L2CAP connection for unbound server, rejecting");
      return -EINVAL;
    }
    if (listen_impl->pending_channels_.size() >= ZephyrBleL2capListenImpl::MAX_PENDING_CHANNELS) {
      ESP_LOGW(TAG, "Max pending channels reached, rejecting");
      return -ENOMEM;
    }
    std::unique_ptr<ZephyrBleL2capImpl> impl = std::make_unique<ZephyrBleL2capImpl>();

    impl->le_chan_.chan.ops = &ZephyrBleL2capImpl::OPS;
    impl->le_chan_.rx.mtu = SOCKET_BLE_MTU;
    impl->le_chan_.tx.mtu = SOCKET_BLE_MTU;
    *chan = &impl->le_chan_.chan;
    listen_impl->pending_channels_.push(std::move(impl));
    ESP_LOGD(TAG, "Accepted new connection as pending");
  }
  App.wake_loop_threadsafe();

  return 0;
}

std::unique_ptr<ZephyrBleL2capImpl> ZephyrBleL2capListenImpl::accept(struct sockaddr_l2 *addr, socklen_t *addrlen) {
  Lock lock;
  std::unique_ptr<ZephyrBleL2capImpl> impl;
  do {
    if (this->pending_channels_.empty()) {
      errno = EWOULDBLOCK;
      return nullptr;
    }
    impl = std::move(this->pending_channels_.front());
    this->pending_channels_.pop();
  } while (impl->closed_);

  if (addr != nullptr && addrlen != nullptr) {
    impl->getpeername(addr, addrlen);
  }
  return impl;
}

int ZephyrBleL2capListenImpl::close() {
  Lock lock;
  // Zephyr doesn't provide any API to unregister a ble l2cap server. We therefore just mark the listen socket as
  // unbound and close all accepted channels.
  ESP_LOGW(TAG, "L2CAP listen sockets can't be closed dynamically");
  this->bound_ = false;
  for (auto &impl_iter : this->pending_channels_) {
    std::unique_ptr<ZephyrBleL2capImpl> impl = std::move(impl_iter);
    if (impl) {
      impl->close();
    }
  }
  this->pending_channels_.clear();
  return 0;
}

// NOLINTNEXTLINE(cert-dcl54-cpp,misc-new-delete-overloads)
void ZephyrBleL2capListenImpl::operator delete(void *ptr) {
  // no-op for preventing unique_ptr deletion, listen sockets are statically allocated to keep the Zephyr L2CAP server
  // alive for the lifetime of the application.
}

bool ZephyrBleL2capImpl::ready() {
  Lock lock;
  return this->rx_buf_ != nullptr || !sys_slist_is_empty(&this->queued_rx_bufs_) || this->closed_;
}

ssize_t ZephyrBleL2capImpl::read(void *buf, size_t len) {
  Lock lock;
  if (this->released_ || this->closed_) {
    errno = EPIPE;
    return -1;
  }
  if (this->rx_buf_ == nullptr) {
    this->rx_buf_ = net_buf_slist_get(&this->queued_rx_bufs_);
    if (this->rx_buf_ == nullptr) {
      // No data available: mark to be woken when data arrives.
      this->wake_after_recv_ = true;
      errno = EWOULDBLOCK;
      return -1;
    }
  }
  net_buf *rx_buf = this->rx_buf_;
  // Copy at most `len` bytes from the pending net_buf into the caller buffer.
  size_t to_copy = std::min(len, static_cast<size_t>(rx_buf->len));
  /* net_buf_pull copies `to_copy` bytes into `buf` and removes them from the net_buf. */
  memcpy(buf, net_buf_pull_mem(rx_buf, to_copy), to_copy);

  if (rx_buf->len == 0) {
    this->rx_buf_ = nullptr;
    bt_l2cap_chan_recv_complete(&this->le_chan_.chan, rx_buf);
  }
  return static_cast<ssize_t>(to_copy);
}

ssize_t ZephyrBleL2capImpl::write(const void *buf, size_t len) {
  struct net_buf *tx_buf;
  {
    Lock lock;
    if (this->closed_) {
      errno = EPIPE;
      return -1;
    }
    tx_buf = net_buf_alloc(&send_data_pool, K_NO_WAIT);
    if (!tx_buf) {
      errno = EWOULDBLOCK;
      this->wake_after_sent_ = true;
      return -1;
    }
  }
  size_t to_copy = std::min({len, static_cast<size_t>(this->le_chan_.tx.mtu), static_cast<size_t>(SOCKET_BLE_MTU)});
  net_buf_reserve(tx_buf, BT_L2CAP_SDU_CHAN_SEND_RESERVE);
  net_buf_add_mem(tx_buf, buf, to_copy);
  int err = bt_l2cap_chan_send(&this->le_chan_.chan, tx_buf);
  if (err < 0) {
    net_buf_unref(tx_buf);
    ESP_LOGE(TAG, "bt_l2cap_chan_send() failed: %d", err);
    if (err == -ENOBUFS) {
      errno = EWOULDBLOCK;
    } else {
      errno = EIO;
    }
    return -1;
  }
  return to_copy;
}

ssize_t ZephyrBleL2capImpl::writev(const struct iovec *iov, int iovcnt) {
  struct net_buf *tx_buf;
  {
    Lock lock;
    if (this->closed_) {
      errno = EPIPE;
      return -1;
    }
    tx_buf = net_buf_alloc(&send_data_pool, K_NO_WAIT);
    if (!tx_buf) {
      errno = EWOULDBLOCK;
      this->wake_after_sent_ = true;
      return -1;
    }
  }
  net_buf_reserve(tx_buf, BT_L2CAP_SDU_CHAN_SEND_RESERVE);
  size_t available_space = std::min(static_cast<size_t>(this->le_chan_.tx.mtu), static_cast<size_t>(SOCKET_BLE_MTU));
  size_t total_copied = 0;
  for (int i = 0; i < iovcnt; i++) {
    size_t to_copy = std::min(iov[i].iov_len, available_space);
    net_buf_add_mem(tx_buf, iov[i].iov_base, to_copy);
    available_space -= to_copy;
    total_copied += to_copy;
    if (available_space == 0) {
      break;
    }
  }

  int err = bt_l2cap_chan_send(&this->le_chan_.chan, tx_buf);
  if (err < 0) {
    net_buf_unref(tx_buf);
    ESP_LOGE(TAG, "bt_l2cap_chan_send() failed: %d", err);
    if (err == -ENOBUFS) {
      errno = EWOULDBLOCK;
    } else {
      errno = EIO;
    }
    return -1;
  }
  return total_copied;
}

int ZephyrBleL2capImpl::close() {
  Lock lock;
  if (this->closed_) {
    return 0;
  }
  this->closed_ = true;

  ESP_LOGD(TAG, "User closing L2CAP channel");
  bt_l2cap_chan_disconnect(&this->le_chan_.chan);

  struct net_buf *rx_buf = this->rx_buf_;
  if (rx_buf != nullptr) {
    this->rx_buf_ = nullptr;
    bt_l2cap_chan_recv_complete(&this->le_chan_.chan, rx_buf);
  }
  while ((rx_buf = net_buf_slist_get(&this->queued_rx_bufs_)) != nullptr) {
    bt_l2cap_chan_recv_complete(&this->le_chan_.chan, rx_buf);
  }
  return 0;
}

size_t ZephyrBleL2capImpl::getpeername_to(std::span<char, BDADDR_STR_LEN> buf) {
  const bt_addr_le_t *le_addr = bt_conn_get_dst((const bt_conn *) this->le_chan_.chan.conn);
  return format_bdaddr_to(le_addr->a.val, buf);
}

int ZephyrBleL2capImpl::getpeername(struct sockaddr_l2 *addr, socklen_t *addrlen) {
  *addrlen = sizeof(struct sockaddr_l2);
  memset(addr, 0, sizeof(sockaddr_l2));
  addr->l2_family = AF_BLUETOOTH;
  addr->l2_psm = this->le_chan_.psm;

  const bt_addr_le_t *le_addr = bt_conn_get_dst((const bt_conn *) this->le_chan_.chan.conn);
  memcpy(&addr->l2_bdaddr, le_addr->a.val, sizeof(bdaddr_t));
  addr->l2_bdaddr_type = le_addr->type;
  return 0;
}

std::unique_ptr<ZephyrBleL2capListenImpl> socket_ble_listen_loop_monitored(int domain, int type, int protocol) {
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
      return std::unique_ptr<ZephyrBleL2capListenImpl>{&socket};
    }
  }
  ESP_LOGE(TAG, "No more available listen sockets");
  errno = ENOMEM;
  return nullptr;
}
}  // namespace esphome::socket_ble
#endif  // USE_ZEPHYR
