# network.priority DNS loss (#19805, espressif/esp-idf#19082)

With two interfaces in `network: priority:` on ESP32, ESPHome sets CONFIG_ESP_NETIF_SET_DNS_PER_DEFAULT_NETIF and pins
the default with `esp_netif_set_default_netif()`. In that mode a DHCP ACK only fills the netif's own DNS cache; lwIP's
global table is written only by `esp_netif_set_default_netif_internal()`. `esp_netif_dhcpc_start()` and
`esp_netif_set_ip_info()` (DHCP client stopped) on ANY netif call `dns_clear_servers(true)` on the global table, and the
pin makes IDF skip re-election, so the table stays empty until the app re-asserts the default. Lookups then fail at once
with getaddrinfo 202 (lwIP ERR_VAL, no server set). Unfixed on IDF master when this was written.

Triggers: Wi-Fi joining or reconnecting while Ethernet is the default (ESPHome's `wifi_sta_ip_config_` and IDF's
`esp_netif_action_connected` start DHCP); Ethernet with no link while Wi-Fi is the default (the 15 s
`Connecting failed; reconnecting` retry runs `set_ip_info` plus `dhcpc_start` every time).

Harness: `templates/network_dns/dnsrepro.yaml` + `dnsdiag.hpp` for the Waveshare ESP32-S3-ETH (W5500). Every 3 s a `DIAG`
line classifies the global DNS slots against each netif's cache (0 empty, E eth's, W wifi's, B both, X other) and names
the IDF default and lwIP `netif_default`; every 5 s a `LOOKUP` resolves a fresh `*.10.0.0.1.nip.io` name (a public
wildcard name, never cached). Timeline: Wi-Fi off at boot, enabled at 40 s, disabled and enabled at 100 and 110 s, W5500
PHY powered off at 170 s (a software link down, no unplugging) and on at 250 s. Set the `first`/`second` substitutions
for both priority orders; `phy_boot: "false"` starts with the PHY down (a stand-in for no cable). Needs Wi-Fi and, for
the full matrix, a live Ethernet link. The DIAG lines carry no addresses, so captures can be quoted.

- The W5500 PHY driver's `pwrctl()` is a no-op; `dnsdiag::phy_power()` writes PHYCFGR (OPMD=1, OPMDC 110 off and 111
  on, RST low then high) and pokes `phy->get_link()`. Power down alone is not reported as a link loss (traffic kept
  flowing); the PHY reset at power on is, so the 250 s step is a short link drop and return.
- Harness trap: `lwip_getaddrinfo` on the loop task with DNS servers set and no interface up blocks until the DNS
  timeout and trips the task watchdog; `lookup()` skips while offline.
- Counts on #22800 (lookups ok/failed, PR against its merge base): Ethernet first with link 66/1 vs 23/43, Wi-Fi first
  with the PHY down until 250 s 57/0 vs 1/56, Ethernet first with the PHY down 58/0 vs 20/36. Dev empties DNS on every
  Wi-Fi join with Ethernet as the default, and on the Ethernet lease with Wi-Fi as the default.
- While the PHY is powered down the W5500 MAC logs `send command timeout` every 2 s on any build (driver noise from
  the test).
- Compare build sizes only between names of equal length: the name is stored several times, so two names that differ
  in length move the flash delta by tens of bytes.
