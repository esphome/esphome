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

#ifdef USE_RTSP_H264
  if (this->h264_stream_ != nullptr) {
    if (!this->h264_stream_->start_stream(this)) {
      ESP_LOGE(TAG, "H.264 stream rejected RTSP metadata subscription");
      this->mark_failed();
      return;
    }
    this->h264_subscribed_ = true;
    this->h264_priming_ = true;
    return;
  }
#endif
  camera::Camera::instance()->add_listener(this);
}

void RTSPServer::loop() {
  const uint32_t now = App.get_loop_component_start_time();
#ifdef USE_RTSP_H264
  if (this->h264_stream_ != nullptr) {
    this->update_h264_metadata_();
  }
#endif
  if (this->socket_ != nullptr && this->socket_->ready()
#ifdef USE_RTSP_H264
      && (this->h264_stream_ == nullptr || this->h264_ready_)
#endif
  ) {
    this->accept_new_connections_(now);
  }

  bool playing = false;
  for (auto &session : this->sessions_) {
    session.loop(now, this->port_);
    playing |= session.is_playing();
  }

#ifdef USE_RTSP_H264
  if (this->h264_stream_ != nullptr) {
    this->update_h264_subscription_(playing);
    if (this->h264_discontinuity_.exchange(false, std::memory_order_acq_rel)) {
      for (auto &session : this->sessions_)
        session.on_h264_discontinuity();
    }
    auto frame = this->take_pending_h264_frame_();
    if (frame != nullptr && playing) {
      camera_video::H264CodecConfig config;
      const camera_video::H264CodecConfig *config_ptr = nullptr;
      if (frame->is_keyframe() && this->h264_stream_->get_h264_codec_config(&config)) {
        config_ptr = &config;
      }
      for (auto &session : this->sessions_) {
        session.on_new_h264_frame(frame, config_ptr, now);
      }
    }
    return;
  }
#endif

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
#ifdef USE_RTSP_H264
    free_slot->open(std::move(sock), now, this->h264_stream_, this->h264_payload_size_);
#else
    free_slot->open(std::move(sock), now);
#endif
  }
}

void RTSPServer::on_camera_image(const std::shared_ptr<camera::CameraImage> &image) {
#ifdef USE_RTSP_H264
  if (this->h264_stream_ != nullptr)
    return;
#endif
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

#ifdef USE_RTSP_H264
void RTSPServer::on_h264_frame(const camera_video::FrameRef<camera_video::H264Frame> &frame) {
  if (frame == nullptr || !this->h264_subscribed_)
    return;
  if (!this->h264_frame_mutex_.try_lock()) {
    this->h264_discontinuity_.store(true, std::memory_order_release);
    return;
  }
  if (this->pending_h264_frame_ != nullptr)
    this->h264_discontinuity_.store(true, std::memory_order_release);
  this->pending_h264_frame_ = frame;
  this->h264_frame_mutex_.unlock();
}

void RTSPServer::update_h264_metadata_() {
  if (this->h264_ready_ || this->h264_stream_ == nullptr)
    return;
  camera_video::H264CodecConfig config;
  if (!this->h264_stream_->get_h264_codec_config(&config))
    return;
  this->h264_ready_ = true;
  ESP_LOGI(TAG, "H.264 track metadata ready; RTSP DESCRIBE ready");
  if (this->h264_priming_ && !this->streaming_ && this->h264_subscribed_) {
    this->h264_stream_->stop_stream(this);
    this->h264_subscribed_ = false;
    this->h264_priming_ = false;
    this->clear_pending_h264_frame_();
  }
}

void RTSPServer::update_h264_subscription_(bool playing) {
  if (playing == this->streaming_)
    return;
  if (playing) {
    this->h264_priming_ = false;
    if (!this->h264_subscribed_) {
      if (!this->h264_stream_->start_stream(this)) {
        ESP_LOGE(TAG, "H.264 stream rejected RTSP subscription");
        return;
      }
      this->h264_subscribed_ = true;
    }
    this->streaming_ = true;
  } else {
    this->streaming_ = false;
    if (this->h264_subscribed_) {
      this->h264_stream_->stop_stream(this);
      this->h264_subscribed_ = false;
    }
    this->clear_pending_h264_frame_();
  }
}

camera_video::FrameRef<camera_video::H264Frame> RTSPServer::take_pending_h264_frame_() {
  LockGuard lock(this->h264_frame_mutex_);
  camera_video::FrameRef<camera_video::H264Frame> frame;
  frame.swap(this->pending_h264_frame_);
  return frame;
}

void RTSPServer::clear_pending_h264_frame_() {
  LockGuard lock(this->h264_frame_mutex_);
  this->pending_h264_frame_.reset();
}
#endif

void RTSPServer::dump_config() {
  ESP_LOGCONFIG(TAG,
                "RTSP Server:\n"
                "  Port: %u\n"
                "  Max connections: %u",
                this->port_, MAX_RTSP_CONNECTIONS);
#ifdef USE_RTSP_H264
  if (this->h264_stream_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  Media: H.264/RTP (payload type 96, max payload %zu B)", this->h264_payload_size_);
  } else
#endif
  {
    ESP_LOGCONFIG(TAG, "  Media: JPEG/RTP (payload type 26)");
  }
#ifdef USE_SOCKET_IPV4_ALLOW
  this->allow_.dump_config(TAG);
#endif
}

void RTSPServer::on_shutdown() {
#ifdef USE_RTSP_H264
  if (this->h264_stream_ != nullptr && this->h264_subscribed_) {
    this->h264_stream_->stop_stream(this);
    this->h264_subscribed_ = false;
    this->clear_pending_h264_frame_();
  }
#endif
}

float RTSPServer::get_setup_priority() const { return setup_priority::LATE; }

}  // namespace esphome::rtsp_server
#endif
