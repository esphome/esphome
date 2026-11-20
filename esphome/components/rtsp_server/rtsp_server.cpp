#include "rtsp_server.h"

#ifdef USE_CAMERA

#include <algorithm>
#include <cerrno>
#include <cinttypes>
#include <utility>

#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#ifdef USE_RTSP_SERVER_CAPTURE
#include <esp_timer.h>

#include "esphome/components/audio/audio.h"
#include "esphome/components/microphone/microphone.h"
#endif

namespace esphome::rtsp_server {

ESPHOME_LOG_TAG(TAG, "rtsp_server");

#ifdef USE_RTSP_SERVER_CAPTURE
static constexpr uint8_t RTP_PAYLOAD_TYPE_PCMU = 0;
static constexpr uint8_t RTP_PAYLOAD_TYPE_PCMA = 8;
static constexpr uint8_t RTP_PAYLOAD_TYPE_L16 = 96;  // dynamic, described by the SDP
static constexpr uint32_t G711_SAMPLE_RATE = 8000;
static constexpr size_t G711_SAMPLES_PER_PACKET = G711_SAMPLE_RATE * AUDIO_PACKET_MS / 1000;
// The microphone's capture times may run this far ahead of its sample count before the difference counts as
// audio it lost (a source that stopped and resumed, such as a USB microphone that reconnected).
static constexpr int64_t CAPTURE_JUMP_US = 50000;
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

#ifdef USE_RTSP_SERVER_CAPTURE
  this->setup_audio_();
#endif
  camera::Camera::instance()->add_listener(this);
}

#ifdef USE_RTSP_SERVER_CAPTURE
void RTSPServer::setup_audio_() {
  const audio::AudioStreamInfo info = this->microphone_source_->get_audio_stream_info();
  const uint32_t sample_rate = info.get_sample_rate();
  const bool g711 = this->audio_codec_ != AudioCodec::AUDIO_CODEC_L16;
  if (g711 && sample_rate != G711_SAMPLE_RATE) {
    ESP_LOGE(TAG, "G.711 needs an %" PRIu32 " Hz microphone, not %" PRIu32 " Hz; streaming video only",
             G711_SAMPLE_RATE, sample_rate);
    return;
  }
  const auto samples = static_cast<uint16_t>(sample_rate * AUDIO_PACKET_MS / 1000);
  const auto input_len = static_cast<uint16_t>(info.samples_to_bytes(samples));  // 16-bit mono
  const uint16_t payload_len = g711 ? samples : input_len;
  const uint8_t payload_type = this->audio_codec_ == AudioCodec::AUDIO_CODEC_PCMU   ? RTP_PAYLOAD_TYPE_PCMU
                               : this->audio_codec_ == AudioCodec::AUDIO_CODEC_PCMA ? RTP_PAYLOAD_TYPE_PCMA
                                                                                    : RTP_PAYLOAD_TYPE_L16;
  const size_t sdp_len =
      format_audio_sdp(this->sdp_.data(), this->sdp_.size(), this->audio_codec_, payload_type, sample_rate);

  // Allocated once here and kept, so starting and stopping the stream never touches the heap.
  this->ring_buffer_ = ring_buffer::RingBuffer::create(AUDIO_SLOTS * input_len);
  RAMAllocator<uint8_t> allocator;
  uint8_t *slots = allocator.allocate(AUDIO_SLOTS * payload_len);
  if (this->ring_buffer_ == nullptr || slots == nullptr || sdp_len == 0) {
    ESP_LOGE(TAG, "Audio buffer allocation failed; streaming video only");
    if (slots != nullptr)
      allocator.deallocate(slots, AUDIO_SLOTS * payload_len);
    this->ring_buffer_.reset();
    return;
  }

  this->audio_track_.slots = slots;
  this->audio_track_.sdp = this->sdp_.data();
  this->audio_track_.sdp_len = static_cast<uint16_t>(sdp_len);
  this->audio_track_.payload_len = payload_len;
  this->audio_track_.samples_per_packet = samples;
  this->audio_track_.payload_type = payload_type;
  this->audio_track_.filler = silence_fill(this->audio_codec_);
  for (auto &session : this->sessions_)
    session.set_audio_track(&this->audio_track_);

  this->microphone_source_->add_data_callback(
      [this](const microphone::AudioCapture &capture) { this->on_audio_(capture); });
  this->audio_ready_ = true;
}

