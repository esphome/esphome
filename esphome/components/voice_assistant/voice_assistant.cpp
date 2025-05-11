#include "voice_assistant.h"
#include "esphome/core/defines.h"

#ifdef USE_VOICE_ASSISTANT

#include "esphome/core/log.h"

#include <cinttypes>
#include <cstdio>

namespace esphome {
namespace voice_assistant {

static const char *const TAG = "voice_assistant";

#ifdef SAMPLE_RATE_HZ
#undef SAMPLE_RATE_HZ
#endif

static const size_t SAMPLE_RATE_HZ = 16000;

static const size_t RING_BUFFER_SAMPLES = 512 * SAMPLE_RATE_HZ / 1000;  // 512 ms * 16 kHz/ 1000 ms
static const size_t RING_BUFFER_SIZE = RING_BUFFER_SAMPLES * sizeof(int16_t);
static const size_t SEND_BUFFER_SAMPLES = 32 * SAMPLE_RATE_HZ / 1000;  // 32ms * 16kHz / 1000ms
static const size_t SEND_BUFFER_SIZE = SEND_BUFFER_SAMPLES * sizeof(int16_t);
static const size_t RECEIVE_SIZE = 1024;
static const size_t SPEAKER_BUFFER_SIZE = 16 * RECEIVE_SIZE;

VoiceAssistant::VoiceAssistant() { global_voice_assistant = this; }

void VoiceAssistant::setup() {
  this->mic_source_->add_data_callback([this](const std::vector<uint8_t> &data) {
    std::shared_ptr<RingBuffer> temp_ring_buffer = this->ring_buffer_;
    if (this->ring_buffer_.use_count() > 1) {
      temp_ring_buffer->write((void *) data.data(), data.size());
    }
  });
}

float VoiceAssistant::get_setup_priority() const { return setup_priority::AFTER_CONNECTION; }

bool VoiceAssistant::start_udp_socket_() {
  this->socket_ = socket::socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
  if (this->socket_ == nullptr) {
    ESP_LOGE(TAG, "Could not create socket");
    this->mark_failed();
    return false;
  }
  int enable = 1;
  int err = this->socket_->setsockopt(SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
  if (err != 0) {
    ESP_LOGW(TAG, "Socket unable to set reuseaddr: errno %d", err);
    // we can still continue
  }
  err = this->socket_->setblocking(false);
  if (err != 0) {
    ESP_LOGE(TAG, "Socket unable to set nonblocking mode: errno %d", err);
    this->mark_failed();
    return false;
  }

#ifdef USE_SPEAKER
  if (this->speaker_ != nullptr) {
    struct sockaddr_storage server;

    socklen_t sl = socket::set_sockaddr_any((struct sockaddr *) &server, sizeof(server), 6055);
    if (sl == 0) {
      ESP_LOGE(TAG, "Socket unable to set sockaddr: errno %d", errno);
      this->mark_failed();
      return false;
    }

    err = this->socket_->bind((struct sockaddr *) &server, sizeof(server));
    if (err != 0) {
      ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
      this->mark_failed();
      return false;
    }
  }
#endif
  this->udp_socket_running_ = true;
  return true;
}

bool VoiceAssistant::allocate_buffers_() {
#ifdef USE_SPEAKER
  if ((this->speaker_ != nullptr) && (this->speaker_buffer_ == nullptr)) {
    ExternalRAMAllocator<uint8_t> speaker_allocator(ExternalRAMAllocator<uint8_t>::ALLOW_FAILURE);
    this->speaker_buffer_ = speaker_allocator.allocate(SPEAKER_BUFFER_SIZE);
    if (this->speaker_buffer_ == nullptr) {
      ESP_LOGW(TAG, "Could not allocate speaker buffer");
      return false;
    }
  }
#endif

  if (this->ring_buffer_.use_count() == 0) {
    this->ring_buffer_ = RingBuffer::create(RING_BUFFER_SIZE);
    if (this->ring_buffer_.use_count() == 0) {
      ESP_LOGE(TAG, "Could not allocate ring buffer");
      return false;
    }
  }

  if (this->send_buffer_ == nullptr) {
    ExternalRAMAllocator<uint8_t> send_allocator(ExternalRAMAllocator<uint8_t>::ALLOW_FAILURE);
    this->send_buffer_ = send_allocator.allocate(SEND_BUFFER_SIZE);
    if (send_buffer_ == nullptr) {
      ESP_LOGW(TAG, "Could not allocate send buffer");
      return false;
    }
  }

  return true;
}

void VoiceAssistant::clear_buffers_() {
  if (this->send_buffer_ != nullptr) {
    memset(this->send_buffer_, 0, SEND_BUFFER_SIZE);
  }

  if (this->ring_buffer_ != nullptr) {
    this->ring_buffer_->reset();
  }

#ifdef USE_SPEAKER
  if ((this->speaker_ != nullptr) && (this->speaker_buffer_ != nullptr)) {
    memset(this->speaker_buffer_, 0, SPEAKER_BUFFER_SIZE);

    this->speaker_buffer_size_ = 0;
    this->speaker_buffer_index_ = 0;
    this->speaker_bytes_received_ = 0;
  }
#endif
}

void VoiceAssistant::deallocate_buffers_() {
  if (this->send_buffer_ != nullptr) {
    ExternalRAMAllocator<uint8_t> send_deallocator(ExternalRAMAllocator<uint8_t>::ALLOW_FAILURE);
    send_deallocator.deallocate(this->send_buffer_, SEND_BUFFER_SIZE);
    this->send_buffer_ = nullptr;
  }

  if (this->ring_buffer_.use_count() > 0) {
    this->ring_buffer_.reset();
  }

#ifdef USE_SPEAKER
  if ((this->speaker_ != nullptr) && (this->speaker_buffer_ != nullptr)) {
    ExternalRAMAllocator<uint8_t> speaker_deallocator(ExternalRAMAllocator<uint8_t>::ALLOW_FAILURE);
    speaker_deallocator.deallocate(this->speaker_buffer_, SPEAKER_BUFFER_SIZE);
    this->speaker_buffer_ = nullptr;
  }
#endif
}

void VoiceAssistant::reset_conversation_id() {
  this->conversation_id_ = "";
  ESP_LOGD(TAG, "reset conversation ID");
}

void VoiceAssistant::loop() {
  if (this->api_client_ == nullptr && this->state_ != State::IDLE && this->state_ != State::STOP_MICROPHONE &&
      this->state_ != State::STOPPING_MICROPHONE) {
    if (this->mic_source_->is_running() || this->state_ == State::STARTING_MICROPHONE) {
      this->set_state_(State::STOP_MICROPHONE, State::IDLE);
    } else {
      this->set_state_(State::IDLE, State::IDLE);
    }
    this->continuous_ = false;
    this->signal_stop_();
    this->clear_buffers_();
    return;
  }
  switch (this->state_) {
    case State::IDLE: {
      if (this->continuous_ && this->desired_state_ == State::IDLE) {
        this->idle_trigger_->trigger();
        this->set_state_(State::START_MICROPHONE, State::START_PIPELINE);
      } else {
        this->deallocate_buffers_();
      }
      break;
    }
    case State::START_MICROPHONE: {
      ESP_LOGD(TAG, "Starting Microphone");
      if (!this->allocate_buffers_()) {
        this->status_set_error("Failed to allocate buffers");
        return;
      }
      if (this->status_has_error()) {
        this->status_clear_error();
      }
      this->clear_buffers_();

      this->mic_source_->start();
      this->set_state_(State::STARTING_MICROPHONE);
      break;
    }
    case State::STARTING_MICROPHONE: {
      if (this->mic_source_->is_running()) {
        this->set_state_(this->desired_state_);
      }
      break;
    }
    case State::START_PIPELINE: {
      ESP_LOGD(TAG, "Requesting start...");
      uint32_t flags = 0;
      if (!this->continue_conversation_ && this->use_wake_word_)
        flags |= api::enums::VOICE_ASSISTANT_REQUEST_USE_WAKE_WORD;
      if (this->silence_detection_)
        flags |= api::enums::VOICE_ASSISTANT_REQUEST_USE_VAD;
      api::VoiceAssistantAudioSettings audio_settings;
      audio_settings.noise_suppression_level = this->noise_suppression_level_;
      audio_settings.auto_gain = this->auto_gain_;
      audio_settings.volume_multiplier = this->volume_multiplier_;

      api::VoiceAssistantRequest msg;
      msg.start = true;
      msg.conversation_id = this->conversation_id_;
      msg.flags = flags;
      msg.audio_settings = audio_settings;
      msg.wake_word_phrase = this->wake_word_;
      this->wake_word_ = "";

      if (this->api_client_ == nullptr || !this->api_client_->send_voice_assistant_request(msg)) {
        ESP_LOGW(TAG, "Could not request start");
        this->error_trigger_->trigger("not-connected", "Could not request start");
        this->continuous_ = false;
        this->set_state_(State::IDLE, State::IDLE);
        break;
      }
      this->set_state_(State::STARTING_PIPELINE);
      this->set_timeout("reset-conversation_id", this->conversation_timeout_,
                        [this]() { this->reset_conversation_id(); });
      break;
    }
    case State::STARTING_PIPELINE: {
      break;  // State changed when udp server port received
    }
    case State::STREAMING_MICROPHONE: {
      size_t available = this->ring_buffer_->available();
      while (available >= SEND_BUFFER_SIZE) {
        size_t read_bytes = this->ring_buffer_->read((void *) this->send_buffer_, SEND_BUFFER_SIZE, 0);
        if (this->audio_mode_ == AUDIO_MODE_API) {
          api::VoiceAssistantAudio msg;
          msg.data.assign((char *) this->send_buffer_, read_bytes);
          this->api_client_->send_voice_assistant_audio(msg);
        } else {
          if (!this->udp_socket_running_) {
            if (!this->start_udp_socket_()) {
              this->set_state_(State::STOP_MICROPHONE, State::IDLE);
              break;
            }
          }
          this->socket_->sendto(this->send_buffer_, read_bytes, 0, (struct sockaddr *) &this->dest_addr_,
                                sizeof(this->dest_addr_));
        }
        available = this->ring_buffer_->available();
      }

      break;
    }
    case State::STOP_MICROPHONE: {
      if (this->mic_source_->is_running()) {
        this->mic_source_->stop();
        this->set_state_(State::STOPPING_MICROPHONE);
      } else {
        this->set_state_(this->desired_state_);
      }
      break;
    }
    case State::STOPPING_MICROPHONE: {
      if (this->mic_source_->is_stopped()) {
        this->set_state_(this->desired_state_);
      }
      break;
    }
    case State::AWAITING_RESPONSE: {
      break;  // State changed by events
    }
    case State::STREAMING_RESPONSE: {
      bool playing = false;
#ifdef USE_SPEAKER
      if (this->speaker_ != nullptr) {
        ssize_t received_len = 0;
        if (this->audio_mode_ == AUDIO_MODE_UDP) {
          if (this->speaker_buffer_index_ + RECEIVE_SIZE < SPEAKER_BUFFER_SIZE) {
            received_len = this->socket_->read(this->speaker_buffer_ + this->speaker_buffer_index_, RECEIVE_SIZE);
            if (received_len > 0) {
              this->speaker_buffer_index_ += received_len;
              this->speaker_buffer_size_ += received_len;
              this->speaker_bytes_received_ += received_len;
            }
          } else {
            ESP_LOGD(TAG, "Receive buffer full");
          }
        }
        // Build a small buffer of audio before sending to the speaker
        bool end_of_stream = this->stream_ended_ && (this->audio_mode_ == AUDIO_MODE_API || received_len < 0);
        if (this->speaker_bytes_received_ > RECEIVE_SIZE * 4 || end_of_stream)
          this->write_speaker_();
        if (this->wait_for_stream_end_) {
          this->cancel_timeout("playing");
          if (end_of_stream) {
            ESP_LOGD(TAG, "End of audio stream received");
            this->cancel_timeout("speaker-timeout");
            this->set_state_(State::RESPONSE_FINISHED, State::RESPONSE_FINISHED);
          }
          break;  // We dont want to timeout here as the STREAM_END event will take care of that.
        }
        playing = this->speaker_->is_running();
      }
#endif
#ifdef USE_MEDIA_PLAYER
      if (this->media_player_ != nullptr) {
        playing = (this->media_player_->state == media_player::MediaPlayerState::MEDIA_PLAYER_STATE_ANNOUNCING);

        if (playing && this->media_player_wait_for_announcement_start_) {
          // Announcement has started playing, wait for it to finish
          this->media_player_wait_for_announcement_start_ = false;
          this->media_player_wait_for_announcement_end_ = true;
        }

        if (!playing && this->media_player_wait_for_announcement_end_) {
          // Announcement has finished playing
          this->media_player_wait_for_announcement_end_ = false;
          this->cancel_timeout("playing");
          ESP_LOGD(TAG, "Announcement finished playing");
          this->set_state_(State::RESPONSE_FINISHED, State::RESPONSE_FINISHED);

          api::VoiceAssistantAnnounceFinished msg;
          msg.success = true;
          this->api_client_->send_voice_assistant_announce_finished(msg);
          break;
        }
      }
#endif
      if (playing) {
        this->start_playback_timeout_();
      }
      break;
    }
    case State::RESPONSE_FINISHED: {
#ifdef USE_SPEAKER
      if (this->speaker_ != nullptr) {
        if (this->speaker_buffer_size_ > 0) {
          this->write_speaker_();
          break;
        }
        if (this->speaker_->has_buffered_data() || this->speaker_->is_running()) {
          break;
        }
        ESP_LOGD(TAG, "Speaker has finished outputting all audio");
        this->speaker_->stop();
        this->cancel_timeout("speaker-timeout");
        this->cancel_timeout("playing");

        this->clear_buffers_();

        this->wait_for_stream_end_ = false;
        this->stream_ended_ = false;

        this->tts_stream_end_trigger_->trigger();
      }
#endif
      if (this->continue_conversation_) {
        this->set_state_(State::START_MICROPHONE, State::START_PIPELINE);
      } else {
        this->set_state_(State::IDLE, State::IDLE);
      }
      break;
    }
    default:
      break;
  }
}

#ifdef USE_SPEAKER
void VoiceAssistant::write_speaker_() {
  if ((this->speaker_ != nullptr) && (this->speaker_buffer_ != nullptr)) {
    if (this->speaker_buffer_size_ > 0) {
      size_t write_chunk = std::min<size_t>(this->speaker_buffer_size_, 4 * 1024);
      size_t written = this->speaker_->play(this->speaker_buffer_, write_chunk);
      if (written > 0) {
        memmove(this->speaker_buffer_, this->speaker_buffer_ + written, this->speaker_buffer_size_ - written);
        this->speaker_buffer_size_ -= written;
        this->speaker_buffer_index_ -= written;
        this->set_timeout("speaker-timeout", 5000, [this]() { this->speaker_->stop(); });
      } else {
        ESP_LOGV(TAG, "Speaker buffer full, trying again next loop");
      }
    }
  }
}
#endif

void VoiceAssistant::client_subscription(api::APIConnection *client, bool subscribe) {
  if (!subscribe) {
    if (this->api_client_ == nullptr || client != this->api_client_) {
      ESP_LOGE(TAG, "Client attempting to unsubscribe that is not the current API Client");
      return;
    }
    this->api_client_ = nullptr;
    this->client_disconnected_trigger_->trigger();
    return;
  }

  if (this->api_client_ != nullptr) {
    ESP_LOGE(TAG, "Multiple API Clients attempting to connect to Voice Assistant");
    ESP_LOGE(TAG, "Current client: %s", this->api_client_->get_client_combined_info().c_str());
    ESP_LOGE(TAG, "New client: %s", client->get_client_combined_info().c_str());
    return;
  }

  this->api_client_ = client;
  this->client_connected_trigger_->trigger();
}

static const LogString *voice_assistant_state_to_string(State state) {
  switch (state) {
    case State::IDLE:
      return LOG_STR("IDLE");
    case State::START_MICROPHONE:
      return LOG_STR("START_MICROPHONE");
    case State::STARTING_MICROPHONE:
      return LOG_STR("STARTING_MICROPHONE");
    case State::WAIT_FOR_VAD:
      return LOG_STR("WAIT_FOR_VAD");
    case State::WAITING_FOR_VAD:
      return LOG_STR("WAITING_FOR_VAD");
    case State::START_PIPELINE:
      return LOG_STR("START_PIPELINE");
    case State::STARTING_PIPELINE:
      return LOG_STR("STARTING_PIPELINE");
    case State::STREAMING_MICROPHONE:
      return LOG_STR("STREAMING_MICROPHONE");
    case State::STOP_MICROPHONE:
      return LOG_STR("STOP_MICROPHONE");
    case State::STOPPING_MICROPHONE:
      return LOG_STR("STOPPING_MICROPHONE");
    case State::AWAITING_RESPONSE:
      return LOG_STR("AWAITING_RESPONSE");
    case State::STREAMING_RESPONSE:
      return LOG_STR("STREAMING_RESPONSE");
    case State::RESPONSE_FINISHED:
      return LOG_STR("RESPONSE_FINISHED");
    default:
      return LOG_STR("UNKNOWN");
  }
};

void VoiceAssistant::set_state_(State state) {
  State old_state = this->state_;
  this->state_ = state;
  ESP_LOGD(TAG, "State changed from %s to %s", LOG_STR_ARG(voice_assistant_state_to_string(old_state)),
           LOG_STR_ARG(voice_assistant_state_to_string(state)));
}

void VoiceAssistant::set_state_(State state, State desired_state) {
  this->set_state_(state);
  this->desired_state_ = desired_state;
  ESP_LOGD(TAG, "Desired state set to %s", LOG_STR_ARG(voice_assistant_state_to_string(desired_state)));
}

void VoiceAssistant::failed_to_start() {
  ESP_LOGE(TAG, "Failed to start server. See Home Assistant logs for more details.");
  this->error_trigger_->trigger("failed-to-start", "Failed to start server. See Home Assistant logs for more details.");
  this->set_state_(State::STOP_MICROPHONE, State::IDLE);
}

void VoiceAssistant::start_streaming() {
  if (this->state_ != State::STARTING_PIPELINE) {
    this->signal_stop_();
    return;
  }

  ESP_LOGD(TAG, "Client started, streaming microphone");
  this->audio_mode_ = AUDIO_MODE_API;

  if (this->mic_source_->is_running()) {
    this->set_state_(State::STREAMING_MICROPHONE, State::STREAMING_MICROPHONE);
  } else {
    this->set_state_(State::START_MICROPHONE, State::STREAMING_MICROPHONE);
  }
}

void VoiceAssistant::start_streaming(struct sockaddr_storage *addr, uint16_t port) {
  if (this->state_ != State::STARTING_PIPELINE) {
    this->signal_stop_();
    return;
  }

  ESP_LOGD(TAG, "Client started, streaming microphone");
  this->audio_mode_ = AUDIO_MODE_UDP;

  memcpy(&this->dest_addr_, addr, sizeof(this->dest_addr_));
  if (this->dest_addr_.ss_family == AF_INET) {
    ((struct sockaddr_in *) &this->dest_addr_)->sin_port = htons(port);
  }
#if LWIP_IPV6
  else if (this->dest_addr_.ss_family == AF_INET6) {
    ((struct sockaddr_in6 *) &this->dest_addr_)->sin6_port = htons(port);
  }
#endif
  else {
    ESP_LOGW(TAG, "Unknown address family: %d", this->dest_addr_.ss_family);
    return;
  }

  if (this->mic_source_->is_running()) {
    this->set_state_(State::STREAMING_MICROPHONE, State::STREAMING_MICROPHONE);
  } else {
    this->set_state_(State::START_MICROPHONE, State::STREAMING_MICROPHONE);
  }
}

void VoiceAssistant::request_start(bool continuous, bool silence_detection) {
  if (this->api_client_ == nullptr) {
    ESP_LOGE(TAG, "No API client connected");
    this->set_state_(State::IDLE, State::IDLE);
    this->continuous_ = false;
    return;
  }
  if (this->state_ == State::IDLE) {
    this->continuous_ = continuous;
    this->silence_detection_ = silence_detection;

    this->set_state_(State::START_MICROPHONE, State::START_PIPELINE);
  }
}

void VoiceAssistant::request_stop() {
  this->continuous_ = false;
  this->continue_conversation_ = false;

  switch (this->state_) {
    case State::IDLE:
      break;
    case State::START_MICROPHONE:
    case State::STARTING_MICROPHONE:
    case State::WAIT_FOR_VAD:
    case State::WAITING_FOR_VAD:
    case State::START_PIPELINE:
      this->set_state_(State::STOP_MICROPHONE, State::IDLE);
      break;
    case State::STARTING_PIPELINE:
    case State::STREAMING_MICROPHONE:
      this->signal_stop_();
      this->set_state_(State::STOP_MICROPHONE, State::IDLE);
      break;
    case State::STOP_MICROPHONE:
    case State::STOPPING_MICROPHONE:
      this->desired_state_ = State::IDLE;
      break;
    case State::AWAITING_RESPONSE:
      this->signal_stop_();
      break;
    case State::STREAMING_RESPONSE:
#ifdef USE_MEDIA_PLAYER
      // Stop any ongoing media player announcement
      if (this->media_player_ != nullptr) {
        this->media_player_->make_call()
            .set_command(media_player::MEDIA_PLAYER_COMMAND_STOP)
            .set_announcement(true)
            .perform();
      }
#endif
      break;
    case State::RESPONSE_FINISHED:
      break;  // Let the incoming audio stream finish then it will go to idle.
  }
}

void VoiceAssistant::signal_stop_() {
  memset(&this->dest_addr_, 0, sizeof(this->dest_addr_));
  if (this->api_client_ == nullptr) {
    return;
  }
  ESP_LOGD(TAG, "Signaling stop...");
  api::VoiceAssistantRequest msg;
  msg.start = false;
  this->api_client_->send_voice_assistant_request(msg);
}

void VoiceAssistant::start_playback_timeout_() {
  this->set_timeout("playing", 2000, [this]() {
    this->cancel_timeout("speaker-timeout");
    this->set_state_(State::RESPONSE_FINISHED, State::RESPONSE_FINISHED);

    api::VoiceAssistantAnnounceFinished msg;
    msg.success = true;
    this->api_client_->send_voice_assistant_announce_finished(msg);
  });
}

void VoiceAssistant::on_event(const api::VoiceAssistantEventResponse &msg) {
  ESP_LOGD(TAG, "Event Type: %" PRId32, msg.event_type);
  switch (msg.event_type) {
    case api::enums::VOICE_ASSISTANT_RUN_START:
      ESP_LOGD(TAG, "Assist Pipeline running");
      this->defer([this]() { this->start_trigger_->trigger(); });
      break;
    case api::enums::VOICE_ASSISTANT_WAKE_WORD_START:
      break;
    case api::enums::VOICE_ASSISTANT_WAKE_WORD_END: {
      ESP_LOGD(TAG, "Wake word detected");
      this->defer([this]() { this->wake_word_detected_trigger_->trigger(); });
      break;
    }
    case api::enums::VOICE_ASSISTANT_STT_START:
      ESP_LOGD(TAG, "STT started");
      this->defer([this]() { this->listening_trigger_->trigger(); });
      break;
    case api::enums::VOICE_ASSISTANT_STT_END: {
      std::string text;
      for (auto arg : msg.data) {
        if (arg.name == "text") {
          text = std::move(arg.value);
        }
      }
      if (text.empty()) {
        ESP_LOGW(TAG, "No text in STT_END event");
        return;
      }
      ESP_LOGD(TAG, "Speech recognised as: \"%s\"", text.c_str());
      this->defer([this, text]() { this->stt_end_trigger_->trigger(text); });
      break;
    }
    case api::enums::VOICE_ASSISTANT_INTENT_START:
      ESP_LOGD(TAG, "Intent started");
      this->defer([this]() { this->intent_start_trigger_->trigger(); });
      break;
    case api::enums::VOICE_ASSISTANT_INTENT_END: {
      for (auto arg : msg.data) {
        if (arg.name == "conversation_id") {
          this->conversation_id_ = std::move(arg.value);
        } else if (arg.name == "continue_conversation") {
          this->continue_conversation_ = (arg.value == "1");
        }
      }
      this->defer([this]() { this->intent_end_trigger_->trigger(); });
      break;
    }
    case api::enums::VOICE_ASSISTANT_TTS_START: {
      std::string text;
      for (auto arg : msg.data) {
        if (arg.name == "text") {
          text = std::move(arg.value);
        }
      }
      if (text.empty()) {
        ESP_LOGW(TAG, "No text in TTS_START event");
        return;
      }
      ESP_LOGD(TAG, "Response: \"%s\"", text.c_str());
      this->defer([this, text]() {
        this->tts_start_trigger_->trigger(text);
#ifdef USE_SPEAKER
        if (this->speaker_ != nullptr) {
          this->speaker_->start();
        }
#endif
      });
      break;
    }
    case api::enums::VOICE_ASSISTANT_TTS_END: {
      std::string url;
      for (auto arg : msg.data) {
        if (arg.name == "url") {
          url = std::move(arg.value);
        }
      }
      if (url.empty()) {
        ESP_LOGW(TAG, "No url in TTS_END event");
        return;
      }
      ESP_LOGD(TAG, "Response URL: \"%s\"", url.c_str());
      this->defer([this, url]() {
#ifdef USE_MEDIA_PLAYER
        if (this->media_player_ != nullptr) {
          this->media_player_->make_call().set_media_url(url).set_announcement(true).perform();

          this->media_player_wait_for_announcement_start_ = true;
          this->media_player_wait_for_announcement_end_ = false;
          // Start the playback timeout, as the media player state isn't immediately updated
          this->start_playback_timeout_();
        }
#endif
        this->tts_end_trigger_->trigger(url);
      });
      State new_state = this->local_output_ ? State::STREAMING_RESPONSE : State::IDLE;
      this->set_state_(new_state, new_state);
      break;
    }
    case api::enums::VOICE_ASSISTANT_RUN_END: {
      ESP_LOGD(TAG, "Assist Pipeline ended");
      if ((this->state_ == State::START_PIPELINE) || (this->state_ == State::STARTING_PIPELINE) ||
          (this->state_ == State::STREAMING_MICROPHONE)) {
        // Microphone is running, stop it
        this->set_state_(State::STOP_MICROPHONE, State::IDLE);
      } else if (this->state_ == State::AWAITING_RESPONSE) {
        // No TTS start event ("nevermind")
        this->set_state_(State::IDLE, State::IDLE);
      }
      this->defer([this]() { this->end_trigger_->trigger(); });
      break;
    }
    case api::enums::VOICE_ASSISTANT_ERROR: {
      std::string code = "";
      std::string message = "";
      for (auto arg : msg.data) {
        if (arg.name == "code") {
          code = std::move(arg.value);
        } else if (arg.name == "message") {
          message = std::move(arg.value);
        }
      }
      if (code == "wake-word-timeout" || code == "wake_word_detection_aborted" || code == "no_wake_word") {
        // Don't change state here since either the "tts-end" or "run-end" events will do it.
        return;
      } else if (code == "wake-provider-missing" || code == "wake-engine-missing") {
        // Wake word is not set up or not ready on Home Assistant so stop and do not retry until user starts again.
        this->defer([this, code, message]() {
          this->request_stop();
          this->error_trigger_->trigger(code, message);
        });
        return;
      }
      ESP_LOGE(TAG, "Error: %s - %s", code.c_str(), message.c_str());
      if (this->state_ != State::IDLE) {
        this->signal_stop_();
        this->set_state_(State::STOP_MICROPHONE, State::IDLE);
      }
      this->defer([this, code, message]() { this->error_trigger_->trigger(code, message); });
      break;
    }
    case api::enums::VOICE_ASSISTANT_TTS_STREAM_START: {
#ifdef USE_SPEAKER
      if (this->speaker_ != nullptr) {
        this->wait_for_stream_end_ = true;
        ESP_LOGD(TAG, "TTS stream start");
        this->defer([this] { this->tts_stream_start_trigger_->trigger(); });
      }
#endif
      break;
    }
    case api::enums::VOICE_ASSISTANT_TTS_STREAM_END: {
#ifdef USE_SPEAKER
      if (this->speaker_ != nullptr) {
        this->stream_ended_ = true;
        ESP_LOGD(TAG, "TTS stream end");
      }
#endif
      break;
    }
    case api::enums::VOICE_ASSISTANT_STT_VAD_START:
      ESP_LOGD(TAG, "Starting STT by VAD");
      this->defer([this]() { this->stt_vad_start_trigger_->trigger(); });
      break;
    case api::enums::VOICE_ASSISTANT_STT_VAD_END:
      ESP_LOGD(TAG, "STT by VAD end");
      this->set_state_(State::STOP_MICROPHONE, State::AWAITING_RESPONSE);
      this->defer([this]() { this->stt_vad_end_trigger_->trigger(); });
      break;
    default:
      ESP_LOGD(TAG, "Unhandled event type: %" PRId32, msg.event_type);
      break;
  }
}

void VoiceAssistant::on_audio(const api::VoiceAssistantAudio &msg) {
#ifdef USE_SPEAKER  // We should never get to this function if there is no speaker anyway
  if ((this->speaker_ != nullptr) && (this->speaker_buffer_ != nullptr)) {
    if (this->speaker_buffer_index_ + msg.data.length() < SPEAKER_BUFFER_SIZE) {
      memcpy(this->speaker_buffer_ + this->speaker_buffer_index_, msg.data.data(), msg.data.length());
      this->speaker_buffer_index_ += msg.data.length();
      this->speaker_buffer_size_ += msg.data.length();
      this->speaker_bytes_received_ += msg.data.length();
      ESP_LOGV(TAG, "Received audio: %u bytes from API", msg.data.length());
    } else {
      ESP_LOGE(TAG, "Cannot receive audio, buffer is full");
    }
  }
#endif
}

void VoiceAssistant::on_timer_event(const api::VoiceAssistantTimerEventResponse &msg) {
  Timer timer = {
      .id = msg.timer_id,
      .name = msg.name,
      .total_seconds = msg.total_seconds,
      .seconds_left = msg.seconds_left,
      .is_active = msg.is_active,
  };
  this->timers_[timer.id] = timer;
  ESP_LOGD(TAG, "Timer Event");
  ESP_LOGD(TAG, "  Type: %" PRId32, msg.event_type);
  ESP_LOGD(TAG, "  %s", timer.to_string().c_str());

  switch (msg.event_type) {
    case api::enums::VOICE_ASSISTANT_TIMER_STARTED:
      this->timer_started_trigger_->trigger(timer);
      break;
    case api::enums::VOICE_ASSISTANT_TIMER_UPDATED:
      this->timer_updated_trigger_->trigger(timer);
      break;
    case api::enums::VOICE_ASSISTANT_TIMER_CANCELLED:
      this->timer_cancelled_trigger_->trigger(timer);
      this->timers_.erase(timer.id);
      break;
    case api::enums::VOICE_ASSISTANT_TIMER_FINISHED:
      this->timer_finished_trigger_->trigger(timer);
      this->timers_.erase(timer.id);
      break;
  }

  if (this->timers_.empty()) {
    this->cancel_interval("timer-event");
    this->timer_tick_running_ = false;
  } else if (!this->timer_tick_running_) {
    this->set_interval("timer-event", 1000, [this]() { this->timer_tick_(); });
    this->timer_tick_running_ = true;
  }
}

void VoiceAssistant::timer_tick_() {
  std::vector<Timer> res;
  res.reserve(this->timers_.size());
  for (auto &pair : this->timers_) {
    auto &timer = pair.second;
    if (timer.is_active && timer.seconds_left > 0) {
      timer.seconds_left--;
    }
    res.push_back(timer);
  }
  this->timer_tick_trigger_->trigger(res);
}

void VoiceAssistant::on_announce(const api::VoiceAssistantAnnounceRequest &msg) {
#ifdef USE_MEDIA_PLAYER
  if (this->media_player_ != nullptr) {
    this->tts_start_trigger_->trigger(msg.text);
    if (!msg.preannounce_media_id.empty()) {
      this->media_player_->make_call().set_media_url(msg.preannounce_media_id).set_announcement(true).perform();
    }
    // Enqueueing a URL with an empty playlist will still play the file immediately
    this->media_player_->make_call()
        .set_command(media_player::MEDIA_PLAYER_COMMAND_ENQUEUE)
        .set_media_url(msg.media_id)
        .set_announcement(true)
        .perform();
    this->continue_conversation_ = msg.start_conversation;

    this->media_player_wait_for_announcement_start_ = true;
    this->media_player_wait_for_announcement_end_ = false;
    // Start the playback timeout, as the media player state isn't immediately updated
    this->start_playback_timeout_();

    if (this->continuous_) {
      this->set_state_(State::STOP_MICROPHONE, State::STREAMING_RESPONSE);
    } else {
      this->set_state_(State::STREAMING_RESPONSE, State::STREAMING_RESPONSE);
    }

    this->tts_end_trigger_->trigger(msg.media_id);
    this->end_trigger_->trigger();
  }
#endif
}

void VoiceAssistant::on_set_configuration(const std::vector<std::string> &active_wake_words) {
#ifdef USE_MICRO_WAKE_WORD
  if (this->micro_wake_word_) {
    // Disable all wake words first
    for (auto &model : this->micro_wake_word_->get_wake_words()) {
      model->disable();
    }

    // Enable only active wake words
    for (auto ww_id : active_wake_words) {
      for (auto &model : this->micro_wake_word_->get_wake_words()) {
        if (model->get_id() == ww_id) {
          model->enable();
          ESP_LOGD(TAG, "Enabled wake word: %s (id=%s)", model->get_wake_word().c_str(), model->get_id().c_str());
        }
      }
    }
  }
#endif
};

const Configuration &VoiceAssistant::get_configuration() {
  this->config_.available_wake_words.clear();
  this->config_.active_wake_words.clear();

#ifdef USE_MICRO_WAKE_WORD
  if (this->micro_wake_word_) {
    this->config_.max_active_wake_words = 1;

    for (auto &model : this->micro_wake_word_->get_wake_words()) {
      if (model->is_enabled()) {
        this->config_.active_wake_words.push_back(model->get_id());
      }

      WakeWord wake_word;
      wake_word.id = model->get_id();
      wake_word.wake_word = model->get_wake_word();
      for (const auto &lang : model->get_trained_languages()) {
        wake_word.trained_languages.push_back(lang);
      }
      this->config_.available_wake_words.push_back(std::move(wake_word));
    }
  } else {
#endif
    // No microWakeWord
    this->config_.max_active_wake_words = 0;
#ifdef USE_MICRO_WAKE_WORD
  }
#endif

  return this->config_;
};

VoiceAssistant *global_voice_assistant = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace voice_assistant
}  // namespace esphome

#endif  // USE_VOICE_ASSISTANT
