// This implements a transmit-only Link Layer Discovery Protocol (LLDP) IEEE
// 802.1AB packet generator and transmitter state engine.
//
// All mandatory and some optional data units have been implemented.
//
// Written by Nic Bellamy, 2025. https://github.com/Gnuspice
#include "esphome/core/defines.h"

#ifdef USE_ETHERNET_LLDP
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include <string>
#include <array>

#include "esphome/core/log.h"
#include "lwip/ip_addr.h"
// For ETHTYPE_LLDP
#include "lwip/prot/ieee.h"
#include "lldp.h"

// For ethernet and L2TAP bits
#include "esp_eth.h"
#include "esp_vfs_l2tap.h"

#define SIZE_TLV_END (2)
#define LLDP_STRING_MAX_SIZE_T ((size_t) LLDP_STRING_MAX)

namespace esphome {
namespace ethernet {

static const char *const TAG = "ethernet.lldp";

// Management TLV containing an IPv4 address
//
// Struct for ease of construction.
struct lldp_mgmt_ipv4 {
  uint8_t addr_len;
  uint8_t addr_subtype;
  uint32_t addr;
  uint8_t if_subtype;
  uint32_t if_number;
  uint8_t oid_len;
} __attribute__((packed));

// Management TLV containing an IPv6 address
//
// Struct for ease of construction.
struct lldp_mgmt_ipv6 {
  uint8_t addr_len;
  uint8_t addr_subtype;
  uint32_t addr[4];
  uint8_t if_subtype;
  uint32_t if_number;
  uint8_t oid_len;
} __attribute__((packed));

// Work out the maximum size of an LLDP packet so we only reserve as much on
// the stack during transmit() as required.
//
// This needs to match up with the TLVs (and their sizes) used in LLDPTransmitter::generate()
//
const size_t PKT_MAX = sizeof(struct eth_hdr)                    /* eth_hdr */
                       + 2 + 1 + 6                               /* Chassis ID (MAC) TLV */
                       + 2 + 1 + LLDP_STRING_MAX                 /* Port ID TLV */
                       + 2 + 2                                   /* TTL TLV */
                       + 2 + LLDP_STRING_MAX                     /* System Name TLV */
                       + 2 + LLDP_STRING_MAX                     /* System Description TLV */
                       + 2 + 4                                   /* Capabilities TLV */
                       + 5 * (2 + sizeof(struct lldp_mgmt_ipv6)) /* [0..5] IPv6 Management Address TLVs */
                       + SIZE_TLV_END;                           /* END TLV */

// LLDPPacket is a convenience class for building LLDP packets using an existing memory buffer.
class LLDPPacket {
 public:
  LLDPPacket(uint8_t *buf, size_t len) : buf_{buf}, len_{len}, used_{0} {}
  enum TLV : uint8_t {
    TLV_END = 0,
    TLV_CHASSIS_ID = 1,
    TLV_PORT_ID = 2,
    TLV_TTL = 3,
    TLV_SYSTEM_NAME = 5,
    TLV_SYSTEM_DESCRIPTION = 6,
    TLV_CAPABILITIES = 7,
    TLV_MANAGEMENT_ADDRESS = 8,
  };
  enum SubType : uint8_t {
    CHASSIS_ID_SUBTYPE_MAC = 4,
    PORT_ID_SUBTYPE_IFNAME = 5,
  };
  // begin() starts generation of a new packet
  bool begin(struct eth_addr &src_mac, struct eth_addr &dst_mac);

  // append_tlv() appends a simple TLV
  bool append_tlv(TLV tlv, size_t len, const uint8_t *data);

  // append_tlv_subtype() appends a subtyped TLV
  bool append_tlv_subtype(TLV tlv, SubType subtype, size_t len, const uint8_t *data);

  // get_used() returns the generated packet size
  size_t get_used() { return this->used_; };

