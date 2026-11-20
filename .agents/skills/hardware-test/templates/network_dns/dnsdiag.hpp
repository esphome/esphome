#pragma once
// esphome#19805 diagnostics: classify lwIP's global DNS table against each netif's own cache,
// resolve a fresh hostname, and power the W5500 PHY down/up from software.
#include "esphome/core/log.h"
#include "esphome/components/ethernet/ethernet_component.h"
#include "esphome/components/wifi/wifi_component.h"
#include "esp_netif.h"
#include "esp_netif_net_stack.h"
#include "esphome/core/helpers.h"
#include "esp_eth_driver.h"
#include "lwip/netif.h"
#include "lwip/netdb.h"
#include <cstring>

namespace dnsdiag {
static const char *const TAG = "dnsdiag";

static bool eq(const esp_ip_addr_t &a, const esp_ip_addr_t &b) {
  return a.type == b.type && memcmp(&a.u_addr, &b.u_addr, a.type == ESP_IPADDR_TYPE_V4 ? 4 : 16) == 0;
}
static bool is_zero(const esp_ip_addr_t &a) {
  static constexpr uint8_t Z[16] = {0};
  return memcmp(&a.u_addr, Z, a.type == ESP_IPADDR_TYPE_V4 ? 4 : 16) == 0;
}
static esp_ip_addr_t dns(esp_netif_t *n, esp_netif_dns_type_t t) {
  esp_netif_dns_info_t d{};
  // NULL reads lwIP's global table; a failed read is logged so it is not mistaken for an empty server
  esp_err_t err = esp_netif_get_dns_info(n, t, &d);
  if (err != ESP_OK)
    ESP_LOGW(TAG, "DNS read failed on %s: %s", n ? esp_netif_get_desc(n) : "global", esp_err_to_name(err));
  return d.ip;
}
// label: 0 = empty, E = eth's, W = wifi's, B = both (same server), X = something else
static char label(const esp_ip_addr_t &g, esp_netif_t *eth, esp_netif_t *sta, esp_netif_dns_type_t t) {
  if (is_zero(g))
    return '0';
  bool e = eth && eq(g, dns(eth, t)), w = sta && eq(g, dns(sta, t));
  return e && w ? 'B' : e ? 'E' : w ? 'W' : 'X';
}
static const char *desc(esp_netif_t *n) { return n ? esp_netif_get_desc(n) : "none"; }

inline void print() {
  esp_netif_t *eth = esphome::ethernet::global_eth_component->get_esp_netif();
  esp_netif_t *sta = esphome::wifi::global_wifi_component->get_esp_netif_sta();
  esp_ip_addr_t g0 = dns(nullptr, ESP_NETIF_DNS_MAIN), g1 = dns(nullptr, ESP_NETIF_DNS_BACKUP);
  const char *lw = "none";
  int link = -1;
  {
    esphome::LwIPLock lock;
    struct netif *nd = netif_default;
    if (eth && esp_netif_get_netif_impl(eth))
      link = netif_is_link_up((struct netif *) esp_netif_get_netif_impl(eth)) ? 1 : 0;
    if (nd != nullptr) {
      if (eth && nd == esp_netif_get_netif_impl(eth))
        lw = "eth";
      else if (sta && nd == esp_netif_get_netif_impl(sta))
        lw = "sta";
      else
        lw = "other";
    }
  }
  ESP_LOGI(
      TAG,
      "DIAG g0=%c g1=%c eth.dns0=%s sta.dns0=%s eth_link=%d eth_conn=%d sta_conn=%d esp_default=%s netif_default=%s",
      label(g0, eth, sta, ESP_NETIF_DNS_MAIN), label(g1, eth, sta, ESP_NETIF_DNS_BACKUP),
      eth ? (is_zero(dns(eth, ESP_NETIF_DNS_MAIN)) ? "empty" : "set") : "n/a",
      sta ? (is_zero(dns(sta, ESP_NETIF_DNS_MAIN)) ? "empty" : "set") : "n/a", link,
      esphome::ethernet::global_eth_component->is_connected(), esphome::wifi::global_wifi_component->is_connected(),
      desc(esp_netif_get_default_netif()), lw);
}

inline void lookup() {
  // getaddrinfo blocks the loop until the DNS timeout when servers are set but no interface is up
  if (!esphome::ethernet::global_eth_component->is_connected() &&
      !esphome::wifi::global_wifi_component->is_connected()) {
    ESP_LOGI(TAG, "LOOKUP skipped, offline");
    return;
  }
  static uint32_t n = 0;
  char host[64];
  // a fresh name each time so the lwIP cache never answers
  snprintf(host, sizeof(host), "q%" PRIu32 "-%" PRIu32 ".10.0.0.1.nip.io", (uint32_t) esphome::millis() / 1000, n++);
  struct addrinfo hints{};
  hints.ai_family = AF_INET;
  struct addrinfo *res = nullptr;
  uint32_t t0 = esphome::millis();
  int rc = lwip_getaddrinfo(host, nullptr, &hints, &res);
  uint32_t dt = esphome::millis() - t0;
  if (res)
    lwip_freeaddrinfo(res);
  if (rc == 0)
    ESP_LOGI(TAG, "LOOKUP ok %" PRIu32 " ms", dt);
  else
    ESP_LOGW(TAG, "LOOKUP FAIL rc=%d %" PRIu32 " ms", rc, dt);
}

// The W5500 PHY driver's pwrctl() is a no-op, so write PHYCFGR directly: OPMD=1 with OPMDC 110 (power down)
// or 111 (all capable, autonegotiation), applied by a PHY reset (RST low, then high).
inline void phy_power(bool on) {
  static constexpr uint32_t PHYCFGR = 0x002E << 16;  // common register block
  esp_eth_mac_t *mac = nullptr;
  if (esp_eth_get_mac_instance(esphome::ethernet::global_eth_component->get_eth_handle(), &mac) != ESP_OK || !mac) {
    ESP_LOGE(TAG, "no mac");
    return;
  }
  uint32_t before = 0;
  mac->read_phy_reg(mac, 0, PHYCFGR, &before);
  uint32_t val = (before & 0x07) | 0x40 | (on ? 0x38 : 0x30);
  esp_err_t err = mac->write_phy_reg(mac, 0, PHYCFGR, val);  // RST=0 applies the mode
  vTaskDelay(pdMS_TO_TICKS(10));
  if (err == ESP_OK)
    err = mac->write_phy_reg(mac, 0, PHYCFGR, val | 0x80);
  // The driver may not poll the link on its own; let it notice the change and post the link event
  esp_eth_phy_t *phy = nullptr;
  if (esp_eth_get_phy_instance(esphome::ethernet::global_eth_component->get_eth_handle(), &phy) == ESP_OK && phy)
    phy->get_link(phy);
  uint32_t after = 0;
  mac->read_phy_reg(mac, 0, PHYCFGR, &after);
  ESP_LOGI(TAG, "PHY power %s: %s PHYCFGR 0x%02X -> 0x%02X", on ? "ON" : "OFF", esp_err_to_name(err), (unsigned) before,
           (unsigned) after);
}
}  // namespace dnsdiag
