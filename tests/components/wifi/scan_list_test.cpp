#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

#include "esphome/components/wifi/scan_list.h"

namespace esphome::wifi::testing {

namespace {

// Stand-in for WiFiScanResult, which does not compile on the host.
struct Entry {
  std::string ssid;
  int8_t rssi;
  bool with_auth{true};
  bool is_hidden{false};

  // Compares length and bytes like CompactString does, so an embedded NUL counts.
  bool ssid_equals(const Entry &other) const { return this->ssid == other.ssid; }
  int8_t get_rssi() const { return this->rssi; }
  bool get_with_auth() const { return this->with_auth; }
  bool get_is_hidden() const { return this->is_hidden; }
};

// One network as a consumer would emit it.
struct Row {
  std::string ssid;
  int8_t rssi;
  bool lock;

  bool operator==(const Row &rhs) const { return ssid == rhs.ssid && rssi == rhs.rssi && lock == rhs.lock; }
};

// Walk the results the way the consumers do and collect the rows that survive.
std::vector<Row> rows(const std::vector<Entry> &results) {
  std::vector<Row> out;
  for (size_t i = 0; i < results.size(); i++) {
    bool with_auth = false;
    if (!should_show_scan_entry(results, results[i], with_auth))
      continue;
    out.push_back({results[i].ssid, results[i].rssi, with_auth});
  }
  return out;
}

}  // namespace

TEST(ScanList, SingleEntryShown) {
  std::vector<Entry> results = {{"Home", -60}};
  EXPECT_EQ(rows(results), (std::vector<Row>{{"Home", -60, true}}));
}

TEST(ScanList, DistinctSsidsAllShownInOrder) {
  std::vector<Entry> results = {{"Home", -60}, {"Guest", -70}, {"Cafe", -40}};
  EXPECT_EQ(rows(results), (std::vector<Row>{{"Home", -60, true}, {"Guest", -70, true}, {"Cafe", -40, true}}));
}

// Results are ordered by connection preference, not RSSI, so the strongest entry
// can sit anywhere in the list.
TEST(ScanList, SameSsidKeepsStrongest) {
  std::vector<Entry> results = {{"Home", -70}, {"Home", -50}, {"Home", -60}};
  EXPECT_EQ(rows(results), (std::vector<Row>{{"Home", -50, true}}));
}

TEST(ScanList, EqualRssiKeepsFirst) {
  std::vector<Entry> results = {{"Home", -60}, {"Home", -60}, {"Home", -60}};
  bool with_auth = false;
  EXPECT_TRUE(should_show_scan_entry(results, results[0], with_auth));
  EXPECT_FALSE(should_show_scan_entry(results, results[1], with_auth));
  EXPECT_FALSE(should_show_scan_entry(results, results[2], with_auth));
  EXPECT_EQ(rows(results), (std::vector<Row>{{"Home", -60, true}}));
}

// with_auth is an out-parameter that must only be written for a shown entry.
TEST(ScanList, WithAuthUntouchedWhenNotShown) {
  std::vector<Entry> results = {{"Home", -50, false}, {"Home", -70, true}};
  bool with_auth = false;
  EXPECT_FALSE(should_show_scan_entry(results, results[1], with_auth));
  EXPECT_FALSE(with_auth);
}

TEST(ScanList, DuplicatesInterleavedWithOtherNetworks) {
  std::vector<Entry> results = {{"Home", -70}, {"Guest", -55}, {"Home", -50}, {"Guest", -65}};
  EXPECT_EQ(rows(results), (std::vector<Row>{{"Guest", -55, true}, {"Home", -50, true}}));
}

// Hidden networks scan with an empty SSID. They are never listed and do not
// collapse into each other or into anything else.
TEST(ScanList, HiddenEntriesNeverShown) {
  std::vector<Entry> results = {{"", -40, true, true}, {"Home", -70}, {"", -30, true, true}};
  EXPECT_EQ(rows(results), (std::vector<Row>{{"Home", -70, true}}));
}

// On ESP8266 the hidden flag comes from the driver alongside a real SSID, so a
// hidden access point can share its name with a visible one. It must not
// outrank that visible entry and leave the network unlisted.
TEST(ScanList, HiddenEntryDoesNotSuppressVisibleSameSsid) {
  std::vector<Entry> results = {{"Home", -40, true, true}, {"Home", -70}};
  EXPECT_EQ(rows(results), (std::vector<Row>{{"Home", -70, true}}));
}

// An open access point and a secured one sharing an SSID collapse to one row that
// still asks for a password, whichever of them is strongest.
TEST(ScanList, LockSetWhenAnyEntryRequiresAuth) {
  std::vector<Entry> open_stronger = {{"Home", -50, false}, {"Home", -70, true}};
  EXPECT_EQ(rows(open_stronger), (std::vector<Row>{{"Home", -50, true}}));

  std::vector<Entry> secured_stronger = {{"Home", -70, false}, {"Home", -50, true}};
  EXPECT_EQ(rows(secured_stronger), (std::vector<Row>{{"Home", -50, true}}));
}

TEST(ScanList, LockClearWhenEveryEntryIsOpen) {
  std::vector<Entry> results = {{"Cafe", -60, false}, {"Cafe", -50, false}};
  EXPECT_EQ(rows(results), (std::vector<Row>{{"Cafe", -50, false}}));
}

// The auth flag of an unrelated network must not leak into another SSID's row.
TEST(ScanList, LockIsPerSsid) {
  std::vector<Entry> results = {{"Cafe", -60, false}, {"Home", -50, true}};
  EXPECT_EQ(rows(results), (std::vector<Row>{{"Cafe", -60, false}, {"Home", -50, true}}));
}

TEST(ScanList, EmptyListShowsNothing) {
  std::vector<Entry> results;
  EXPECT_TRUE(rows(results).empty());
}

}  // namespace esphome::wifi::testing
