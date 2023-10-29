#include "voice_assistant.h"

#ifdef USE_VOICE_ASSISTANT

#include "esphome/core/log.h"

#include <cstdio>

namespace esphome {
namespace voice_assistant {

static const char *const TAG = "voice_assistant";

#ifdef SAMPLE_RATE_HZ
#undef SAMPLE_RATE_HZ
#endif

static const size_t SAMPLE_RATE_HZ = 16000;
static const size_t INPUT_BUFFER_SIZE = 32 * SAMPLE_RATE_HZ / 1000;  // 32ms * 16kHz / 1000ms
static const size_t BUFFER_SIZE = 1000 * SAMPLE_RATE_HZ / 1000;      // 1s
static const size_t SEND_BUFFER_SIZE = INPUT_BUFFER_SIZE * sizeof(int16_t);
static const size_t RECEIVE_SIZE = 1024;
static const size_t SPEAKER_BUFFER_SIZE = 16 * RECEIVE_SIZE;

float VoiceAssistant::get_setup_priority() const { return setup_priority::AFTER_CONNECTION; }

void VoiceAssistant::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Voice Assistant...");

  global_voice_assistant = this;

  this->socket_ = socket::socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
  if (socket_ == nullptr) {
    ESP_LOGW(TAG, "Could not create socket.");
    this->mark_failed();
    return;
  }
  int enable = 1;
  int err = socket_->setsockopt(SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
  if (err != 0) {
    ESP_LOGW(TAG, "Socket unable to set reuseaddr: errno %d", err);
    // we can still continue
  }
  err = socket_->setblocking(false);
  if (err != 0) {
    ESP_LOGW(TAG, "Socket unable to set nonblocking mode: errno %d", err);
    this->mark_failed();
    return;
  }

#ifdef USE_SPEAKER
  if (this->speaker_ != nullptr) {
    struct sockaddr_storage server;

    socklen_t sl = socket::set_sockaddr_any((struct sockaddr *) &server, sizeof(server), 6055);
    if (sl == 0) {
      ESP_LOGW(TAG, "Socket unable to set sockaddr: errno %d", errno);
      this->mark_failed();
      return;
    }

    err = socket_->bind((struct sockaddr *) &server, sizeof(server));
    if (err != 0) {
      ESP_LOGW(TAG, "Socket unable to bind: errno %d", errno);
      this->mark_failed();
      return;
    }

    ExternalRAMAllocator<uint8_t> speaker_allocator(ExternalRAMAllocator<uint8_t>::ALLOW_FAILURE);
    this->speaker_buffer_ = speaker_allocator.allocate(SPEAKER_BUFFER_SIZE);
    if (this->speaker_buffer_ == nullptr) {
      ESP_LOGW(TAG, "Could not allocate speaker buffer.");
      this->mark_failed();
      return;
    }
  }
#endif

  ExternalRAMAllocator<int16_t> allocator(ExternalRAMAllocator<int16_t>::ALLOW_FAILURE);
  this->input_buffer_ = allocator.allocate(INPUT_BUFFER_SIZE);
  if (this->input_buffer_ == nullptr) {
    ESP_LOGW(TAG, "Could not allocate input buffer.");
    this->mark_failed();
    return;
  }

#ifdef USE_ESP_ADF
  this->vad_instance_ = vad_create(VAD_MODE_4);

  this->ring_buffer_ = rb_create(BUFFER_SIZE, sizeof(int16_t));
  if (this->ring_buffer_ == nullptr) {
    ESP_LOGW(TAG, "Could not allocate ring buffer.");
    this->mark_failed();
    return;
  }
#endif

  ExternalRAMAllocator<uint8_t> send_allocator(ExternalRAMAllocator<uint8_t>::ALLOW_FAILURE);
  this->send_buffer_ = send_allocator.allocate(SEND_BUFFER_SIZE);
  if (send_buffer_ == nullptr) {
    ESP_LOGW(TAG, "Could not allocate send buffer.");
    this->mark_failed();
    return;
  }
}

int VoiceAssistant::read_microphone_() {
  size_t bytes_read = 0;
  if (this->mic_->is_running()) {  // Read audio into input buffer
    bytes_read = this->mic_->read(this->input_buffer_, INPUT_BUFFER_SIZE * sizeof(int16_t));
    if (bytes_read == 0) {
      memset(this->input_buffer_, 0, INPUT_BUFFER_SIZE * sizeof(int16_t));
      return 0;
    }
#ifdef USE_ESP_ADF
    // Write audio into ring buffer
    int available = rb_bytes_available(this->ring_buffer_);
    if (available < bytes_read) {
      rb_read(this->ring_buffer_, nullptr, bytes_read - available, 0);
    }
    rb_write(this->ring_buffer_, (char *) this->input_buffer_, bytes_read, 0);
#endif
  } else {
    ESP_LOGD(TAG, "microphone not running");
  }
  return bytes_read;
}

void VoiceAssistant::loop() {
  if (this->state_ != State::IDLE && this->state_ != State::STOP_MICROPHONE &&
      this->state_ != State::STOPPING_MICROPHONE && !api::global_api_server->is_connected()) {
    if (this->mic_->is_running() || this->state_ == State::STARTING_MICROPHONE) {
      this->set_state_(State::STOP_MICROPHONE, State::IDLE);
    } else {
      this->set_state_(State::IDLE, State::IDLE);
    }
    this->continuous_ = false;
    this->signal_stop_();
    return;
  }
  switch (this->state_) {
    case State::IDLE: {
      if (this->continuous_ && this->desired_state_ == State::IDLE) {
#ifdef USE_ESP_ADF
        if (this->use_wake_word_) {
          rb_reset(this->ring_buffer_);
          this->set_state_(State::START_MICROPHONE, State::WAIT_FOR_VAD);
        } else
#endif
        {
          this->set_state_(State::START_PIPELINE, State::START_MICROPHONE);
        }
      } else {
        this->high_freq_.stop();
      }
      break;
    }
    case State::START_MICROPHONE: {
      ESP_LOGD(TAG, "Starting Microphone");
      memset(this->send_buffer_, 0, SEND_BUFFER_SIZE);
      memset(this->input_buffer_, 0, INPUT_BUFFER_SIZE * sizeof(int16_t));
      this->mic_->start();
      this->high_freq_.start();
      this->set_state_(State::STARTING_MICROPHONE);
      break;
    }
    case State::STARTING_MICROPHONE: {
      if (this->mic_->is_running()) {
        this->set_state_(this->desired_state_);
      }
      break;
    }
#ifdef USE_ESP_ADF
    case State::WAIT_FOR_VAD: {
      this->read_microphone_();
      ESP_LOGD(TAG, "Waiting for speech...");
      this->set_state_(State::WAITING_FOR_VAD);
      break;
    }
    case State::WAITING_FOR_VAD: {
      size_t bytes_read = this->read_microphone_();
      if (bytes_read > 0) {
        vad_state_t vad_state =
            vad_process(this->vad_instance_, this->input_buffer_, SAMPLE_RATE_HZ, VAD_FRAME_LENGTH_MS);
        if (vad_state == VAD_SPEECH) {
          if (this->vad_counter_ < this->vad_threshold_) {
            this->vad_counter_++;
          } else {
            ESP_LOGD(TAG, "VAD detected speech");
            this->set_state_(State::START_PIPELINE, State::STREAMING_MICROPHONE);

            // Reset for next time
            this->vad_counter_ = 0;
          }
        } else {
          if (this->vad_counter_ > 0) {
            this->vad_counter_--;
          }
        }
      }
      break;
    }
#endif
    case State::START_PIPELINE: {
      this->read_microphone_();
      ESP_LOGD(TAG, "Requesting start...");
      uint32_t flags = 0;
      if (this->use_wake_word_)
        flags |= api::enums::VOICE_ASSISTANT_REQUEST_USE_WAKE_WORD;
      if (this->silence_detection_)
        flags |= api::enums::VOICE_ASSISTANT_REQUEST_USE_VAD;
      api::VoiceAssistantAudioSettings audio_settings;
      audio_settings.noise_suppression_level = this->noise_suppression_level_;
      audio_settings.auto_gain = this->auto_gain_;
      audio_settings.volume_multiplier = this->volume_multiplier_;
      if (!api::global_api_server->start_voice_assistant(this->conversation_id_, flags, audio_settings)) {
        ESP_LOGW(TAG, "Could not request start.");
        this->error_trigger_->trigger("not-connected", "Could not request start.");
        this->continuous_ = false;
        this->set_state_(State::IDLE, State::IDLE);
        break;
      }
      this->set_state_(State::STARTING_PIPELINE);
      this->set_timeout("reset-conversation_id", 5 * 60 * 1000, [this]() { this->conversation_id_ = ""; });
      break;
    }
    case State::STARTING_PIPELINE: {
      this->read_microphone_();
      break;  // State changed when udp server port received
    }
    case State::STREAMING_MICROPHONE: {
      size_t bytes_read = this->read_microphone_();
#ifdef USE_ESP_ADF
      if (rb_bytes_filled(this->ring_buffer_) >= SEND_BUFFER_SIZE) {
        rb_read(this->ring_buffer_, (char *) this->send_buffer_, SEND_BUFFER_SIZE, 0);
        this->socket_->sendto(this->send_buffer_, SEND_BUFFER_SIZE, 0, (struct sockaddr *) &this->dest_addr_,
                              sizeof(this->dest_addr_));
      }
#else
      if (bytes_read > 0) {
        this->socket_->sendto(this->input_buffer_, bytes_read, 0, (struct sockaddr *) &this->dest_addr_,
                              sizeof(this->dest_addr_));
      }
#endif
      break;
    }
    case State::STOP_MICROPHONE: {
      if (this->mic_->is_running()) {
        this->mic_->stop();
        this->set_state_(State::STOPPING_MICROPHONE);
      } else {
        this->set_state_(this->desired_state_);
      }
      break;
    }
    case State::STOPPING_MICROPHONE: {
      if (this->mic_->is_stopped()) {
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
        if (this->speaker_buffer_index_ + RECEIVE_SIZE < SPEAKER_BUFFER_SIZE) {
          auto len = this->socket_->read(this->speaker_buffer_ + this->speaker_buffer_index_, RECEIVE_SIZE);
          if (len > 0) {
            this->speaker_buffer_index_ += len;
            this->speaker_buffer_size_ += len;
          }
        } else {
          ESP_LOGW(TAG, "Receive buffer full.");
        }
        if (this->speaker_buffer_size_ > 0) {
          size_t written = this->speaker_->play(this->speaker_buffer_, this->speaker_buffer_size_);
          if (written > 0) {
            memmove(this->speaker_buffer_, this->speaker_buffer_ + written, this->speaker_buffer_size_ - written);
            this->speaker_buffer_size_ -= written;
            this->speaker_buffer_index_ -= written;
            this->set_timeout("speaker-timeout", 1000, [this]() { this->speaker_->stop(); });
          } else {
            ESP_LOGW(TAG, "Speaker buffer full.");
          }
        }
        playing = this->speaker_->is_running();
      }
#endif
#ifdef USE_MEDIA_PLAYER
      if (this->media_player_ != nullptr) {
        playing = (this->media_player_->state == media_player::MediaPlayerState::MEDIA_PLAYER_STATE_PLAYING);
      }
#endif
      if (playing) {
        this->set_timeout("playing", 100, [this]() {
          this->cancel_timeout("speaker-timeout");
          this->set_state_(State::IDLE, State::IDLE);
        });
      }
      break;
    }
    default:
      break;
  }
}

void VoiceAssistant::set_state_(State state) {
  State old_state = this->state_;
  this->state_ = state;
  ESP_LOGD(TAG, "State changed from %d to %d", static_cast<uint8_t>(old_state), static_cast<uint8_t>(state));
}

void VoiceAssistant::set_state_(State state, State desired_state) {
  this->set_state_(state);
  this->desired_state_ = desired_state;
  ESP_LOGD(TAG, "Desired state set to %d", static_cast<uint8_t>(desired_state));
}

void VoiceAssistant::failed_to_start() {
  ESP_LOGE(TAG, "Failed to start server. See Home Assistant logs for more details.");
  this->error_trigger_->trigger("failed-to-start", "Failed to start server. See Home Assistant logs for more details.");
  this->set_state_(State::STOP_MICROPHONE, State::IDLE);
}

void VoiceAssistant::start_streaming(struct sockaddr_storage *addr, uint16_t port) {
  if (this->state_ != State::STARTING_PIPELINE) {
    this->signal_stop_();
    return;
  }

  ESP_LOGD(TAG, "Client started, streaming microphone");

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

  if (this->mic_->is_running()) {
    this->set_state_(State::STREAMING_MICROPHONE, State::STREAMING_MICROPHONE);
  } else {
    this->set_state_(State::START_MICROPHONE, State::STREAMING_MICROPHONE);
  }
}

void VoiceAssistant::request_start(bool continuous, bool silence_detection) {
  if (!api::global_api_server->is_connected()) {
    ESP_LOGE(TAG, "No API client connected");
    this->set_state_(State::IDLE, State::IDLE);
    this->continuous_ = false;
    return;
  }
  if (this->state_ == State::IDLE) {
    this->continuous_ = continuous;
    this->silence_detection_ = silence_detection;
#ifdef USE_ESP_ADF
    if (this->use_wake_word_) {
      rb_reset(this->ring_buffer_);
      this->set_state_(State::START_MICROPHONE, State::WAIT_FOR_VAD);
    } else
#endif
    {
      this->set_state_(State::START_PIPELINE, State::START_MICROPHONE);
    }
  }
}

void VoiceAssistant::request_stop() {
  this->continuous_ = false;

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
    case State::STREAMING_RESPONSE:
      break;  // Let the incoming audio stream finish then it will go to idle.
  }
}

void VoiceAssistant::signal_stop_() {
  ESP_LOGD(TAG, "Signaling stop...");
  api::global_api_server->stop_voice_assistant();
  memset(&this->dest_addr_, 0, sizeof(this->dest_addr_));
}

void VoiceAssistant::on_event(const api::VoiceAssistantEventResponse &msg) {
  ESP_LOGD(TAG, "Event Type: %d", msg.event_type);
  switch (msg.event_type) {
    case api::enums::VOICE_ASSISTANT_RUN_START:
      ESP_LOGD(TAG, "Assist Pipeline running");
      this->start_trigger_->trigger();
      break;
    case api::enums::VOICE_ASSISTANT_WAKE_WORD_START:
      break;
    case api::enums::VOICE_ASSISTANT_WAKE_WORD_END: {
      ESP_LOGD(TAG, "Wake word detected");
      this->wake_word_detected_trigger_->trigger();
      break;
    }
    case api::enums::VOICE_ASSISTANT_STT_START:
      ESP_LOGD(TAG, "STT Started");
      this->listening_trigger_->trigger();
      break;
    case api::enums::VOICE_ASSISTANT_STT_END: {
      this->set_state_(State::STOP_MICROPHONE, State::AWAITING_RESPONSE);
      std::string text;
      for (auto arg : msg.data) {
        if (arg.name == "text") {
          text = std::move(arg.value);
        }
      }
      if (text.empty()) {
        ESP_LOGW(TAG, "No text in STT_END event.");
        return;
      }
      ESP_LOGD(TAG, "Speech recognised as: \"%s\"", text.c_str());
      this->stt_end_trigger_->trigger(text);
      break;
    }
    case api::enums::VOICE_ASSISTANT_INTENT_END: {
      for (auto arg : msg.data) {
        if (arg.name == "conversation_id") {
          this->conversation_id_ = std::move(arg.value);
        }
      }
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
        ESP_LOGW(TAG, "No text in TTS_START event.");
        return;
      }
      ESP_LOGD(TAG, "Response: \"%s\"", text.c_str());
      this->tts_start_trigger_->trigger(text);
#ifdef USE_SPEAKER
      this->speaker_->start();
#endif
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
        ESP_LOGW(TAG, "No url in TTS_END event.");
        return;
      }
      ESP_LOGD(TAG, "Response URL: \"%s\"", url.c_str());
#ifdef USE_MEDIA_PLAYER
      if (this->media_player_ != nullptr) {
        this->media_player_->make_call().set_media_url(url).perform();
      }
#endif
      State new_state = this->local_output_ ? State::STREAMING_RESPONSE : State::IDLE;
      this->set_state_(new_state, new_state);
      this->tts_end_trigger_->trigger(url);
      break;
    }
    case api::enums::VOICE_ASSISTANT_RUN_END: {
      ESP_LOGD(TAG, "Assist Pipeline ended");
      if (this->state_ == State::STREAMING_MICROPHONE) {
#ifdef USE_ESP_ADF
        if (this->use_wake_word_) {
          rb_reset(this->ring_buffer_);
          // No need to stop the microphone since we didn't use the speaker
          this->set_state_(State::WAIT_FOR_VAD, State::WAITING_FOR_VAD);
        } else
#endif
        {
          this->set_state_(State::IDLE, State::IDLE);
        }
      }
      this->end_trigger_->trigger();
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
      if (code == "wake-word-timeout" || code == "wake_word_detection_aborted") {
        // Don't change state here since either the "tts-end" or "run-end" events will do it.
        return;
      }
      ESP_LOGE(TAG, "Error: %s - %s", code.c_str(), message.c_str());
      if (this->state_ != State::IDLE) {
        this->signal_stop_();
        this->set_state_(State::STOP_MICROPHONE, State::IDLE);
      }
      this->error_trigger_->trigger(code, message);
      break;
    }
    default:
      ESP_LOGD(TAG, "Unhandled event type: %d", msg.event_type);
      break;
  }
}

VoiceAssistant *global_voice_assistant = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace voice_assistant
}  // namespace esphome

#endif  // USE_VOICE_ASSISTANT
