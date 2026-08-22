#pragma once
#include <cstdint>

namespace esphome::wifi {

// A scan returns one entry per BSSID, so a network served by several access points
// appears several times. Provisioning only ever submits an SSID, so consumers
// (captive_portal, improv_serial) list each SSID once.
//
// Returns true when scan is the entry to show for its SSID: the strongest RSSI,
// earliest entry on ties. Hidden entries are never shown. Results are sorted by
// connection preference, not strictly by RSSI, so RSSI is compared explicitly.
// scan must be an element of results; ties are broken on its position there.
//
// with_auth is written only when returning true, and is true when any entry with
// that SSID requires a key, so a password is asked for whenever one might be
// needed, whichever access point was strongest.
//
// Templated on the container and entry so the rule can be unit tested on the host,
// where wifi has no backend.
template<typename Results, typename Entry>
bool should_show_scan_entry(const Results &results, const Entry &scan, bool &with_auth) {
  if (scan.get_is_hidden())
    return false;
  const int8_t rssi = scan.get_rssi();
  bool any_auth = false;
  for (const auto &other : results) {
    if (other.get_is_hidden() || !other.ssid_equals(scan))
      continue;
    // &other < &scan orders the two the way their indices do: both point into the
    // same array, so the earlier entry wins a tie.
    if (&other != &scan && (other.get_rssi() > rssi || (other.get_rssi() == rssi && &other < &scan)))
      return false;
    any_auth |= other.get_with_auth();
  }
  with_auth = any_auth;
  return true;
}

}  // namespace esphome::wifi
