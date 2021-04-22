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

// Handler to notify user about new public IP
typedef std::function<void(const char* old_ip, const char* new_ip)> DDNSUpdateHandler;

class EasyDDNSClass{
public:
  void service(String ddns_service);
  void client(String ddns_domain, String ddns_username, String ddns_password = "");
  void update(unsigned long ddns_update_interval, bool use_local_ip = false);

  // Callback
  void onUpdate(DDNSUpdateHandler handler) {
    _ddnsUpdateFunc = handler;
  }

private:
  DDNSUpdateHandler _ddnsUpdateFunc = nullptr;

  unsigned long interval;
  unsigned long previousMillis;

  String new_ip;
  String old_ip;
  String update_url;
  String ddns_u;
  String ddns_p;
  String ddns_d;
  String ddns_choice;
};
extern EasyDDNSClass EasyDDNS;
#endif
