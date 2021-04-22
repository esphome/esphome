#include "ddns.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"

static const char *TAG = "DDNS";

namespace esphome {
namespace ddns{
  void DDNSComponent::setup() {
    EasyDDNS.service(this->service);
    if (this->use_token)
      EasyDDNS.client(this->domain, this->token);
    else
      EasyDDNS.client(this->domain, this->username, this->password);

    EasyDDNS.onUpdate([&](const char* oldIP, const char* newIP){
      ESP_LOGD(TAG, "DDNS - IP Change Detected: %s", newIP);
    });
  }

  void DDNSComponent::loop() {
    EasyDDNS.update(this->update_interval, this->use_local_ip);
  }
};
};
