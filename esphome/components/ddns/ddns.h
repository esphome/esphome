#pragma once
#include "esphome.h"
#include "EasyDDNS.h"


namespace esphome {
namespace ddns{

  class DDNSComponent : public Component {
   public:
      void set_service(String service) {
              this->service = service;
            };
      void set_client(String domain, String token)  {
              this->domain = domain;
              this->token= token;
              this->use_token = true;
            };
      void set_client(String domain, String username, String password) {
              this->domain = domain;
              this->username = username;
              this->password = password;
            };
      void set_update(int update_interval, bool use_local_ip)  {
              this->update_interval = update_interval;
              this->use_local_ip = use_local_ip;
            };
      void set_update_interval(uint32_t val) { /* ignore */
      }
      void setup() override;
      void loop() override;

   protected:
      bool use_token = false;
      String service;
      String domain;
      String username;
      String password;
      String token;
      int update_interval;
      bool use_local_ip;
  };

};
};