void RTSPServer::on_audio_(const microphone::AudioCapture &capture) {
  const audio::AudioStreamInfo &info = capture.stream_info;
  const auto samples = static_cast<uint32_t>(info.bytes_to_samples(capture.data.size()));
  // The esp_timer time of the chunk's first sample; without one, it ended about now.
  const int64_t rate = info.get_sample_rate();
  const int64_t duration_us = static_cast<int64_t>(samples) * 1000000 / rate;
  const bool timed = capture.capture_time_us != 0;
  const int64_t first_us = timed ? capture.capture_time_us : esp_timer_get_time() - duration_us;
  {
    LockGuard guard(this->capture_lock_);
    // A callback that was already running when capture stopped can deliver its chunk after the restart.
    if (timed && first_us + duration_us <= this->restart_us_)
      return;
    // Only real capture times show an outage; delivery times also move with task scheduling.
    if (timed && this->anchored_) {
      const int64_t expected_us =
          this->anchor_us_ + static_cast<int64_t>(this->captured_ - this->anchor_sample_) * 1000000 / rate;
      if (first_us - expected_us > CAPTURE_JUMP_US) {
        const auto lost = static_cast<uint32_t>((first_us - expected_us) * rate / 1000000);
        this->add_gap_(this->captured_, lost);
        this->captured_ += lost;
      }
    }
    // A full ring buffer drops the new audio, so what it holds stays contiguous and each loss has one position.
    const size_t written = this->ring_buffer_->write_without_replacement(capture.data.data(), capture.data.size());
    const auto kept = static_cast<uint32_t>(info.bytes_to_samples(written));
    if (kept < samples)
      this->add_gap_(this->captured_ + kept, samples - kept);
    this->anchor_us_ = first_us;
    this->anchor_sample_ = this->captured_;
    this->captured_ += samples;
    this->anchored_ |= timed;
  }
  // Drain it promptly even with a long loop_interval; the ring buffer holds only 160 ms.
  App.wake_loop_threadsafe();
}

void RTSPServer::add_gap_(uint32_t at, uint32_t len) {
  if (this->gap_count_ > 0) {
    CaptureGap &last = this->gaps_[this->gap_count_ - 1];
    // A loss that follows the last one directly, or one with no room left, extends it; the audio between two
    // merged losses is then placed early by the later one, which only a main loop stalled several times over
    // before it drains the buffer can cause.
    if (last.at + last.len == at || this->gap_count_ == this->gaps_.size()) {
      last.len += len;
      return;
    }
  }
  this->gaps_[this->gap_count_++] = {at, len};
}

