#include "ota_esphome.h"
#if defined(USE_OTA) && defined(ESPHOME_OTA_UDP)

#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#ifdef ESPHOME_OTA_UDP_LWIP
#include "lwip/inet.h"
#ifdef USE_LIBRETINY
#include "lwip/tcpip.h"
#endif
#endif

#include <algorithm>
#include <cstring>
#include <memory>

/*
 * UDP data phase: the same byte stream as TCP, cut into numbered datagrams.
 *   client -> device  [type | COMMITTED][token u32][seq u16][device bytes received u16][payload]
 *   device -> client  [ACK][token u32][received u16][seq that prompted it u16][stream offset u16][window u8][our bytes]
 * The lwIP callback acks each datagram on arrival, so flash writes never look like loss, and keeps messages in its
 * UDP_WINDOW slots until the OTA loop reads them. "window": free slots in the high nibble, and in the low nibble bit k
 * set when received + k is already here (like TCP SACK).
 */

namespace esphome {

ESPHOME_LOG_TAG(TAG, "esphome.ota");

// PING (0x02) and PROBE (0x03) only prompt an ACK
static constexpr uint8_t UDP_MSG_DATA = 0x01;
static constexpr uint8_t UDP_FLAG_COMMITTED = 0x80;
static constexpr uint8_t UDP_MSG_ACK = 0x40;
static constexpr size_t UDP_HEADER_SIZE = 9;
static constexpr size_t UDP_ACK_HEADER_SIZE = 12;
// The client's 60 s wait for the token over a lossy TCP link, plus its 5 s probe
static constexpr uint32_t UDP_COMMIT_TIMEOUT = 70000;
// Like the TCP socket's SO_RCVTIMEO in handle_data_, so readall_ treats both alike
static constexpr uint32_t UDP_READ_WAIT_MS = 2000;
// How often the commit wait looks at the UDP link between reads of the TCP fallback byte
static constexpr uint32_t UDP_COMMIT_POLL_MS = 100;
static constexpr uint32_t UDP_LINGER_MS = 3000;

#ifdef ESPHOME_OTA_UDP_LWIP
#ifdef USE_LIBRETINY
// LibreTiny's LwIPLock is a no-op; take the core lock directly (the header drops UDP without one)
struct UdpLock {
  UdpLock() { LOCK_TCPIP_CORE(); }
  ~UdpLock() { UNLOCK_TCPIP_CORE(); }
};
#else
using UdpLock = LwIPLock;
#endif
static uint16_t slot_len(struct pbuf *p) { return p->tot_len; }
static uint16_t slot_copy(struct pbuf *p, void *dst, uint16_t len, uint16_t offset) {
  return pbuf_copy_partial(p, dst, len, offset);
}
static void slot_free(struct pbuf *p) { pbuf_free(p); }
#else
// The host has no lwIP thread: the OTA loop drains the socket itself. Empty bodies, not = default, so clang-tidy
// does not flag the guards as unused
struct UdpLock {
  UdpLock() {}
  ~UdpLock() {}
};
struct OtaUdpDatagram {
  uint16_t len;
  uint8_t data[1500];
};
static uint16_t slot_len(OtaUdpDatagram *p) { return p->len; }
static uint16_t slot_copy(OtaUdpDatagram *p, void *dst, uint16_t len, uint16_t offset) {
  const uint16_t n = std::min<uint16_t>(len, p->len - offset);
  memcpy(dst, p->data + offset, n);
  return n;
}
static void slot_free(OtaUdpDatagram *p) { delete p; }
#endif

static void put_u16(uint8_t *at, uint16_t value) {
  at[0] = value >> 8;
  at[1] = value & 0xFF;
}

ESPHomeOTAComponent::UdpLink::~UdpLink() { this->release(); }

void ESPHomeOTAComponent::UdpLink::release() {
  UdpLock lock;
#ifdef ESPHOME_OTA_UDP_LWIP
  if (this->pcb != nullptr)
    udp_remove(this->pcb);
  this->pcb = nullptr;
#else
  this->sock = nullptr;
#endif
  for (auto &p : this->queue) {
    if (p != nullptr)
      slot_free(p);
    p = nullptr;
  }
}

void ESPHomeOTAComponent::udp_send_ack(UdpLink &link, uint16_t prompted_by) {
  const uint16_t len = UDP_ACK_HEADER_SIZE + link.out_len;
#ifdef ESPHOME_OTA_UDP_LWIP
  struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, len, PBUF_RAM);
  if (p == nullptr)
    return;  // Like a lost ACK: the client asks again
  auto *ack = static_cast<uint8_t *>(p->payload);
#else
  uint8_t ack[UDP_ACK_HEADER_SIZE + UDP_OUT_SIZE];
#endif
  ack[0] = UDP_MSG_ACK;
  memcpy(ack + 1, link.reply, 4);
  put_u16(ack + 5, link.received);
  put_u16(ack + 7, prompted_by);
  put_u16(ack + 9, link.out_base);
  uint8_t window = (UDP_WINDOW - static_cast<uint16_t>(link.received - link.consumed)) << 4;
  for (uint8_t k = 1; k < UDP_WINDOW; k++) {
    if (link.queue[(link.received + k) % UDP_WINDOW] != nullptr)
      window |= 1 << k;
  }
  ack[11] = window;
  memcpy(ack + UDP_ACK_HEADER_SIZE, link.out, link.out_len);
  // Best effort: a lost ACK is repeated for the client's next datagram
#ifdef ESPHOME_OTA_UDP_LWIP
  udp_sendto(link.pcb, p, &link.peer_ip, link.peer_port);
  pbuf_free(p);
#else
  struct sockaddr_storage to = link.peer;
  if (to.ss_family == AF_INET6) {
    reinterpret_cast<sockaddr_in6 *>(&to)->sin6_port = htons(link.peer_port);
  } else {
    reinterpret_cast<sockaddr_in *>(&to)->sin_port = htons(link.peer_port);
  }
  link.sock->sendto(ack, len, 0, reinterpret_cast<const struct sockaddr *>(&to), link.peer_len);
#endif
}

