#pragma once
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "EasyDDNS.h"


namespace esphome {
namespace ddns {

  class DDNSComponent : public Component {
   public:
      void set_service(String service) {
              this->service_ = service;
            };
      void set_client(String domain, String token)  {
              this->domain_ = domain;
              this->token_ = token;
              this->use_token_ = true;
            };
      void set_client(String domain, String username, String password) {
              this->domain_ = domain;
              this->username_ = username;
              this->password_ = password;
            };
      void set_update(int update_interval, bool use_local_ip)  {
              this->update_interval_ = update_interval;
              this->use_local_ip_ = use_local_ip;
            };
      void set_update_interval(uint32_t val) { /* ignore */
      }
      void setup() override;
      void loop() override;

   protected:
      bool use_token_ = false;
      String service_;
      String domain_;
      String username_;
      String password_;
      String token_;
      int update_interval_;
      bool use_local_ip_;
  };

}  // namespace ddns
}  // namespace esphome

