#pragma once
#include "esphome/core/defines.h"

#if defined(USE_ESP32) && defined(CONFIG_BT_NIMBLE_ENABLED)
#include "esphome/core/helpers.h"
#include <memory>
extern "C" {

#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "nimble/ble.h"
#include "host/ble_l2cap.h"
}
namespace esphome::socket_ble {
// Forward declaration
class ESP32BleL2capImpl;

class ESP32BleL2capListenImpl {
 public:
  int bind(const struct sockaddr_l2 *name, socklen_t addrlen);
  int close();

  bool ready() const { return this->pending_channel_count_ > 0; }

  std::unique_ptr<ESP32BleL2capImpl> accept(struct sockaddr_l2 *addr, socklen_t *addrlen);
  std::unique_ptr<ESP32BleL2capImpl> accept_loop_monitored(struct sockaddr_l2 *addr, socklen_t *addrlen) {
    return this->accept(addr, addrlen);
  }
  int listen(int backlog) { return 0; }

  int setblocking(bool) { return 0; }

  // No-op: instances are statically allocated and must never be freed.
  static void operator delete(void *ptr);  // NOLINT(cert-dcl54-cpp,misc-new-delete-overloads)

 private:
  friend std::unique_ptr<ESP32BleL2capListenImpl> socket_ble_listen_loop_monitored(int domain, int type, int protocol);

  static int event_cb(struct ble_l2cap_event *event, void *arg);

  // Never cleared: once taken, a listen socket is bound for the process lifetime
  // (NimBLE has no API to unregister an L2CAP PSM server).
  bool in_use_ = false;
  bool bound_ = false;

  std::array<ESP32BleL2capImpl *, SOCKET_BLE_COUNT> open_channels_;
  size_t pending_channel_count_ = 0;
};

class ESP32BleL2capImpl {
 public:
  ESP32BleL2capImpl(ESP32BleL2capImpl **slot, struct ble_l2cap_chan *chan, size_t mtu)
      : slot_(slot), chan_(chan), mtu_(mtu){};
  ~ESP32BleL2capImpl() {
    // Remove this impl from the listen socket's open_channels_ array.
    assert(slot_ != nullptr);
    *slot_ = nullptr;
  }

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

 private:
  friend class ESP32BleL2capListenImpl;

  // Slot containing this channel inside the listener's open_channels_ array.
  ESP32BleL2capImpl **slot_;
  struct ble_l2cap_chan *chan_;
  size_t mtu_ = 0;
  bool closed_ = false;
  bool accepted_ = false;
  bool stalled_ = false;
  bool wake_after_sent_ = false;
  bool wake_after_recv_ = false;
  size_t rx_offset_ = 0;
  bool receive_in_progress_ = false;
  struct os_mbuf *sdu_rx_data_ = nullptr;
};
}  // namespace esphome::socket_ble
#endif