void ESPHomeOTAComponent::udp_on_datagram(UdpLink &link, UdpSlot p, uint16_t port) {
  uint8_t hdr[UDP_HEADER_SIZE];
  if (slot_copy(p, hdr, UDP_HEADER_SIZE, 0) != UDP_HEADER_SIZE || memcmp(hdr + 1, link.reply, 4) != 0) {
    slot_free(p);
    return;
  }
  // The token proves the sender; answer the port it arrived from, which NAT may have rewritten
  link.peer_port = port;
  link.committed |= (hdr[0] & UDP_FLAG_COMMITTED) != 0;
  // Drop the responses the client already has
  const uint16_t acked = encode_uint16(hdr[7], hdr[8]) - link.out_base;
  if (acked != 0 && acked <= link.out_len) {
    link.out_len -= acked;
    memmove(link.out, link.out + acked, link.out_len);
    link.out_base += acked;
  }
  const uint16_t seq = encode_uint16(hdr[5], hdr[6]);
  UdpSlot &slot = link.queue[seq % UDP_WINDOW];
  if ((hdr[0] & ~UDP_FLAG_COMMITTED) == UDP_MSG_DATA && slot_len(p) > UDP_HEADER_SIZE &&
      static_cast<uint16_t>(seq - link.consumed) < UDP_WINDOW && slot == nullptr) {
    slot = p;
    while (static_cast<uint16_t>(link.received - link.consumed) < UDP_WINDOW &&
           link.queue[link.received % UDP_WINDOW] != nullptr)
      link.received++;
  } else {
    slot_free(p);
  }
  udp_send_ack(link, seq);
}

