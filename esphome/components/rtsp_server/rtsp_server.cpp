#include "rtsp_server.h"

#ifdef USE_CAMERA

#include <cerrno>
#include <cinttypes>
#include <span>
#include <utility>

#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome::rtsp_server {

ESPHOME_LOG_TAG(TAG, "rtsp_server");

#ifdef USE_SOCKET_IPV4_ALLOW
static constexpr uint32_t REJECT_LOG_INTERVAL_MS = 5000;
#endif

void RTSPServer::socket_failed_(const char *what) {
  ESP_LOGE(TAG, "Socket %s failed: errno %d", what, errno);
  delete this->socket_;
  this->socket_ = nullptr;
  this->mark_failed();
}

void RTSPServer::setup() {
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
  const uint32_t now = App.get_loop_component_start_time();
  if (this->socket_ != nullptr && this->socket_->ready()) {
    this->accept_new_connections_(now);
  }

  bool playing = false;
  for (auto &session : this->sessions_) {
    session.loop(now, this->port_);
    playing |= session.is_playing();
  }

  if (playing != this->streaming_) {
    this->streaming_ = playing;
    if (playing) {
      camera::Camera::instance()->start_stream(camera::RTSP_REQUESTER);
    } else {
      camera::Camera::instance()->stop_stream(camera::RTSP_REQUESTER);
    }
  }
}

void RTSPServer::accept_new_connections_(uint32_t now) {
  while (true) {
    struct sockaddr_storage source_addr;
    socklen_t addr_len = sizeof(source_addr);

    auto sock = this->socket_->accept_loop_monitored((struct sockaddr *) &source_addr, &addr_len);
    if (!sock)
      break;
#ifdef USE_SOCKET_IPV4_ALLOW
    if (!this->allow_.allows(reinterpret_cast<const struct sockaddr *>(&source_addr))) {
      if (this->last_reject_log_ms_ == 0 || now - this->last_reject_log_ms_ >= REJECT_LOG_INTERVAL_MS) {
        this->last_reject_log_ms_ = now;
        char text[socket::SOCKADDR_STR_LEN];
        socket::format_sockaddr_to(reinterpret_cast<const struct sockaddr *>(&source_addr), addr_len,
                                   std::span<char, socket::SOCKADDR_STR_LEN>(text));
        ESP_LOGW(TAG, "Rejected %s", text);
      }
      continue;
    }
#endif

    RTSPSession *free_slot = nullptr;
    for (auto &session : this->sessions_) {
      if (!session.is_active()) {
        free_slot = &session;
        break;
      }
    }
    if (free_slot == nullptr) {
      ESP_LOGW(TAG, "Max connections (%d) reached, rejecting connection", MAX_RTSP_CONNECTIONS);
      continue;
    }

    ESP_LOGD(TAG, "Accepted RTSP connection");
    free_slot->open(std::move(sock), now);
  }
}

void RTSPServer::on_camera_image(const std::shared_ptr<camera::CameraImage> &image) {
  if (!this->streaming_ || !image->was_requested_by(camera::RTSP_REQUESTER))
    return;

  JpegInfo jpeg;
  if (!parse_jpeg_info(image->get_data_buffer(), image->get_data_length(), jpeg) ||
      jpeg.width > RTP_JPEG_MAX_DIMENSION || jpeg.height > RTP_JPEG_MAX_DIMENSION) {
    if (!this->frame_warned_) {
      this->frame_warned_ = true;
      ESP_LOGW(TAG,
               "Dropping frame %" PRIu16 "x%" PRIu16 ": not a baseline 4:2:2/4:2:0 JPEG without restart markers"
               " of at most %" PRIu16 " px per side",
               jpeg.width, jpeg.height, RTP_JPEG_MAX_DIMENSION);
    }
    return;
  }

  const uint32_t now = App.get_loop_component_start_time();
  for (auto &session : this->sessions_) {
    session.on_new_frame(image, jpeg, now);
  }
}

void RTSPServer::dump_config() {
  ESP_LOGCONFIG(TAG,
                "RTSP Server:\n"
                "  Port: %u\n"
                "  Max connections: %u",
                this->port_, MAX_RTSP_CONNECTIONS);
#ifdef USE_SOCKET_IPV4_ALLOW
  for (size_t i = 0; i < this->allow_.size(); i++) {
    const socket::Ipv4AllowEntry e = this->allow_.entry(i);
    // Network order is dotted order, and the contiguous mask's popcount is the prefix
    const auto *b = reinterpret_cast<const uint8_t *>(&e.addr);
    ESP_LOGCONFIG(TAG, "  Allowed IP: %u.%u.%u.%u/%u", b[0], b[1], b[2], b[3],
                  static_cast<unsigned>(__builtin_popcount(e.mask)));
  }
#endif
}

float RTSPServer::get_setup_priority() const { return setup_priority::LATE; }

}  // namespace esphome::rtsp_server

#endif
