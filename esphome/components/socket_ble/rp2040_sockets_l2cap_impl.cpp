#include "esphome/core/defines.h"

#ifdef USE_RP2040
#include "rp2040_sockets_l2cap_impl.h"
#include "socket_ble.h"
extern "C" {
#include "btstack.h"
}
#include "esphome/core/log.h"
#include <pico/cyw43_arch.h>
#include <cstring>

namespace esphome::socket_ble {
static const char *const TAG = "socket_ble.rp2040";

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static RP2040BleL2capImpl *channels[SOCKET_BLE_COUNT] = {};
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static RP2040BleL2capListenImpl *listeners[SOCKET_BLE_LISTEN_COUNT] = {};

class AsyncContextLock {
 public:
  AsyncContextLock() { async_context_acquire_lock_blocking(cyw43_arch_async_context()); }

  ~AsyncContextLock() { async_context_release_lock(cyw43_arch_async_context()); }

  AsyncContextLock(const AsyncContextLock &) = delete;
  AsyncContextLock &operator=(const AsyncContextLock &) = delete;
};

static RP2040BleL2capImpl *find_channel(uint16_t cid) {
  for (auto *ch : channels) {
    if (ch && ch->local_cid() == cid)
      return ch;
  }
  return nullptr;
}

static bool register_channel(RP2040BleL2capImpl *ch) {
  for (auto &slot : channels) {
    if (!slot) {
      slot = ch;
      return true;
    }
  }
  return false;
}

static void unregister_channel(RP2040BleL2capImpl *ch) {
  for (auto &slot : channels) {
    if (slot == ch) {
      slot = nullptr;
      return;
    }
  }
}

static RP2040BleL2capListenImpl *find_listener(uint16_t psm) {
  for (auto *listener : listeners) {
    if (listener && listener->psm() == psm)
      return listener;
  }
  return nullptr;
}

static bool register_listener(RP2040BleL2capListenImpl *listener) {
  for (auto &slot : listeners) {
    if (!slot) {
      slot = listener;
      return true;
    }
  }
  return false;
}

static void unregister_listener(RP2040BleL2capListenImpl *listener) {
  for (auto &slot : listeners) {
    if (slot == listener) {
      slot = nullptr;
      return;
    }
  }
}

int RP2040BleL2capListenImpl::bind(const sockaddr_l2 *addr, socklen_t) {
  AsyncContextLock lock;
  if (bound_)
    return -1;

  static bool packet_handler_registered = false;
  if (!packet_handler_registered) {
    static btstack_packet_callback_registration_t l2cap_event_callback_registration;
    l2cap_event_callback_registration.callback = &packet_handler;
    l2cap_add_event_handler(&l2cap_event_callback_registration);
    packet_handler_registered = true;
  }

  psm_ = addr->l2_psm;
  ESP_LOGD(TAG, "Binding L2CAP server to PSM 0x%04X", psm_);
  bool success = register_listener(this);
  if (!success) {
    ESP_LOGE(TAG, "Failed to register new listener, too many listeners open");
    errno = ENOMEM;
    return -1;
  }

  int err = l2cap_cbm_register_service(packet_handler, psm_, LEVEL_1);
  if (err) {
    ESP_LOGE(TAG, "l2cap_cbm_register_service() failed: %d", err);
    unregister_listener(this);
    errno = EINVAL;
    return -1;
  }

  bound_ = true;
  return 0;
}

int RP2040BleL2capListenImpl::close() {
  AsyncContextLock lock;
  if (!bound_)
    return 0;

  bound_ = false;
  unregister_listener(this);

  for (auto &pending_channel_entry : this->pending_channels_) {
    l2cap_decline_connection(pending_channel_entry.cid);
  }
  this->pending_channels_.clear();

  l2cap_cbm_unregister_service(psm_);

  return 0;
}

std::unique_ptr<RP2040BleL2capImpl> RP2040BleL2capListenImpl::accept(sockaddr_l2 *addr, socklen_t *addrlen) {
  AsyncContextLock lock;

  if (this->pending_channels_.empty())
    return {};

  struct pending_channel_entry incoming_channel = this->pending_channels_.front();
  this->pending_channels_.pop();

  std::unique_ptr<RP2040BleL2capImpl> impl = std::make_unique<RP2040BleL2capImpl>();
  impl->local_cid_ = incoming_channel.cid;
  memcpy(impl->peer_addr_, incoming_channel.address, sizeof(bdaddr_t));
  bool success = register_channel(impl.get());
  if (!success) {
    ESP_LOGE(TAG, "Failed to register new channel, too many channels open");
    l2cap_decline_connection(incoming_channel.cid);
    return {};
  }
  // Max credits must be one to allow sharing of receive_sdu_buffer_ ownership between this implementation and btstack
  int err = l2cap_cbm_accept_connection(incoming_channel.cid, impl->receive_sdu_buffer_, SOCKET_BLE_MTU, 1);
  if (err) {
    ESP_LOGE(TAG, "l2cap_cbm_accept_connection() failed: %d", err);
    unregister_channel(impl.get());
    l2cap_decline_connection(incoming_channel.cid);
    return {};
  }

  if (addr != nullptr && addrlen != nullptr) {
    impl->getpeername(addr, addrlen);
  }

  return impl;
}

void RP2040BleL2capListenImpl::packet_handler(uint8_t packet_type,
                                              uint16_t channel,  // local_cid
                                              uint8_t *packet, uint16_t size) {
  if (packet_type == L2CAP_DATA_PACKET) {
    auto *impl = find_channel(channel);
    if (!impl) {
      ESP_LOGE(TAG, "Received L2CAP data packet for unknown channel 0x%04X", channel);
      return;
    }
    if (impl->receive_read_available_) {
      ESP_LOGE(TAG, "Received new packet while incoming packet isn't processed yet");
      return;
    }
    impl->receive_read_available_ = size;
    // We received a full packet, as we don't grant multiple credit to the peer at all we don't have to expect any
    // further incoming packets until we call l2cap_cbm_provide_credits and are also able to borrow exclusive ownership
    // of receive_sdu_buffer_ in the meantime
    return;
  }

  if (packet_type != HCI_EVENT_PACKET)
    return;

  switch (hci_event_packet_get_type(packet)) {
    case L2CAP_EVENT_CBM_INCOMING_CONNECTION: {
      uint16_t psm = l2cap_event_cbm_incoming_connection_get_psm(packet);
      struct pending_channel_entry incoming_channel;
      incoming_channel.cid = l2cap_event_cbm_incoming_connection_get_local_cid(packet);
      l2cap_event_cbm_incoming_connection_get_address(packet, incoming_channel.address);
      auto *listener = find_listener(psm);
      if (!listener) {
        ESP_LOGW(TAG, "Received L2CAP connection for unbound server, rejecting");
        l2cap_decline_connection(incoming_channel.cid);
        return;
      }

      if (listener->pending_channels_.size() >= MAX_PENDING_CHANNELS) {
        ESP_LOGW(TAG, "Max pending channels reached, rejecting");
        l2cap_decline_connection(incoming_channel.cid);
        return;
      }
      ESP_LOGD(TAG, "Incoming L2CAP connection on PSM 0x%04X, CID 0x%04X", psm, incoming_channel.cid);
      listener->pending_channels_.push(incoming_channel);
      break;
    }
    case L2CAP_EVENT_PACKET_SENT: {
      auto *impl = find_channel(channel);
      if (!impl)
        return;

      impl->send_sdu_buffer_sending_ = false;
      break;
    }
    case L2CAP_EVENT_CHANNEL_CLOSED: {
      auto *impl = find_channel(channel);

      if (impl) {
        ESP_LOGD(TAG, "L2CAP channel closed, CID 0x%04X", impl->local_cid_);
        impl->closed_ = true;
        unregister_channel(impl);
      }

      break;
    }
  }
}

ssize_t RP2040BleL2capImpl::read(void *buf, size_t len) {
  AsyncContextLock lock;

  if (this->closed_) {
    errno = EPIPE;
    return -1;
  }

  if (this->receive_read_available_ == 0) {
    // Receive buffer is empty
    errno = EWOULDBLOCK;
    return -1;
  }

  size_t n = std::min(len, this->receive_read_available_);

  memcpy(buf, &this->receive_sdu_buffer_[this->receive_read_pos_], n);
  this->receive_read_available_ -= n;

  if (this->receive_read_available_ == 0) {
    this->receive_read_pos_ = 0;
    // Grant one credit to the peer, this implicitly passes the exclusive ownership of receive_sdu_buffer_ back to
    // btstack which will use the buffer for the next incoming packet again.
    l2cap_cbm_provide_credits(this->local_cid_, 1);
  } else {
    this->receive_read_pos_ += n;
  }
  return n;
}

ssize_t RP2040BleL2capImpl::write(const void *buf, size_t len) {
  AsyncContextLock lock;

  if (this->closed_) {
    errno = EPIPE;
    return -1;
  }

  if (this->send_sdu_buffer_sending_) {
    errno = EWOULDBLOCK;
    return -1;
  }

  size_t to_copy = std::min(len, static_cast<size_t>(SOCKET_BLE_MTU));
  memcpy(this->send_sdu_buffer_, buf, to_copy);
  this->send_sdu_buffer_sending_ = true;

  int err = l2cap_send(this->local_cid_, this->send_sdu_buffer_, to_copy);

  if (err) {
    ESP_LOGE(TAG, "l2cap_send() failed: %d", err);
    this->send_sdu_buffer_sending_ = false;
    errno = EIO;
    return -1;
  }
  return to_copy;
}

ssize_t RP2040BleL2capImpl::writev(const struct iovec *iov, int iovcnt) {
  AsyncContextLock lock;

  if (this->closed_) {
    errno = EPIPE;
    return -1;
  }

  if (this->send_sdu_buffer_sending_) {
    errno = EWOULDBLOCK;
    return -1;
  }
  size_t total_copied = 0;
  for (int i = 0; i < iovcnt; i++) {
    size_t to_copy = std::min(iov[i].iov_len, SOCKET_BLE_MTU - total_copied);
    if (to_copy == 0) {
      break;
    }
    memcpy(&this->send_sdu_buffer_[total_copied], iov[i].iov_base, to_copy);
    total_copied += to_copy;
  }
  this->send_sdu_buffer_sending_ = true;

  int err = l2cap_send(this->local_cid_, this->send_sdu_buffer_, total_copied);

  if (err) {
    ESP_LOGE(TAG, "l2cap_send() failed: %d", err);
    this->send_sdu_buffer_sending_ = false;
    errno = EIO;
    return -1;
  }
  return total_copied;
}

int RP2040BleL2capImpl::close() {
  AsyncContextLock lock;

  if (closed_)
    return 0;

  closed_ = true;
  // xxx we don't call l2cap_disconnect here, but instead rely on the peer to close
  // the channel first. This is a workaround for a btstack bug, where the source_cid is not freed preventing the same
  // client to reconnect.
  unregister_channel(this);
  ESP_LOGD(TAG, "User closing L2CAP channel, CID 0x%04X", this->local_cid_);

  return 0;
}

size_t RP2040BleL2capImpl::getpeername_to(std::span<char, BDADDR_STR_LEN> buf) {
  format_bdaddr_to(this->peer_addr_, buf);
  return BDADDR_STR_LEN;
}

int RP2040BleL2capImpl::getpeername(sockaddr_l2 *addr, socklen_t *addrlen) {
  *addrlen = sizeof(sockaddr_l2);
  memset(addr, 0, sizeof(sockaddr_l2));
  addr->l2_family = AF_BLUETOOTH;
  memcpy(&addr->l2_bdaddr, this->peer_addr_, sizeof(bd_addr_t));
  return 0;
}

std::unique_ptr<RP2040BleL2capListenImpl> socket_ble_listen_loop_monitored(int domain, int type, int protocol) {
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

  return std::make_unique<RP2040BleL2capListenImpl>();
}

}  // namespace esphome::socket_ble

#endif  // USE_RP2040
