#ifdef USE_ESP32
#include "litter_robot_presence_detector.h"
#include "esphome/core/log.h"

namespace esphome {
namespace litter_robot_presence_detector {

static const char *const TAG = "litter_robot_presence_detector";

float LitterRobotPresenceDetector::get_setup_priority() const { return setup_priority::LATE; }

void LitterRobotPresenceDetector::on_shutdown() {
  this->inferring_ = false;
  this->image_ = nullptr;
  vSemaphoreDelete(this->semaphore_);
  this->semaphore_ = nullptr;
}

void LitterRobotPresenceDetector::setup() {
  if (!esp32_camera::global_esp32_camera || esp32_camera::global_esp32_camera->is_failed()) {
    this->mark_failed();
    return;
  }

  this->semaphore_ = xSemaphoreCreateBinary();

  esp32_camera::global_esp32_camera->add_image_callback([this](std::shared_ptr<esp32_camera::CameraImage> image) {
    if (!this->inferring_ && image->was_requested_by(esp32_camera::API_REQUESTER)) {
      this->image_ = std::move(image);
      xSemaphoreGive(this->semaphore_);
    }
  });

  ESP_LOGD(TAG, "setup litter robot presence detector successfully");
}

void LitterRobotPresenceDetector::update() {
  if (this->inferring_) {
    ESP_LOGI(TAG, "litter robot presence detector is inferring, skip!");
    return;
  }

  esp32_camera::global_esp32_camera->request_image(esphome::esp32_camera::WEB_REQUESTER);
  auto image = this->wait_for_image_();

  if (!image) {
    ESP_LOGW(TAG, "SNAPSHOT: failed to acquire frame");
    return;
  }

  size_t image_size = image->get_data_length();
  ESP_LOGI(TAG, "SNAPSHOT: acquired frame with size %d", image_size);
}

std::shared_ptr<esphome::esp32_camera::CameraImage> LitterRobotPresenceDetector::wait_for_image_() {
  std::shared_ptr<esphome::esp32_camera::CameraImage> image;
  image.swap(this->image_);

  if (!image) {
    // retry as we might still be fetching image
    xSemaphoreTake(this->semaphore_, 2000);
    image.swap(this->image_);
  }

  return image;
}
}  // namespace litter_robot_presence_detector
}  // namespace esphome
#endif
