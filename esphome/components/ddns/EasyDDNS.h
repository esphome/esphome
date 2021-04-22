#pragma once
/*
EasyDDNS Library for ESP8266 or ESP32
See the README file for more details.

Written in 2017 by Ayush Sharma. Licensed under MIT.
*/

#ifndef EasyDDNS_H
#define EasyDDNS_H

#include "Arduino.h"
#include "stdlib_noniso.h"

#if defined(ESP8266)
  #include "ESP8266WiFi.h"
  #include "ESP8266HTTPClient.h"
#elif defined(ESP32)
  #include "WiFi.h"
  #include "HTTPClient.h"
#endif
namespace esphome {
namespace ddns {
// Handler to notify user about new public IP
typedef std::function<void(const char* old_ip, const char* new_ip)> DDNSUpdateHandler;

class EasyDDNSClass{
public:
  void service(String ddns_service);
  void client(String ddns_domain, String ddns_username, String ddns_password = "");
  void update(unsigned long ddns_update_interval, bool use_local_ip = false);

  // Callback
  void on_update(DDNSUpdateHandler handler) {
    this->ddns_update_func_ = handler;
  }

protected:
  DDNSUpdateHandler ddns_update_func_ = nullptr;

  unsigned long interval_;
  unsigned long previous_millis_;

  String new_ip_;
  String old_ip_;
  String update_url_;
  String ddns_u_;
  String ddns_p_;
  String ddns_d_;
  String ddns_choice_;
};
extern EasyDDNSClass EasyDDNS;

}  // namespace ddns
}  // namespace esphome
#endif
