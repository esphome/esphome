#pragma once

// ============================================================================
// HOST-ONLY TEST COMPONENT — DO NOT COPY TO PRODUCTION CODE
//
// Stub of the real wifi component with just enough API surface for
// improv_serial to build and run on the host platform. Scan results are
// fixed, "connecting" succeeds immediately, and save_wifi_sta only logs so
// tests can assert on the log output.
// ============================================================================

#include "esphome/components/network/ip_address.h"
#include "esphome/core/component.h"
#include "esphome/core/string_ref.h"

#include <string>
#include <vector>

namespace esphome::wifi {

class WiFiAP {
 public:
  void set_ssid(const char *ssid) { this->ssid_ = ssid; }
  void set_password(const char *password) { this->password_ = password; }
  StringRef get_ssid() const { return StringRef(this->ssid_); }
  StringRef get_password() const { return StringRef(this->password_); }

 protected:
  std::string ssid_;
  std::string password_;
};

class WiFiScanResult {
 public:
  WiFiScanResult(const char *ssid, int8_t rssi, bool with_auth, bool hidden)
      : ssid_(ssid), rssi_(rssi), with_auth_(with_auth), hidden_(hidden) {}
  StringRef get_ssid() const { return StringRef(this->ssid_); }
  int8_t get_rssi() const { return this->rssi_; }
  bool get_with_auth() const { return this->with_auth_; }
  bool get_is_hidden() const { return this->hidden_; }
  bool ssid_equals(const WiFiScanResult &other) const { return this->ssid_ == other.ssid_; }

 protected:
  std::string ssid_;
  int8_t rssi_;
  bool with_auth_;
  bool hidden_;
};

class WiFiComponent : public Component {
 public:
  WiFiComponent();
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::WIFI; }

  bool has_sta() const { return false; }
  bool is_disabled() const { return false; }
  // Always connected so network::is_connected() keeps the API server accepting clients
  bool is_connected() const { return true; }
  void start_scanning();
  const std::vector<WiFiScanResult> &get_scan_result() const { return this->scan_result_; }
  void set_sta(const WiFiAP &ap);
  void start_connecting(const WiFiAP &ap);
  void clear_sta();
  void save_wifi_sta(StringRef ssid, StringRef password);
  // Called by network::util on any USE_WIFI build
  const char *get_use_address() const { return "localhost"; }
  network::IPAddresses get_ip_addresses() { return {}; }

 protected:
  std::vector<WiFiScanResult> scan_result_;
};

extern WiFiComponent *global_wifi_component;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esphome::wifi
