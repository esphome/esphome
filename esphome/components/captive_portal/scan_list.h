#pragma once
#include <cstddef>

namespace esphome::captive_portal {

// A scan returns one entry per BSSID, so a network served by several access points
// appears several times. The portal form only ever submits an SSID, so the list
// shows each SSID once.
//
// Returns true when results[i] is the entry to show for its SSID: the strongest
// RSSI, first occurrence on ties. Hidden entries are never shown. Results are
// sorted by connection preference, not strictly by RSSI, so RSSI is compared
// explicitly.
//
// with_auth is written only when returning true, and is true when any entry with
// that SSID requires a key, so the padlock icon is drawn whenever one might be
// needed, whichever access point was strongest.
//
// Templated on the container so the rule can be unit tested on the host, where
// wifi has no backend.
template<typename Results> bool should_show_scan_entry(const Results &results, size_t i, bool &with_auth) {
  const auto &scan = results[i];
  if (scan.get_is_hidden())
    return false;
  bool any_auth = scan.get_with_auth();
  for (size_t j = 0; j < results.size(); j++) {
    if (j == i)
      continue;
    const auto &other = results[j];
    if (other.get_is_hidden() || other.get_ssid() != scan.get_ssid())
      continue;
    if (other.get_rssi() > scan.get_rssi() || (other.get_rssi() == scan.get_rssi() && j < i))
      return false;
    any_auth |= other.get_with_auth();
  }
  with_auth = any_auth;
  return true;
}

}  // namespace esphome::captive_portal