 protected:
  uint8_t *buf_;
  size_t len_;
  size_t used_;
};

bool LLDPPacket::begin(struct eth_addr &src_mac, struct eth_addr &dst_mac) {
  if (this->len_ < sizeof(struct eth_hdr)) {
    return false;
  }
  struct eth_hdr *eth_header = (struct eth_hdr *) this->buf_;
  memcpy(eth_header->dest.addr, &dst_mac.addr, ETH_HWADDR_LEN);
  memcpy(eth_header->src.addr, &src_mac.addr, ETH_HWADDR_LEN);
  eth_header->type = htons(ETHTYPE_LLDP);
  this->used_ = sizeof(struct eth_hdr);
  return true;
}

bool LLDPPacket::append_tlv(TLV tlv, size_t len, const uint8_t *data) {
  size_t buf_free = this->len_ - this->used_;
  size_t buf_need = len + 2 /* TLV tag + length takes two bytes */;

  if (tlv != TLV_END) {
    // If this isn't the end-of-DU TLV, ensure we leave space for it
    buf_free -= SIZE_TLV_END;
  }

  if (buf_need > buf_free) {
    // No space in buffer for this TLV
    return false;
  }
  uint16_t tag = (tlv << 9) + (len & 0x1ff);
  uint8_t *p = this->buf_ + this->used_;
  p[0] = (tag >> 8);
  p[1] = (tag & 0xff);
  if (len) {
    memcpy(&p[2], data, len);
  }
  this->used_ += buf_need;
  return true;
}

bool LLDPPacket::append_tlv_subtype(TLV tlv, SubType subtype, size_t len, const uint8_t *data) {
  // TLV_END should not be used with this function, as it never includes a
  // subtype.
  if (tlv == TLV_END) {
    return false;
  }
  // Always subtract the size required for adding it.
  size_t buf_free = this->len_ - this->used_ - SIZE_TLV_END;
  size_t buf_need = len + 3 /* TLV tag + length + subtype takes three bytes */;

  if (buf_need > buf_free) {
    // No space in buffer for this TLV
    return false;
  }
  // length + 1 as we need to count the subtype byte too
  uint16_t tag = (tlv << 9) + ((len + 1) & 0x1ff);
  uint8_t *p = this->buf_ + this->used_;
  p[0] = (tag >> 8);
  p[1] = (tag & 0xff);
  p[2] = subtype;
  if (len) {
    memcpy(&p[3], data, len);
  }
  this->used_ += buf_need;
  return true;
}

// setup() opens the L2TAP device on the given ethernet handle
esp_err_t LLDPTransmitter::setup(esp_eth_handle_t eth_handle) {
  esp_err_t err;
  int ret;

  err = esp_vfs_l2tap_intf_register(NULL);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_vfs_l2tap_intf_register failed: (%d) %s", err, esp_err_to_name(err));
    return err;
  }

  int fd = ::open(L2TAP_VFS_DEFAULT_PATH, 0);
  if (fd < 0) {
    ESP_LOGE(TAG, "L2TAP open(%s) failed: (%d) %s", L2TAP_VFS_DEFAULT_PATH, errno, strerror(errno));
    return ESP_FAIL;
  }

  if ((ret = ::ioctl(fd, L2TAP_S_DEVICE_DRV_HNDL, eth_handle)) == -1) {
    ESP_LOGE(TAG, "Unable to bind L2TAP fd %d with esp_eth_handle_t %p: (%d) %s", fd, eth_handle, errno,
             strerror(errno));
    ::close(fd);
    return ESP_FAIL;
  }

  uint16_t eth_type_filter = ETHTYPE_LLDP;
  if ((ret = ::ioctl(fd, L2TAP_S_RCV_FILTER, &eth_type_filter)) == -1) {
    ESP_LOGE(TAG, "Unable to configure L2TAP fd %d Ethernet type receive filter: (%d) %s", fd, errno, strerror(errno));
    ::close(fd);
    return ESP_FAIL;
  }

  this->l2tap_fd_ = fd;
  ESP_LOGD(TAG, "LLDP L2TAP setup OK, l2tap_fd_=%d", this->l2tap_fd_);

