#pragma once
#include "esphome/core/defines.h"
// DNS server that answers every name with the access point address, so a phone joining the
// AP runs its captive portal check against the device. Shared by captive_portal and the
// web_server AP mode; hides the ESP32 (own implementation) vs Arduino (DNSServer library) split.
#if defined(USE_CAPTIVE_PORTAL) || defined(USE_WEBSERVER_CAPTIVE)
#include <memory>

#include "esphome/components/network/ip_address.h"
#include "esphome/core/helpers.h"
#include "esphome/core/progmem.h"
#if defined(USE_ESP32)
#include "dns_server_esp32_idf.h"
#elif defined(USE_ARDUINO)
#include <DNSServer.h>
#endif

namespace esphome::web_server_base {

// The server object only exists while running, so an idle owner (AP not up) pays one pointer.
class CaptiveDNS {
 public:
  void start(const network::IPAddress &ip) {
    if (this->dns_server_ != nullptr)
      return;
    this->dns_server_ = make_unique<DNSServer>();
#if defined(USE_ESP32)
    this->dns_server_->start(ip);
#elif defined(USE_ARDUINO)
    this->dns_server_->setErrorReplyCode(DNSReplyCode::NoError);
    this->dns_server_->start(53, ESPHOME_F("*"), ip);
#endif
  }
  void stop() {
    if (this->dns_server_ == nullptr)
      return;
    this->dns_server_->stop();
    this->dns_server_ = nullptr;
  }
  /// Answer one pending query; call from the owner's loop() while running.
  void loop() {
    if (this->dns_server_ == nullptr)
      return;
#if defined(USE_ESP32)
    this->dns_server_->process_next_request();
#elif defined(USE_ARDUINO)
    this->dns_server_->processNextRequest();
#endif
  }
  bool is_running() const { return this->dns_server_ != nullptr; }

 protected:
  // ESP32: web_server_base::DNSServer from dns_server_esp32_idf.h; Arduino: the library class.
  std::unique_ptr<DNSServer> dns_server_;
};

}  // namespace esphome::web_server_base
#endif  // USE_CAPTIVE_PORTAL || USE_WEBSERVER_CAPTIVE
