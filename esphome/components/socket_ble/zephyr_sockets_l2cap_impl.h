#pragma once
#include "esphome/core/defines.h"

#ifdef USE_ZEPHYR
#include "esphome/core/helpers.h"
#include <memory>
#include <zephyr/bluetooth/l2cap.h>

namespace esphome::socket_ble {
// Forward declaration
class ZephyrBleL2capImpl;

class ZephyrBleL2capListenImpl {
 public:
  int bind(const struct sockaddr_l2 *name, socklen_t addrlen);
  int close();

  bool ready() const { return !this->pending_channels_.empty(); }

  std::unique_ptr<ZephyrBleL2capImpl> accept(struct sockaddr_l2 *addr, socklen_t *addrlen);
  std::unique_ptr<ZephyrBleL2capImpl> accept_loop_monitored(struct sockaddr_l2 *addr, socklen_t *addrlen) {
    return this->accept(addr, addrlen);
  }
  int listen(int backlog) { return 0; }

  int setblocking(bool) { return 0; }

  // No-op: instances are statically allocated and must never be freed.
  static void operator delete(void *ptr);  // NOLINT(cert-dcl54-cpp,misc-new-delete-overloads)

 private:
  friend std::unique_ptr<ZephyrBleL2capListenImpl> socket_ble_listen_loop_monitored(int domain, int type, int protocol);

  static int accept_cb(struct bt_conn *conn, struct bt_l2cap_server *server, struct bt_l2cap_chan **chan);

  // Never cleared: once taken, a listen socket is bound for the process lifetime
  // (Zephyr has no API to unregister an L2CAP PSM server).
  bool in_use_ = false;
  struct bt_l2cap_server server_;
  bool bound_ = false;
  static constexpr size_t MAX_PENDING_CHANNELS = 2;
  StaticRingBuffer<std::unique_ptr<ZephyrBleL2capImpl>, MAX_PENDING_CHANNELS> pending_channels_;
};

class ZephyrBleL2capImpl {
 public:
  bool ready();

  ssize_t read(void *buf, size_t len);
  ssize_t write(const void *buf, size_t len);
  ssize_t writev(const struct iovec *iov, int iovcnt);

  int close();
  int setblocking(bool blocking) {
    assert(blocking == false);
    return 0;
  }
  size_t getpeername_to(std::span<char, BDADDR_STR_LEN> buf);
  int getpeername(struct sockaddr_l2 *addr, socklen_t *addrlen);

  static void operator delete(void *ptr);  // NOLINT(cert-dcl54-cpp,misc-new-delete-overloads)

 private:
  friend class ZephyrBleL2capListenImpl;
  static void connected_cb(struct bt_l2cap_chan *chan);
  static void disconnected_cb(struct bt_l2cap_chan *chan);
  static net_buf *alloc_buf_cb(struct bt_l2cap_chan *chan);
  static void sent_cb(struct bt_l2cap_chan *chan);
  static int recv_cb(struct bt_l2cap_chan *chan, struct net_buf *buf);
  static void released_cb(struct bt_l2cap_chan *chan);

  static constexpr struct bt_l2cap_chan_ops OPS = {
      .connected = ZephyrBleL2capImpl::connected_cb,
      .disconnected = ZephyrBleL2capImpl::disconnected_cb,
      .alloc_buf = ZephyrBleL2capImpl::alloc_buf_cb,
      .recv = ZephyrBleL2capImpl::recv_cb,
      .sent = ZephyrBleL2capImpl::sent_cb,
      .released = ZephyrBleL2capImpl::released_cb,
  };

  struct bt_l2cap_le_chan le_chan_ = {};

  // Tracks whether the C++ owner and Zephyr still hold references.
  // Storage is reclaimed only after both have released ownership.
  bool referenced_ = true;
  bool released_ = false;

  bool closed_ = false;
  bool wake_after_sent_ = false;
  bool wake_after_recv_ = false;
  sys_slist_t queued_rx_bufs_ = {nullptr, nullptr};
  struct net_buf *rx_buf_ = nullptr;
};
}  // namespace esphome::socket_ble
#endif  // USE_ZEPHYR
