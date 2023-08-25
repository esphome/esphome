#ifdef USE_ESP32

#pragma once

#include "esphome/core/component.h"
#include "esphome/components/esp32_camera/esp32_camera.h"
#include "esphome/core/defines.h"

#ifdef USE_ESPASYNCRTSP
#include <AsyncRTSP.h>
#endif

namespace esphome {
namespace rtsp_server {
#ifdef USE_ESPASYNCRTSP
class RTSPServer : public Component {
 public:
  void setup() override;
  float get_setup_priority() const override;
  void dump_config() override;

  void loop() override;
  void set_port(uint16_t port);
  void set_camera(void *camera);

 private:
  uint16_t port_;
  esp32_camera::ESP32Camera *camera__;
  AsyncRTSPServer *server_;
  dimensions parse_camera_dimensions_(camera_config_t config);
  dimensions dim_;
};
#endif
}  // namespace rtsp_server
}  // namespace esphome
#endif