#ifdef ESPHOME_OTA_UDP_LWIP
// lwIP context: the tcpip thread (ESP32, LibreTiny), SYS (ESP8266) or an IRQ (RP2040)
void ESPHomeOTAComponent::udp_recv_cb(void *arg, struct udp_pcb * /*pcb*/, struct pbuf *p, const ip_addr_t *addr,
                                      uint16_t port) {
  auto &link = *static_cast<UdpLink *>(arg);
  if (!ip_addr_cmp(addr, &link.peer_ip)) {
    pbuf_free(p);
    return;
  }
  udp_on_datagram(link, p, port);
}
#else
static bool same_ip(const struct sockaddr_storage &a, const struct sockaddr_storage &b) {
  uint32_t ipv4_a, ipv4_b;
  const bool v4_a = socket::sockaddr_to_ipv4(reinterpret_cast<const struct sockaddr *>(&a), &ipv4_a);
  const bool v4_b = socket::sockaddr_to_ipv4(reinterpret_cast<const struct sockaddr *>(&b), &ipv4_b);
  if (v4_a || v4_b)
    return v4_a && v4_b && ipv4_a == ipv4_b;
  return a.ss_family == AF_INET6 && b.ss_family == AF_INET6 &&
         memcmp(&reinterpret_cast<const sockaddr_in6 *>(&a)->sin6_addr,
                &reinterpret_cast<const sockaddr_in6 *>(&b)->sin6_addr, sizeof(in6_addr)) == 0;
}

void ESPHomeOTAComponent::udp_poll_() {
  UdpLink &link = *this->udp_;
  for (;;) {
    uint8_t buf[sizeof(OtaUdpDatagram::data)];
    struct sockaddr_storage from;
    socklen_t from_len = sizeof(from);
    const ssize_t len = link.sock->recvfrom(buf, sizeof(buf), reinterpret_cast<struct sockaddr *>(&from), &from_len);
    if (len < 0)
      return;
    if (!same_ip(from, link.peer))
      continue;
    auto *datagram = new OtaUdpDatagram;  // NOLINT(cppcoreguidelines-owning-memory)
    datagram->len = len;
    memcpy(datagram->data, buf, len);
    const uint16_t port = from.ss_family == AF_INET6 ? ntohs(reinterpret_cast<sockaddr_in6 *>(&from)->sin6_port)
                                                     : ntohs(reinterpret_cast<sockaddr_in *>(&from)->sin_port);
    udp_on_datagram(link, datagram, port);
  }
}
#endif

void ESPHomeOTAComponent::udp_open_(UdpLink &link) {
  struct sockaddr_storage peer;
  socklen_t peer_len = sizeof(peer);
  if (this->client_->getpeername(reinterpret_cast<struct sockaddr *>(&peer), &peer_len) != 0)
    return;
  uint8_t token[4];
  if (!random_bytes(token, sizeof(token)))
    return;
#ifdef ESPHOME_OTA_UDP_LWIP
  uint32_t ipv4;
  if (socket::sockaddr_to_ipv4(reinterpret_cast<const struct sockaddr *>(&peer), &ipv4)) {
    ip_addr_set_ip4_u32(&link.peer_ip, ipv4);
  } else {
#if USE_NETWORK_IPV6 && LWIP_IPV6
    inet6_addr_to_ip6addr(ip_2_ip6(&link.peer_ip), &reinterpret_cast<const sockaddr_in6 *>(&peer)->sin6_addr);
    IP_SET_TYPE_VAL(link.peer_ip, IPADDR_TYPE_V6);
#else
    return;
#endif
  }
  UdpLock lock;
#if LWIP_IPV6
  link.pcb = udp_new_ip_type(IPADDR_TYPE_ANY);
#else
  link.pcb = udp_new();
#endif
  // On failure the reply stays all zero and ~UdpLink removes the pcb
  if (link.pcb == nullptr || udp_bind(link.pcb, IP_ANY_TYPE, this->port_) != ERR_OK)
    return;
  udp_recv(link.pcb, udp_recv_cb, &link);
#else
  link.peer = peer;
  link.peer_len = peer_len;
  // Same domain as the listener, so the peer address from getpeername fits
  link.sock = socket::socket_ip(SOCK_DGRAM, IPPROTO_UDP);
  struct sockaddr_storage local;
  const socklen_t local_len =
      socket::set_sockaddr_any(reinterpret_cast<struct sockaddr *>(&local), sizeof(local), this->port_);
  if (link.sock == nullptr || local_len == 0 ||
      link.sock->bind(reinterpret_cast<struct sockaddr *>(&local), local_len) != 0 ||
      link.sock->setblocking(false) != 0)
    return;
#endif
  // Below the error codes (0x80 and up) the client checks the first byte against, and never all zero
  token[0] = (token[0] & 0x7F) | 0x01;
  memcpy(link.reply, token, sizeof(token));
}

