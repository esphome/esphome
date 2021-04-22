/*
* EasyDDNS Library for ESP8266 / ESP32
* See the README file for more details.
*
* Written in 2017 by Ayush Sharma.
*/

#include "EasyDDNS.h"


void EasyDDNSClass::service(String ddns_service) {
  ddns_choice = ddns_service;
}

void EasyDDNSClass::client(String ddns_domain, String ddns_username, String ddns_password) {
  ddns_d = ddns_domain;
  ddns_u = ddns_username;
  ddns_p = ddns_password;
}

void EasyDDNSClass::update(unsigned long ddns_update_interval, bool use_local_ip) {

  interval = ddns_update_interval;

  unsigned long currentMillis = millis(); // Calculate Elapsed Time & Trigger
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    if (use_local_ip) {
      IPAddress ipAddress = WiFi.localIP();
      new_ip = String(ipAddress[0]) + String(".") +
        String(ipAddress[1]) + String(".") +
        String(ipAddress[2]) + String(".") +
        String(ipAddress[3]);
    } else {
      // ######## GET PUBLIC IP ######## //
      HTTPClient http;
      http.begin("http://ipv4bot.whatismyipaddress.com/");
      int httpCode = http.GET();
      if (httpCode > 0) {
        if (httpCode == HTTP_CODE_OK) {
          new_ip = http.getString();
        }
      } else {
        http.end();
        return;
      }
      http.end();
    }

    // ######## GENERATE UPDATE URL ######## //
    if (ddns_choice == "duckdns") {
      update_url = "http://www.duckdns.org/update?domains=" + ddns_d + "&token=" + ddns_u + "&ip=" + new_ip + "";
    } else if (ddns_choice == "noip") {
      update_url = "http://" + ddns_u + ":" + ddns_p + "@dynupdate.no-ip.com/nic/update?hostname=" + ddns_d + "&myip=" + new_ip + "";
    } else if (ddns_choice == "dyndns") {
      update_url = "http://" + ddns_u + ":" + ddns_p + "@members.dyndns.org/v3/update?hostname=" + ddns_d + "&myip=" + new_ip + "";
    } else if (ddns_choice == "dynu") {
      update_url = "http://api.dynu.com/nic/update?hostname=" + ddns_d + "&myip=" + new_ip + "&username=" + ddns_u + "&password=" + ddns_p + "";
    } else if (ddns_choice == "enom") {
      update_url = "http://dynamic.name-services.com/interface.asp?command=SetDnsHost&HostName=" + ddns_d + "&Zone=" + ddns_u + "&DomainPassword=" + ddns_p + "&Address=" + new_ip + "";
    } else if (ddns_choice == "all-inkl") {
      update_url = "http://" + ddns_u + ":" + ddns_p + "@dyndns.kasserver.com/?myip=" + new_ip;
    } else if (ddns_choice == "selfhost.de") {
      update_url = "http://" + ddns_u + ":" + ddns_p + "@carol.selfhost.de/nic/update?";
    } else if (ddns_choice == "dyndns.it") {
      update_url = "http://" + ddns_u + ":" + ddns_p + "@update.dyndns.it/nic/update?hostname=" + ddns_d;
    } else if (ddns_choice == "strato") {
      update_url = "http://" + ddns_u + ":" + ddns_p + "@dyndns.strato.com/nic/update?hostname=" + ddns_d + "&myip=" + new_ip + "";
    } else if (ddns_choice == "freemyip") {
      update_url = "http://freemyip.com/update?domain=" + ddns_d + "&token=" + ddns_u + "&myip=" + new_ip + "";
    } else if (ddns_choice == "afraid.org") {
      update_url = "http://sync.afraid.org/u/" + ddns_u + "/";
    } else if (ddns_choice == "ovh") {
      update_url = "http://" + ddns_u + ":" + ddns_p + "@www.ovh.com/nic/update?system=dyndns&hostname=" + ddns_d + "&myip=" + new_ip + "";
    } else {
      ESP_LOGE("DDNS","## INPUT CORRECT DDNS SERVICE NAME ##");
      return;
    }

    // ######## CHECK & UPDATE ######### //
    if (old_ip != new_ip) {

      HTTPClient http;
      http.begin(update_url);
      int httpCode = http.GET();
      if (httpCode == 200) {
        // Send a callback notification
        if(_ddnsUpdateFunc != nullptr){
          _ddnsUpdateFunc(old_ip.c_str(), new_ip.c_str());
        }
        // Replace Old IP with new one to detect further changes.
        old_ip = new_ip;
      }
      http.end();
    }
  }
}

EasyDDNSClass EasyDDNS;