void RTSPServer::read_audio_() {
  const audio::AudioStreamInfo info = this->microphone_source_->get_audio_stream_info();
  const uint32_t samples = this->audio_track_.samples_per_packet;
  const size_t input_len = info.samples_to_bytes(samples);
  AudioTrack &track = this->audio_track_;
  while (this->ring_buffer_->available() >= input_len) {
    int64_t anchor_us;
    uint32_t anchor_sample;
    {
      LockGuard guard(this->capture_lock_);
      // A loss that starts within this packet is placed before it, at most a packet early.
      while (this->gap_count_ > 0 &&
             static_cast<int32_t>(this->gaps_[0].at - this->next_sample_) < static_cast<int32_t>(samples)) {
        this->next_sample_ += this->gaps_[0].len;
        std::copy(this->gaps_.begin() + 1, this->gaps_.begin() + this->gap_count_, this->gaps_.begin());
        this->gap_count_--;
      }
      anchor_us = this->anchor_us_;
      anchor_sample = this->anchor_sample_;
    }
    const uint32_t index = track.produced;
    for (auto &session : this->sessions_)
      session.on_audio_slot_reuse(index - AUDIO_SLOTS);
    auto *slot = const_cast<uint8_t *>(track.packet(index));
    if (this->audio_codec_ == AudioCodec::AUDIO_CODEC_L16) {
      if (this->ring_buffer_->read(slot, input_len) != input_len)
        return;
      for (size_t i = 0; i + 1 < input_len; i += 2)
        std::swap(slot[i], slot[i + 1]);  // L16 is big endian (RFC 3551 section 4.5.11)
    } else {
      std::array<int16_t, G711_SAMPLES_PER_PACKET> pcm;
      if (this->ring_buffer_->read(pcm.data(), input_len) != input_len)
        return;
      const bool ulaw = this->audio_codec_ == AudioCodec::AUDIO_CODEC_PCMU;
      for (size_t i = 0; i < pcm.size(); i++)
        slot[i] = ulaw ? encode_ulaw(pcm[i]) : encode_alaw(pcm[i]);
    }
    // Measured from the latest chunk, so the microphone's clock cannot drift away from esp_timer.
    const auto offset = static_cast<int32_t>(this->next_sample_ - anchor_sample);
    const int64_t first_us =
        anchor_us + static_cast<int64_t>(offset) * 1000000 / static_cast<int64_t>(info.get_sample_rate());
    track.capture_ms[index % AUDIO_SLOTS] = static_cast<uint64_t>(first_us / 1000);  // the millis_64() clock
    track.sample_pos[index % AUDIO_SLOTS] = this->next_sample_;
    this->next_sample_ += samples;
    track.produced++;
  }
}
#endif

void RTSPServer::loop() {
  const uint32_t now = App.get_loop_component_start_time();
  if (this->socket_ != nullptr && this->socket_->ready()) {
    this->accept_new_connections_(now);
  }

#ifdef USE_RTSP_SERVER_CAPTURE
  if (this->capturing_)
    this->read_audio_();
  bool audio = false;
#endif
  bool playing = false;
  for (auto &session : this->sessions_) {
    session.loop(now, this->port_);
    playing |= session.is_playing_video();
#ifdef USE_RTSP_SERVER_CAPTURE
    audio |= session.is_playing_audio();
#endif
  }

  if (playing != this->streaming_) {
    this->streaming_ = playing;
    if (playing) {
      camera::Camera::instance()->start_stream(camera::RTSP_REQUESTER);
    } else {
      camera::Camera::instance()->stop_stream(camera::RTSP_REQUESTER);
    }
  }
#ifdef USE_RTSP_SERVER_CAPTURE
  if (audio != this->capturing_) {
    this->capturing_ = audio;
    if (audio) {
      {
        // Under the lock, so a callback still running from before the stop cannot write in between; one that
        // runs after this is dropped by its capture time.
        LockGuard guard(this->capture_lock_);
        this->ring_buffer_->reset();
        this->captured_ = this->next_sample_;
        this->anchor_sample_ = this->next_sample_;
        this->gap_count_ = 0;
        this->anchored_ = false;
        this->restart_us_ = esp_timer_get_time();
      }
      this->microphone_source_->start();
    } else {
      this->microphone_source_->stop();
    }
  }
#endif
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
  const uint64_t time_ms = millis_64();
  for (auto &session : this->sessions_) {
    session.on_new_frame(image, jpeg, now, time_ms);
  }
}

void RTSPServer::dump_config() {
  ESP_LOGCONFIG(TAG,
                "RTSP Server:\n"
                "  Port: %u\n"
                "  Max connections: %u",
                this->port_, MAX_RTSP_CONNECTIONS);
#ifdef USE_SOCKET_IPV4_ALLOW
  this->allow_.dump_config(TAG);
#endif
#ifdef USE_RTSP_SERVER_CAPTURE
  if (this->audio_ready_) {
    ESP_LOGCONFIG(TAG, "  Audio: %s, %" PRIu32 " Hz", LOG_STR_ARG(audio_codec_name(this->audio_codec_)),
                  this->microphone_source_->get_audio_stream_info().get_sample_rate());
  }
#endif
}

float RTSPServer::get_setup_priority() const { return setup_priority::LATE; }

}  // namespace esphome::rtsp_server

#endif