bool ESPHomeOTAComponent::udp_start_() {
  // TCP now only carries a fallback byte; short read timeouts let the UDP link be checked in between
  struct timeval tv {
    0, UDP_COMMIT_POLL_MS * 1000
  };
  this->client_->setsockopt(SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
  const uint32_t start = millis();
  while (millis() - start < UDP_COMMIT_TIMEOUT) {
    this->udp_poll_();
    bool committed;
    {
      UdpLock lock;
      committed = this->udp_->committed;
    }
    if (committed) {
      // The client heard us over UDP, so TCP is done. Outside the lock: closing a socket waits on the tcpip thread
      this->client_ = nullptr;
      return true;
    }
    // A raw byte outside any Noise frame: the client heard nothing over UDP. Forging it only forces the ordinary
    // TCP path, which dropping our datagrams forces anyway
    uint8_t fallback;
    const ssize_t read = this->client_->read(&fallback, 1);
    if (read == 1) {
      this->udp_->release();
      this->udp_ = nullptr;
      tv = {UDP_READ_WAIT_MS / 1000, 0};
      this->client_->setsockopt(SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
      return true;
    }
    if (read == 0) {
      this->remote_closed_ = true;
      return false;
    }
    if (!this->would_block_(errno))
      return false;
    App.feed_wdt();
  }
  return false;
}

ssize_t ESPHomeOTAComponent::udp_read_(uint8_t *buf, size_t len) {
  UdpLink &link = *this->udp_;
  const uint32_t start = millis();
  size_t got = 0;
  for (;;) {
    this->udp_poll_();
    UdpSlot p;
    {
      UdpLock lock;
      p = link.consumed != link.received ? link.queue[link.consumed % UDP_WINDOW] : nullptr;
    }
    if (p == nullptr) {
      if (got != 0)
        return got;
      if (millis() - start >= UDP_READ_WAIT_MS)
        break;
      this->yield_and_feed_watchdog_();
      continue;
    }
    // The callback never touches the slot being read, so the copy runs without the lock
    const uint16_t offset = UDP_HEADER_SIZE + link.read_offset;
    const uint16_t n = slot_copy(p, buf + got, std::min<size_t>(len - got, slot_len(p) - offset), offset);
    got += n;
    link.read_offset += n;
    if (UDP_HEADER_SIZE + link.read_offset >= slot_len(p)) {
      UdpLock lock;
      slot_free(p);
      link.queue[link.consumed % UDP_WINDOW] = nullptr;
      // A full window just opened: tell the client, which is holding back
      const bool was_full = static_cast<uint16_t>(link.received - link.consumed) == UDP_WINDOW;
      link.consumed++;
      link.read_offset = 0;
      if (was_full)
        udp_send_ack(link, link.received - 1);
    }
    if (got == len)
      return got;
  }
  errno = EWOULDBLOCK;
  return -1;
}

bool ESPHomeOTAComponent::udp_send_(const uint8_t *data, size_t len) {
  UdpLink &link = *this->udp_;
  UdpLock lock;
  if (link.out_len + len > UDP_OUT_SIZE)
    return false;
  memcpy(link.out + link.out_len, data, len);
  link.out_len += len;
  // Prompted by nothing new, so the client takes no round trip sample from it
  udp_send_ack(link, link.received - 1);
  return true;
}

void ESPHomeOTAComponent::udp_linger_() {
  if (this->udp_ == nullptr)
    return;
  const uint32_t start = millis();
  while (millis() - start < UDP_LINGER_MS) {
    this->udp_poll_();
    {
      UdpLock lock;
      if (this->udp_->out_len == 0)
        return;
    }
    this->yield_and_feed_watchdog_();
  }
}

}  // namespace esphome
#endif  // USE_OTA && ESPHOME_OTA_UDP
