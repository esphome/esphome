#ifdef USE_ESP32
#include "rtsp_server.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/core/util.h"
#include "esphome/components/network/util.h"

namespace esphome {
namespace rtsp_server {
#ifdef USE_ESPASYNCRTSP
static const char *const TAG = "rtsp_server";

void RTSPServer::setup() {
  if (this->camera__ == nullptr) {
    ESP_LOGCONFIG(TAG, "Error: Unable to obtain reference to camera");
  }

  camera_config_t camera_config = this->camera__->get_camera_config();

  this->dim_ = this->parse_camera_dimensions_(camera_config);

  ESP_LOGD(TAG, "Beginning to set up RTSP server listener");

  this->server_ = new AsyncRTSPServer(this->port_, this->dim_);

  this->server_->onClient([](void *s) { ESP_LOGD(TAG, "Received RTSP connection"); }, this);

  this->server_->setLogFunction([](const String &s) { ESP_LOGD(TAG, "%s", s.c_str()); }, this);

  this->server_->onFrameFinished([]() { /*ESP_LOGD(TAG, "Finished pushing frame; freeing buffer");*/ }, this);

  ESP_LOGD(TAG, "Set up RTSP server listener, starting");
  try {
    this->server_->begin();
    ESP_LOGCONFIG(TAG, "Started RTSP server listener");
  } catch (...) {
    ESP_LOGCONFIG(TAG, "Failed to start RTSP server listener");
    this->mark_failed();
    return;
  }

  this->camera__->add_image_callback([this](const std::shared_ptr<esp32_camera::CameraImage> &image) {
    // push the frame as a data buffer with a length, but also push a typecasted shared_prt
    // so that the RTSP server can track (and release) ownership of the buffer
    this->server_->pushFrame(image->get_data_buffer(), image->get_data_length(), std::static_pointer_cast<void>(image));
  });

  this->dump_config();
}

void RTSPServer::loop() {
  this->server_->tick();
  if (this->server_->hasClients()) {
    esp32_camera::global_esp32_camera->start_stream(esp32_camera::RTSP_REQUESTER);
  }
}
dimensions RTSPServer::parse_camera_dimensions_(camera_config_t config) {
  struct dimensions dim = {0, 0};
  switch (config.frame_size) {
    case FRAMESIZE_QQVGA:
      dim.width = 160;
      dim.height = 120;
      break;
    case FRAMESIZE_QCIF:
      dim.width = 176;
      dim.height = 144;
      break;
    case FRAMESIZE_HQVGA:
      dim.width = 240;
      dim.height = 176;
      break;
    case FRAMESIZE_QVGA:
      dim.width = 320;
      dim.height = 240;
      break;
    case FRAMESIZE_CIF:
      dim.width = 400;
      dim.height = 296;
      break;
    case FRAMESIZE_VGA:
      dim.width = 640;
      dim.height = 480;
      break;
    case FRAMESIZE_SVGA:
      dim.width = 800;
      dim.height = 600;
      break;
    case FRAMESIZE_XGA:
      dim.width = 1024;
      dim.height = 768;
      break;
    case FRAMESIZE_SXGA:
      dim.width = 1280;
      dim.height = 1024;
      break;
    case FRAMESIZE_UXGA:
      dim.width = 1600;
      dim.height = 1200;
      break;
  }
  return dim;
}

void RTSPServer::set_port(uint16_t port) { this->port_ = port; }
void RTSPServer::set_camera(void *camera) { this->camera__ = static_cast<esp32_camera::ESP32Camera *>(camera); }

void RTSPServer::dump_config() {
  ESP_LOGCONFIG(TAG, "RTSP Server:");
  ESP_LOGCONFIG(TAG, "  Address: %s:%u", network::get_use_address().c_str(), this->port_);
  ESP_LOGCONFIG(TAG, "  Camera Object: %p", this->camera__);
  ESP_LOGCONFIG(TAG, "  width: %d height: %d ", this->dim_.width, this->dim_.height);
}

float RTSPServer::get_setup_priority() const { return setup_priority::LATE; }
#endif
}  // namespace rtsp_server
}  // namespace esphome
#endif
