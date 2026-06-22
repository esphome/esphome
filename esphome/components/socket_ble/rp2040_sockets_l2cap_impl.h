#pragma once
#include "esphome/core/defines.h"

#ifdef USE_RP2040
#include "headers.h"
#include "esphome/core/helpers.h"

#include <memory>
#include <span>

extern "C" {
#include "btstack.h"
}

namespace esphome::socket_ble {

class RP2040BleL2capImpl;

class RP2040BleL2capListenImpl {
 public:
  ~RP2040BleL2capListenImpl() { close(); }

  int bind(const sockaddr_l2 *name, socklen_t addrlen);
  int listen(int backlog) { return 0; }
  int close();

  bool ready() const { return !this->pending_channels_.empty(); }

  std::unique_ptr<RP2040BleL2capImpl> accept(sockaddr_l2 *addr, socklen_t *addrlen);
  std::unique_ptr<RP2040BleL2capImpl> accept_loop_monitored(struct sockaddr_l2 *addr, socklen_t *addrlen) {
    return this->accept(addr, addrlen);
  }

  int setblocking(bool) { return 0; }
  uint16_t psm() { return this->psm_; }

 private:
  friend class RP2040BleL2capImpl;

  static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

  uint16_t psm_ = 0;
  bool bound_ = false;

  struct pending_channel_entry {
    uint16_t cid;
    bdaddr_t address;
  };
  static constexpr size_t MAX_PENDING_CHANNELS = 2;
  StaticRingBuffer<struct pending_channel_entry, MAX_PENDING_CHANNELS> pending_channels_;
};

class RP2040BleL2capImpl {
 public:
  ssize_t read(void *buf, size_t len);
  ssize_t write(const void *buf, size_t len);

  ssize_t writev(const struct iovec *iov, int iovcnt);

  int close();

  int setblocking(bool blocking) {
    assert(blocking == false);
    return 0;
  }

  bool ready() const { return this->receive_read_available_ != 0 || this->closed_; }

  size_t getpeername_to(std::span<char, BDADDR_STR_LEN> buf);

  int getpeername(sockaddr_l2 *addr, socklen_t *addrlen);

  uint16_t local_cid() { return this->local_cid_; }

 private:
  friend class RP2040BleL2capListenImpl;

  uint16_t local_cid_ = 0;
  bdaddr_t peer_addr_{};

  bool closed_ = false;

  uint8_t receive_sdu_buffer_[SOCKET_BLE_MTU];
  size_t receive_read_available_ = 0;
  size_t receive_read_pos_ = 0;

  uint8_t send_sdu_buffer_[SOCKET_BLE_MTU];
  bool send_sdu_buffer_sending_ = false;
};

}  // namespace esphome::socket_ble

#endif  // USE_RP2040
