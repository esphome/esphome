#include "rtsp_server.h"

#ifdef USE_CAMERA

#include <cerrno>
#include <cinttypes>
#include <utility>

#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome::rtsp_server {

ESPHOME_LOG_TAG(TAG, "rtsp_server");

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

  struct sockaddr_storage server{};
  socklen_t sl = socket::set_sockaddr_any((struct sockaddr *) &server, sizeof(server), this->port_);
  if (sl == 0 || this->socket_->bind((struct sockaddr *) &server, sl) != 0) {
    this->socket_failed_("bind");
    return;
  }

  if (this->socket_->listen(4) != 0) {
    this->socket_failed_("listen");
    return;
  }

  if (this->stream_ == nullptr) {
    // The legacy singleton camera is retained for configurations without stream_id.
    // The adapter registers permanently (camera::Camera does not offer remove_listener).
    this->jpeg_adapter_.reset(new camera_video::LegacyJpegStreamAdapter(camera::Camera::instance(), 1, 1, 30));
    this->stream_ = this->jpeg_adapter_.get();
  }
  // Explicit encoded producers may not know their format until the first input frame.
  // Prime without consulting get_format(), otherwise an uninitialized H.264 encoder
  // cannot start and cannot produce the SPS/PPS needed for DESCRIBE.
  if (this->jpeg_adapter_ == nullptr && this->stream_ != nullptr) {
    this->priming_ = true;
    this->set_subscription_(true);
  }
}

void RTSPServer::loop() {
  const uint32_t now = App.get_loop_component_start_time();
  bool ready = this->stream_ != nullptr && this->stream_->is_ready();
  camera_video::EncodedVideoFormat current_format;
  if (ready && this->stream_->get_format(&current_format) && current_format.codec == camera_video::VideoCodec::H264) {
    size_t config_length = 0;
    ready = this->stream_->get_codec_config(nullptr, 0, &config_length) && config_length != 0;
  }
  if (this->priming_ && ready) {
    this->priming_ = false;
    this->set_subscription_(false);
  }
  if (this->socket_ != nullptr && this->socket_->ready() && ready)
    this->accept_new_connections_(now);
  bool playing = false;
  for (auto &session : this->sessions_) {
    session.loop(now, this->port_);
    playing |= session.is_playing();
  }
  if (this->discontinuity_.exchange(false, std::memory_order_acq_rel)) {
    for (auto &session : this->sessions_)
      session.on_discontinuity();
  }
  auto frame = this->take_pending_frame_();
  if (frame != nullptr && playing)
    for (auto &session : this->sessions_)
      session.on_encoded_frame(frame, now);
  if (playing != this->streaming_) {
    this->streaming_ = playing;
    this->set_subscription_(playing);
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
    if (!this->allow_.admit(reinterpret_cast<const struct sockaddr *>(&source_addr), addr_len, TAG, now))
      continue;
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
    free_slot->open(std::move(sock), now, this->stream_, this->payload_size_);
  }
}

void RTSPServer::on_encoded_frame(const camera_video::FrameRef<camera_video::EncodedVideoFrame> &frame) {
  if (frame == nullptr || !this->subscribed_)
    return;
  if (!this->frame_mutex_.try_lock()) {
    this->discontinuity_.store(true, std::memory_order_release);
    return;
  }
  if (this->pending_frame_ != nullptr)
    this->discontinuity_.store(true, std::memory_order_release);
  this->pending_frame_ = frame;
  this->frame_mutex_.unlock();
}

void RTSPServer::set_subscription_(bool enable) {
  if (this->stream_ == nullptr || enable == this->subscribed_)
    return;
  if (enable) {
    if (this->stream_->start_stream(this)) {
      this->subscribed_ = true;
    } else {
      ESP_LOGE(TAG, "RTSP stream subscription rejected");
    }
  } else {
    this->stream_->stop_stream(this);
    this->subscribed_ = false;
    this->clear_pending_frame_();
  }
}

camera_video::FrameRef<camera_video::EncodedVideoFrame> RTSPServer::take_pending_frame_() {
  LockGuard lock(this->frame_mutex_);
  camera_video::FrameRef<camera_video::EncodedVideoFrame> frame;
  frame.swap(this->pending_frame_);
  return frame;
}

void RTSPServer::clear_pending_frame_() {
  LockGuard lock(this->frame_mutex_);
  this->pending_frame_.reset();
}

void RTSPServer::dump_config() {
  ESP_LOGCONFIG(TAG,
                "RTSP Server:\n"
                "  Port: %u\n"
                "  Max connections: %u",
                this->port_, MAX_RTSP_CONNECTIONS);
  camera_video::EncodedVideoFormat format;
  if (this->stream_ != nullptr && this->stream_->get_format(&format)) {
    ESP_LOGCONFIG(TAG, "  Media: %s/RTP",
                  format.codec == camera_video::VideoCodec::H264 ? LOG_STR_LITERAL("H.264") : LOG_STR_LITERAL("JPEG"));
  }
#ifdef USE_SOCKET_IPV4_ALLOW
  this->allow_.dump_config(TAG);
#endif
}

void RTSPServer::on_shutdown() { this->set_subscription_(false); }

float RTSPServer::get_setup_priority() const { return setup_priority::LATE; }

}  // namespace esphome::rtsp_server
#endif
