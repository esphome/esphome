#ifdef USE_ESP32

#include "rtsp_server.h"

#include <cerrno>
#include <utility>

#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::rtsp_server {

static const char *const TAG = "rtsp_server";

void RTSPServer::socket_failed_(const char *what) {
  ESP_LOGE(TAG, "Socket %s failed: errno %d", what, errno);
  delete this->socket_;
  this->socket_ = nullptr;
  this->mark_failed();
}

void RTSPServer::setup() {
  if (camera::Camera::instance() == nullptr) {
    ESP_LOGE(TAG, "No camera configured");
    this->mark_failed();
    return;
  }

  this->socket_ = socket::socket_ip_loop_monitored(SOCK_STREAM, 0).release();
  if (this->socket_ == nullptr) {
    ESP_LOGE(TAG, "Socket creation failed: errno %d", errno);
    this->mark_failed();
    return;
  }

  int enable = 1;
  this->socket_->setsockopt(SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));

  if (this->socket_->setblocking(false) != 0) {
    this->socket_failed_("nonblocking");
    return;
  }

  struct sockaddr_storage server {};
  socklen_t sl = socket::set_sockaddr_any((struct sockaddr *) &server, sizeof(server), this->port_);
  if (sl == 0 || this->socket_->bind((struct sockaddr *) &server, sl) != 0) {
    this->socket_failed_("bind");
    return;
  }

  if (this->socket_->listen(4) != 0) {
    this->socket_failed_("listen");
    return;
  }

  camera::Camera::instance()->add_listener(this);
}

void RTSPServer::loop() {
  if (this->socket_ != nullptr && this->socket_->ready()) {
    this->accept_new_connections_();
  }

  uint8_t i = 0;
  while (i < this->session_count_) {
    auto &session = this->sessions_[i];
    session->loop();

    if (session->should_close()) {
      session->ensure_stream_stopped();

      const uint8_t last_index = this->session_count_ - 1;
      if (i < last_index) {
        std::swap(this->sessions_[i], this->sessions_[last_index]);
      }
      this->session_count_--;
      this->sessions_[last_index].reset();
    } else {
      i++;
    }
  }
}

void RTSPServer::accept_new_connections_() {
  while (true) {
    struct sockaddr_storage source_addr;
    socklen_t addr_len = sizeof(source_addr);

    auto sock = this->socket_->accept_loop_monitored((struct sockaddr *) &source_addr, &addr_len);
    if (!sock)
      break;

    if (this->session_count_ >= MAX_RTSP_SESSIONS) {
      ESP_LOGW(TAG, "Max sessions (%d) reached, rejecting connection", MAX_RTSP_SESSIONS);
      sock.reset();
      continue;
    }

    ESP_LOGD(TAG, "Accepted RTSP connection");
    this->sessions_[this->session_count_++] = make_unique<RTSPSession>(std::move(sock), this, this->port_);
  }
}

void RTSPServer::on_camera_image(const std::shared_ptr<camera::CameraImage> &image) {
  if (!image->was_requested_by(camera::RTSP_REQUESTER))
    return;
  for (uint8_t i = 0; i < this->session_count_; i++) {
    this->sessions_[i]->on_new_frame(image);
  }
}

void RTSPServer::session_started_playing() {
  if (this->playing_count_++ == 0) {
    camera::Camera::instance()->start_stream(camera::RTSP_REQUESTER);
  }
}

void RTSPServer::session_stopped_playing() {
  if (this->playing_count_ == 0)
    return;
  this->playing_count_--;
  if (this->playing_count_ == 0) {
    camera::Camera::instance()->stop_stream(camera::RTSP_REQUESTER);
  }
}

void RTSPServer::dump_config() {
  ESP_LOGCONFIG(TAG,
                "RTSP Server:\n"
                "  Port: %u\n"
                "  Max sessions: %u",
                this->port_, MAX_RTSP_SESSIONS);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "  Setup Failed");
  }
}

float RTSPServer::get_setup_priority() const { return setup_priority::LATE; }

}  // namespace esphome::rtsp_server

#endif
