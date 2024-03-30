#pragma once

#ifdef USE_ESP32

#include "esphome/core/component.h"
#include "esphome/core/application.h"
#include "esphome/components/esp32_camera/esp32_camera.h"

namespace esphome {
namespace litter_robot_presence_detector {

constexpr uint8_t PRESENCE = 1;
constexpr uint8_t EMPTY = 0;

class LitterRobotPresenceDetector : public PollingComponent {
 public:
  // constructor
  LitterRobotPresenceDetector() : PollingComponent(2000) {}

  void on_shutdown() override;
  void setup() override;
  void update() override;

 protected:
  std::shared_ptr<esphome::esp32_camera::CameraImage> wait_for_image_();
  SemaphoreHandle_t semaphore_;
  std::shared_ptr<esphome::esp32_camera::CameraImage> image_;
  bool inferring_{false};
};
}  // namespace litter_robot_presence_detector
}  // namespace esphome

#endif
