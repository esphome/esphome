#pragma once

#include "esphome/core/defines.h"
#ifdef USE_WEBSERVER_OTA

#include "esphome/components/ota/ota_backend.h"
#include "esphome/components/web_server_base/web_server_base.h"
#include "esphome/core/component.h"

namespace esphome {
namespace web_server {

class WebServerOTAComponent : public ota::OTAComponent {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

 protected:
  friend class OTARequestHandler;
};

}  // namespace web_server
}  // namespace esphome

#endif  // USE_WEBSERVER_OTA
