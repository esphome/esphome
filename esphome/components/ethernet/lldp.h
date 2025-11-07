// This implements a transmit-only Link Layer Discovery Protocol (LLDP) IEEE
// 802.1AB packet generator and transmitter state engine.
//
// All mandatory and some optional data units have been implemented.
//
// Written by Nic Bellamy, 2025. https://github.com/Gnuspice
#pragma once
#include "esphome/core/defines.h"
#ifdef USE_ETHERNET_LLDP
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include <string>
#include <array>

// For eth_addr, eth_hdr and ETH_HWADDR_LEN
#include <lwip/prot/ethernet.h>

#include "esphome/components/network/ip_address.h"
#include "esp_eth.h"

// Initial configuration
#define LLDP_TX_FAST_COUNT (4)       // Initial packet burst count, 1/sec
#define LLDP_TX_INTERVAL (30)        // Interval in seconds to transmit at otherwise
#define LLDP_TX_HOLD (4)             // Hold value, used with LLDP_TX_INTERVAL to calculate TTL
#define LLDP_DEFAULT_PORT "ETH_DEF"  // From esp_netif_defaults.h but not easily accessible
#define LLDP_STRING_MAX (128)        // Maximum lengths of port, system name and description fields

// LLDP "Nearest bridge" multicast address:
#define LLDP_NEAREST_BRIDGE_MAC \
  { 0x01, 0x80, 0xc2, 0x00, 0x00, 0x0e }

// Protocol maximums for configurable/calculated items
#define LLDP_PROTO_MAX_FAST_COUNT (8)
#define LLDP_PROTO_MAX_INTERVAL (3600)
#define LLDP_PROTO_MAX_HOLD (100)
#define LLDP_PROTO_MAX_TTL (65535)

namespace esphome {
namespace ethernet {

class LLDPTransmitter {
 public:
  LLDPTransmitter()
      : mac_addr_{},
        port_(LLDP_DEFAULT_PORT),
        lldp_multicast_mac_{LLDP_NEAREST_BRIDGE_MAC},
        tx_fast_count_{LLDP_TX_FAST_COUNT},
        tx_interval_{LLDP_TX_INTERVAL},
        tx_hold_{LLDP_TX_HOLD},
        state_{TXStateReconnect} {
    this->recalc_ttl();
  }

  // setup() opens the L2TAP device on the given ethernet handle
  esp_err_t setup(esp_eth_handle_t eth_handle);

  // should_transmit() expects to be called on a 1-second timer tick, and will
  // return true if a transmission should be made.
  bool should_transmit();

  // transmit() constructs an LLDP frame and transmits it
  esp_err_t transmit();

  // restart() causes the state engine to delay 1 second then start a fast-mode burst.
  void restart() { this->state_ = TXStateReconnect; };

  // Setup (Required): set_mac() - sets MAC address, must match system
  void set_mac(uint8_t mac[ETH_HWADDR_LEN]) { memcpy(this->mac_addr_.addr, mac, ETH_HWADDR_LEN); };

  // Optional setup follows

  // set_port() - sets alternate port name
  void set_port(const char *port) { this->port_ = port; };
  const char *get_port() { return this->port_; };

  // set_tx_fast_count() sets the number of packets to burst upon (re)connection
  void set_tx_fast_count(uint16_t count) {
    this->tx_fast_count_ = std::min(std::max(count, uint16_t(1)), uint16_t(LLDP_PROTO_MAX_FAST_COUNT));
    this->recalc_ttl();
  };

  // set_tx_interval() sets the non-burst interval (in seconds) to transmit frames at
  void set_tx_interval(uint16_t interval) {
    this->tx_interval_ = std::min(std::max(interval, uint16_t(1)), uint16_t(LLDP_PROTO_MAX_INTERVAL));
    this->recalc_ttl();
  };

  // set_tx_hold() sets the hold value, used to calculate the overall TTL
  void set_tx_hold(uint16_t hold) {
    this->tx_hold_ = std::min(std::max(hold, uint16_t(1)), uint16_t(LLDP_PROTO_MAX_HOLD));
    this->recalc_ttl();
  };

  // get_ttl() - returns calculated time-to-live value
  uint16_t get_ttl() { return this->calc_ttl_; };

  // set_system_name() sets the System Name TLV value
  void set_system_name(const char *name) { this->system_name_ = name; }
  const char *get_system_name() { return this->system_name_; }

  // set_system_description() sets the System Description TLV value
  void set_system_description(const char *desc) { this->system_description_ = desc; }
  const char *get_system_description() { return this->system_description_; }

  // Support for Management TLVs with the device IP address(es)
  //
  // Update prior to transmission, as IPs may change over time.
  void set_ips(network::IPAddresses &ips);

 protected:
  enum TXState : uint8_t {
    TXStateReconnect,
    TXStateFastMode,
    TXStateNormal,
  };

  // Sorted by descending size
  network::IPAddresses ip_addresses_;
  const char *port_{""};
  const char *system_name_{""};
  const char *system_description_{""};
  struct eth_addr lldp_multicast_mac_;
  struct eth_addr mac_addr_;
  int l2tap_fd_;
  uint16_t calc_ttl_;
  uint16_t state_fastmode_count_;
  uint16_t state_message_interval_;
  uint16_t tx_fast_count_;
  uint16_t tx_interval_;
  uint16_t tx_hold_;
  TXState state_;

  // generate() creates the LLDPDU, returning false on error
  bool generate(uint8_t *buf, size_t buf_len, size_t *pkt_len);

  // Recalculate TTL based on interval and hold values
  void recalc_ttl() { this->calc_ttl_ = std::min(this->tx_interval_ * this->tx_hold_ + 1, LLDP_PROTO_MAX_TTL); };
};

}  // namespace ethernet
}  // namespace esphome

#endif /* USE_ETHERNET_LLDP */
// EOF