  return ESP_OK;
}

// Simple state engine for determining if we are due to transmit an LLDP frame.
bool LLDPTransmitter::should_transmit() {
  switch (this->state_) {
    case TXStateReconnect:
      // Skip the first timer tick as it often fails due to device initialisation
      this->state_ = TXStateFastMode;
      this->state_fastmode_count_ = this->tx_fast_count_;
      return false;
    case TXStateFastMode:
      if (this->state_fastmode_count_ > 0) {
        this->state_fastmode_count_--;
        return true;
      }
      this->state_ = TXStateNormal;
      this->state_message_interval_ = this->tx_interval_;
      // FALLTHROUGH
    case TXStateNormal:
      if (--this->state_message_interval_ > 0) {
        return false;
      }
      this->state_message_interval_ = this->tx_interval_;
      return true;
  }
  // NOTREACHED
  return false;
}

esp_err_t LLDPTransmitter::transmit() {
  uint8_t byte_buffer[PKT_MAX];
  size_t pkt_len;

  if (!this->generate(byte_buffer, sizeof(byte_buffer), &pkt_len)) {
    // Should only happen if the buffer has been undersized
    return ESP_ERR_NO_MEM;
  }

  // LLDP packet generated successfully, write it out to our L2TAP handle.
  int ret = ::write(this->l2tap_fd_, byte_buffer, pkt_len);
  if (ret == -1) {
    // If the ethernet port is not connected (no link, maybe other cases), this
    // will return errno 5 - EIO "I/O error". The file descriptor starts
    // working again when the link is reestablished.
    ESP_LOGE(TAG, "LLDP L2TAP write(%d, %d) error %d: %s", this->l2tap_fd_, pkt_len, errno, strerror(errno));
    return ESP_FAIL;
  }
  if (ret != pkt_len) {
    ESP_LOGE(TAG, "LLDP L2TAP short write(%d, %d): only wrote %d bytes", this->l2tap_fd_, pkt_len, ret);
    return ESP_FAIL;
  }

  ESP_LOGD(TAG, "Sent %d byte LLDPDU successfully", pkt_len);
  return ESP_OK;
}

void LLDPTransmitter::set_ips(network::IPAddresses &ips) {
  uint8_t save_idx = 0;

  // Clear the saved address list
  this->ip_addresses_.fill(network::IPAddress());

  // Walk the given list of IP addresses and do a basic insertion filter to
  // build a unique list in ip_addresses_. In theory they should already be
  // unique, but LLDP require all management address entries in a frame are
  // unique, so do it to be sure.
  for (auto &ip : ips) {
    if (ip.is_set()) {
      bool seen = false;
      for (auto i = 0; i < save_idx; i++) {
        if (ip == ip_addresses_[i]) {
          seen = true;
          break;
        }
      }
      if (!seen) {
        this->ip_addresses_[save_idx++] = ip;
      }
    }
  }
}

#define LLDP_ERROR_CHECK(ret) \
  if (!ret) { \
    return false; \
  }

// Generate the LLDP packet into the given buffer.
//
// On success, true is returned, and the final packet length is stored into pkt_len.
//
// If you change the TLVs transmitted ensure the PKT_MAX constant near the top
// of this file is updated to match.
//
// TLVs must be transmitted in-order:
//   * Chassis ID TLV
//   * Port ID TLV
//   * Time To Live TLV
//   * [ zero or more optional TLVs ]
//   * End Of LLDPDU TLV as the final entry.
//
bool LLDPTransmitter::generate(uint8_t *buf, size_t buf_len, size_t *pkt_len) {
  LLDPPacket pkt(buf, buf_len);

  // Start the packet off by building an ethernet header
  pkt.begin(this->mac_addr_, this->lldp_multicast_mac_);

  // MANDATORY TLVs
  // ==============

  // Send Chassis ID TLV with our MAC address
  auto ret = pkt.append_tlv_subtype(LLDPPacket::TLV_CHASSIS_ID, LLDPPacket::CHASSIS_ID_SUBTYPE_MAC, ETH_HWADDR_LEN,
                                    this->mac_addr_.addr);
  LLDP_ERROR_CHECK(ret);

  // Send Port ID with our interface name
  ret = pkt.append_tlv_subtype(LLDPPacket::TLV_PORT_ID, LLDPPacket::PORT_ID_SUBTYPE_IFNAME,
                               std::min(strlen(this->port_), LLDP_STRING_MAX_SIZE_T), (const uint8_t *) this->port_);
  LLDP_ERROR_CHECK(ret);

  // Send our TTL value
  uint16_t ttl_data = htons(this->calc_ttl_);
  ret = pkt.append_tlv(LLDPPacket::TLV_TTL, 2, (uint8_t *) &ttl_data);
  LLDP_ERROR_CHECK(ret);

  // OPTIONAL TLVs
  // =============

  // Send System Name if set
  if (strlen(this->system_name_)) {
    ret = pkt.append_tlv(LLDPPacket::TLV_SYSTEM_NAME, std::min(strlen(this->system_name_), LLDP_STRING_MAX_SIZE_T),
                         (uint8_t *) this->system_name_);
    LLDP_ERROR_CHECK(ret);
  }

  // Send System Description if set
  if (strlen(this->system_description_)) {
    ret = pkt.append_tlv(LLDPPacket::TLV_SYSTEM_DESCRIPTION,
                         std::min(strlen(this->system_description_), LLDP_STRING_MAX_SIZE_T),
                         (uint8_t *) this->system_description_);
    LLDP_ERROR_CHECK(ret);
  }

  // Send Capabilities as "station only"
  uint8_t caps[] = {0x00, 0b10000000, 0x00, 0b10000000};
  ret = pkt.append_tlv(LLDPPacket::TLV_CAPABILITIES, 4, caps);
  LLDP_ERROR_CHECK(ret);

  // Pre-build structures for IPv4 and IPv6 versions of the Management Address
  // TLV.
  struct lldp_mgmt_ipv4 mgmt_ipv4 = {
      .addr_len = 5,          // subtype + 4 bytes for IPv4 address
      .addr_subtype = 1,      // IPv4 address family number (IANA)
      .if_subtype = 2,        // ifIndex subtype
      .if_number = htonl(1),  // index 1
      .oid_len = 0,           // no OID information
  };

  struct lldp_mgmt_ipv6 mgmt_ipv6 = {
      .addr_len = 17,         // subtype + 16 bytes for IPv6 address
      .addr_subtype = 2,      // IPv6 address family number (IANA)
      .if_subtype = 2,        // ifIndex subtype
      .if_number = htonl(1),  // index 1
      .oid_len = 0,           // no OID information
  };

  for (auto &iter : this->ip_addresses_) {
    if (!iter.is_set()) {
      continue;
    }

    ip_addr_t ip = iter;
#if LWIP_IPV6
    if (IP_IS_V6(&ip)) {
      ip6_addr_t *ip6 = ip_2_ip6(&ip);
      memcpy(mgmt_ipv6.addr, ip6->addr, sizeof(mgmt_ipv6.addr));
      ret = pkt.append_tlv(LLDPPacket::TLV_MANAGEMENT_ADDRESS, sizeof(mgmt_ipv6), (const uint8_t *) &mgmt_ipv6);
      LLDP_ERROR_CHECK(ret);
      continue;
    }
#endif
    ip4_addr_t *ip4 = ip_2_ip4(&ip);
    mgmt_ipv4.addr = ip4->addr;
    ret = pkt.append_tlv(LLDPPacket::TLV_MANAGEMENT_ADDRESS, sizeof(mgmt_ipv4), (const uint8_t *) &mgmt_ipv4);
    LLDP_ERROR_CHECK(ret);
  }

  // Final MANDATORY end-of-data TLV; should never fail as space is reserved for
  // it, but check anyway.
  ret = pkt.append_tlv(LLDPPacket::TLV_END, 0, NULL);
  LLDP_ERROR_CHECK(ret);

  // All TLVs added successfully; store the length and return success.
  *pkt_len = pkt.get_used();
  return true;
}

}  // namespace ethernet
}  // namespace esphome

#endif /* USE_ETHERNET_LLDP */
// EOF
