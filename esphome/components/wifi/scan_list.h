#pragma once
#include <cstdint>

namespace esphome::wifi {

// A scan lists every BSSID, so one SSID can appear several times. Returns true for
// the strongest entry per SSID (earliest on ties), never for hidden entries. scan
// must be an element of results. with_auth is written only when returning true and
// is set if any entry with that SSID needs a key. Templated for host tests.
template<typename Results, typename Entry>
bool should_show_scan_entry(const Results &results, const Entry &scan, bool &with_auth) {
  if (scan.get_is_hidden())
    return false;
  const int8_t rssi = scan.get_rssi();
  bool any_auth = false;
  for (const auto &other : results) {
    if (other.get_is_hidden() || !other.ssid_equals(scan))
      continue;
    // Same array, so address order is index order. scan fails both checks against itself.
    if (other.get_rssi() > rssi || (other.get_rssi() == rssi && &other < &scan))
      return false;
    any_auth |= other.get_with_auth();
  }
  with_auth = any_auth;
  return true;
}

}  // namespace esphome::wifi
