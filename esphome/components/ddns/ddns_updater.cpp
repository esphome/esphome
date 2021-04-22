#include "ddns_updater.h"

static const char *TAG = "DDNS";

namespace esphome {
namespace ddns {
  void DDNSComponent::setup() {
    EasyDDNS.service(this->service_);
    if (this->use_token_)
      EasyDDNS.client(this->domain_, this->token_);
    else
      EasyDDNS.client(this->domain_, this->username_, this->password_);

    EasyDDNS.on_update([&](const char* oldIP, const char* newIP){
      ESP_LOGD(TAG, "DDNS - IP Change Detected: %s", newIP);
    });
  }

  void DDNSComponent::loop() {
    EasyDDNS.update(this->update_interval_, this->use_local_ip_);
  }
}
}
