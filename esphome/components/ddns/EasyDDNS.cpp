/*
* EasyDDNS Library for ESP8266 / ESP32
* See the README file for more details.
*
* Written in 2017 by Ayush Sharma.
*/

#include "EasyDDNS.h"

namespace esphome {
namespace ddns {
void EasyDDNSClass::service(String ddns_service) {
  this->ddns_choice_ = ddns_service;
}

void EasyDDNSClass::client(String ddns_domain, String ddns_username, String ddns_password) {
  this->ddns_d_ = ddns_domain;
  this->ddns_u_ = ddns_username;
  this->ddns_p_ = ddns_password;
}

void EasyDDNSClass::update(unsigned long ddns_update_interval, bool use_local_ip) {

  this->interval_ = ddns_update_interval;

  unsigned long current_millis = millis(); // Calculate Elapsed Time & Trigger
  if (current_millis - this->previous_millis_ >= this->interval_) {
    this->previous_millis_ = current_millis;

    if (use_local_ip) {
      IPAddress ip_address = WiFi.localIP();
      this->new_ip_ = String(ip_address[0]) + String(".") +
        String(ip_address[1]) + String(".") +
        String(ip_address[2]) + String(".") +
        String(ip_address[3]);
    } else {
      // ######## GET PUBLIC IP ######## //
      HTTPClient http;
      http.begin("http://ipv4bot.whatismyipaddress.com/");
      int http_code = http.GET();
      if (http_code > 0) {
        if (http_code == HTTP_CODE_OK) {
          this->new_ip_ = http.getString();
        }
      } else {
        http.end();
        return;
      }
      http.end();
    }

    // ######## GENERATE UPDATE URL ######## //
    if (this->ddns_choice_ == "duckdns") {
      this->update_url_ = "http://www.duckdns.org/update?domains=" + this->ddns_d_ + "&token=" + this->ddns_u_ + "&ip=" + this->new_ip_ + "";
    } else if (this->ddns_choice_ == "noip") {
      this->update_url_ = "http://" + this->ddns_u_ + ":" + this->ddns_p_ + "@dynupdate.no-ip.com/nic/update?hostname=" + this->ddns_d_ + "&myip=" + this->new_ip_ + "";
    } else if (this->ddns_choice_ == "dyndns") {
      this->update_url_ = "http://" + this->ddns_u_ + ":" + this->ddns_p_ + "@members.dyndns.org/v3/update?hostname=" + this->ddns_d_ + "&myip=" + this->new_ip_ + "";
    } else if (this->ddns_choice_ == "dynu") {
      this->update_url_ = "http://api.dynu.com/nic/update?hostname=" + this->ddns_d_ + "&myip=" + this->new_ip_ + "&username=" + this->ddns_u_ + "&password=" + this->ddns_p_ + "";
    } else if (this->ddns_choice_ == "enom") {
      this->update_url_ = "http://dynamic.name-services.com/interface.asp?command=SetDnsHost&HostName=" + this->ddns_d_ + "&Zone=" + this->ddns_u_ + "&DomainPassword=" + this->ddns_p_ + "&Address=" + this->new_ip_ + "";
    } else if (this->ddns_choice_ == "all-inkl") {
      this->update_url_ = "http://" + this->ddns_u_ + ":" + this->ddns_p_ + "@dyndns.kasserver.com/?myip=" + this->new_ip_;
    } else if (this->ddns_choice_ == "selfhost.de") {
      this->update_url_ = "http://" + this->ddns_u_ + ":" + this->ddns_p_ + "@carol.selfhost.de/nic/update?";
    } else if (this->ddns_choice_ == "dyndns.it") {
      this->update_url_ = "http://" + this->ddns_u_ + ":" + this->ddns_p_ + "@update.dyndns.it/nic/update?hostname=" + this->ddns_d_;
    } else if (this->ddns_choice_ == "strato") {
      this->update_url_ = "http://" + this->ddns_u_ + ":" + this->ddns_p_ + "@dyndns.strato.com/nic/update?hostname=" + this->ddns_d_ + "&myip=" + this->new_ip_ + "";
    } else if (this->ddns_choice_ == "freemyip") {
      this->update_url_ = "http://freemyip.com/update?domain=" + this->ddns_d_ + "&token=" + this->ddns_u_ + "&myip=" + this->new_ip_ + "";
    } else if (this->ddns_choice_ == "afraid.org") {
      this->update_url_ = "http://sync.afraid.org/u/" + this->ddns_u_ + "/";
    } else if (this->ddns_choice_ == "ovh") {
      this->update_url_ = "http://" + this->ddns_u_ + ":" + this->ddns_p_ + "@www.ovh.com/nic/update?system=dyndns&hostname=" + this->ddns_d_ + "&myip=" + this->new_ip_ + "";
    } else {
      ESP_LOGE("DDNS","## INPUT CORRECT DDNS SERVICE NAME ##");
      return;
    }

    // ######## CHECK & UPDATE ######### //
    if (this->old_ip_ != this->new_ip_) {

      HTTPClient http;
      http.begin(this->update_url_);
      int http_code = http.GET();
      if (http_code == 200) {
        // Send a callback notification
        if(this->ddns_update_func_ != nullptr){
          this->ddns_update_func_(this->old_ip_.c_str(), this->new_ip_.c_str());
        }
        // Replace Old IP with new one to detect further changes.
        this->old_ip_ = this->new_ip_;
      }
      http.end();
    }
  }
}

EasyDDNSClass EasyDDNS;
}  // namespace ddns
}  // namespace esphome
