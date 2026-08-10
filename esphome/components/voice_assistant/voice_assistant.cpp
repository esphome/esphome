#include "voice_assistant.h"
#include "esphome/core/defines.h"

#ifdef USE_VOICE_ASSISTANT

#include "esphome/components/socket/socket.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include "esphome/core/preferences.h"

#include <cinttypes>
#include <cstdio>

#if defined(USE_MICRO_WAKE_WORD) && defined(USE_VOICE_ASSISTANT_RUNTIME_MODEL)
#include "esphome/components/micro_wake_word/model_data.h"
#include "esphome/components/json/json_util.h"
#include "esphome/components/sha256/sha256.h"

#include <algorithm>
#include <memory>
#endif

namespace esphome::voice_assistant {

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

// If one microphone channel keeps producing audio while another configured channel produces none for this
// long, treat the silent channel as failed and stop the stream. A working microphone exposes a chunk every
// SEND_BUFFER_SAMPLES (32 ms), so this is far longer than any legitimate gap between chunks.
static const uint32_t AUDIO_CHANNEL_STALL_TIMEOUT_MS = 2000;

VoiceAssistant::VoiceAssistant() { global_voice_assistant = this; }

void VoiceAssistant::setup() {
  this->mic_source_->add_data_callback([this](const std::vector<uint8_t> &data) {
    std::shared_ptr<ring_buffer::RingBuffer> temp_ring_buffer = this->ring_buffer_.lock();
    if (temp_ring_buffer != nullptr) {
      temp_ring_buffer->write((void *) data.data(), data.size());
    }
  });

  // Second microphone channel
  if (this->mic_source2_ != nullptr) {
    this->mic_source2_->add_data_callback([this](const std::vector<uint8_t> &data) {
      std::shared_ptr<ring_buffer::RingBuffer> temp_ring_buffer = this->ring_buffer2_.lock();
      if (temp_ring_buffer != nullptr) {
        temp_ring_buffer->write((void *) data.data(), data.size());
      }
    });
  }

#ifdef USE_MEDIA_PLAYER
  if (this->media_player_ != nullptr) {
    this->media_player_->add_on_state_callback([this](media_player::MediaPlayerState state) {
      switch (state) {
        case media_player::MediaPlayerState::MEDIA_PLAYER_STATE_ANNOUNCING:
          if (this->media_player_response_state_ == MediaPlayerResponseState::URL_SENT) {
            // State changed to announcing after receiving the url
            this->media_player_response_state_ = MediaPlayerResponseState::PLAYING;
          }
          break;
        default:
          if (this->media_player_response_state_ == MediaPlayerResponseState::PLAYING) {
            // No longer announcing the TTS response
            this->media_player_response_state_ = MediaPlayerResponseState::FINISHED;
          }
          break;
      }
    });
  }
#endif
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
    RAMAllocator<uint8_t> speaker_allocator;
    this->speaker_buffer_ = speaker_allocator.allocate(SPEAKER_BUFFER_SIZE);
    if (this->speaker_buffer_ == nullptr) {
      ESP_LOGW(TAG, "Could not allocate speaker buffer");
      return false;
    }
  }
#endif

  if (this->audio_source_ == nullptr) {
    std::shared_ptr<ring_buffer::RingBuffer> temp_ring_buffer = ring_buffer::RingBuffer::create(RING_BUFFER_SIZE);
    if (temp_ring_buffer == nullptr) {
      ESP_LOGE(TAG, "Could not allocate ring buffer");
      return false;
    }
    // Zero-copy source that reads directly from the ring buffer; frame-aligned to never split an int16 sample.
    this->audio_source_ = audio::RingBufferAudioSource::create(temp_ring_buffer, SEND_BUFFER_SIZE, sizeof(int16_t));
    if (this->audio_source_ == nullptr) {
      ESP_LOGE(TAG, "Could not allocate audio source");
      return false;
    }
    this->ring_buffer_ = temp_ring_buffer;
  }

  // Second microphone channel
  if ((this->mic_source2_ != nullptr) && (this->audio_source2_ == nullptr)) {
    std::shared_ptr<ring_buffer::RingBuffer> temp_ring_buffer = ring_buffer::RingBuffer::create(RING_BUFFER_SIZE);
    if (temp_ring_buffer == nullptr) {
      ESP_LOGE(TAG, "Could not allocate second ring buffer");
      return false;
    }
    this->audio_source2_ = audio::RingBufferAudioSource::create(temp_ring_buffer, SEND_BUFFER_SIZE, sizeof(int16_t));
    if (this->audio_source2_ == nullptr) {
      ESP_LOGE(TAG, "Could not allocate second audio source");
      return false;
    }
    this->ring_buffer2_ = temp_ring_buffer;
  }

  return true;
}

void VoiceAssistant::clear_buffers_() {
  if (this->audio_source_ != nullptr) {
    this->audio_source_->clear_buffered_data();
  }

  // Second microphone channel
  if (this->audio_source2_ != nullptr) {
    this->audio_source2_->clear_buffered_data();
  }

  // Reset the multi-channel stall watchdog (see audio_channel_stall_start_).
  this->audio_channel_stall_start_ = 0;

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
  // Destroying each source releases its ring buffer; the matching weak_ptr then expires automatically.
  this->audio_source_.reset();

  // Second microphone channel
  this->audio_source2_.reset();

#ifdef USE_SPEAKER
  if ((this->speaker_ != nullptr) && (this->speaker_buffer_ != nullptr)) {
    RAMAllocator<uint8_t> speaker_deallocator;
    speaker_deallocator.deallocate(this->speaker_buffer_, SPEAKER_BUFFER_SIZE);
    this->speaker_buffer_ = nullptr;
  }
#endif
}

void VoiceAssistant::reset_conversation_id() {
  this->conversation_id_ = "";
  ESP_LOGD(TAG, "reset conversation ID");
}

void VoiceAssistant::stream_api_audio_() {
  // Both microphone channels are sent together, if configured. Home Assistant feeds one of the
  // channels to its speech-to-text stream and treats an empty payload on that channel as
  // end-of-stream, and the device cannot know which channel it picked, so only send once every
  // configured channel has audio exposed, and always send them together. We don't target any
  // particular message size: Home Assistant re-chunks the audio, and each fill() exposes at most
  // SEND_BUFFER_SIZE bytes.
  while (true) {
    // fill() exposes a new chunk, or returns 0 if a previous chunk is still exposed; available()
    // reports the currently exposed bytes either way.
    this->audio_source_->fill(0, false);
    size_t available = this->audio_source_->available();
    size_t available2 = 0;
    if (this->audio_source2_ != nullptr) {
      this->audio_source2_->fill(0, false);
      available2 = this->audio_source2_->available();
    }

    const bool channel_empty = (available == 0);
    const bool channel2_empty = (this->audio_source2_ != nullptr) && (available2 == 0);
    if (channel_empty || channel2_empty) {
      // A configured channel has no audio yet, so keep any chunk exposed on the other channel for the
      // next pass rather than sending an empty payload.
      this->handle_channel_stall_(available, available2);
      break;
    }

    // Both channels have audio exposed; clear any in-progress stall timer.
    this->audio_channel_stall_start_ = 0;

    api::VoiceAssistantAudio msg;
    // Zero-copy: send_message() copies the data out before we consume it.
    msg.data = this->audio_source_->data();
    msg.data_len = available;
    if (this->audio_source2_ != nullptr) {
      msg.data2 = this->audio_source2_->data();
      msg.data2_len = available2;
    }

    this->api_client_->send_message(msg);

    this->audio_source_->consume(available);
    if (this->audio_source2_ != nullptr) {
      this->audio_source2_->consume(available2);
    }
  }
}

void VoiceAssistant::handle_channel_stall_(size_t available, size_t available2) {
  // Called when at least one configured channel has no audio exposed. When one channel has data and the
  // other does not, watch how long the empty channel stays starved: Home Assistant has no stream timeout
  // and would never tell us to stop, so a channel that fails outright would otherwise hang streaming
  // forever with the live channel's chunk held. Stop the stream with an error after a prolonged imbalance.
  if ((available == 0) && (available2 == 0)) {
    // Both channels are idle (no audio buffered yet); normal, not a stalled channel.
    this->audio_channel_stall_start_ = 0;
    return;
  }

  const uint32_t now = App.get_loop_component_start_time();
  if (this->audio_channel_stall_start_ == 0) {
    this->audio_channel_stall_start_ = now;
  } else if ((now - this->audio_channel_stall_start_) >= AUDIO_CHANNEL_STALL_TIMEOUT_MS) {
    ESP_LOGW(TAG, "Mic channel %d stalled, stopping stream", (available == 0) ? 0 : 1);
    this->audio_channel_stall_start_ = 0;
    this->signal_stop_();
    this->set_state_(State::STOP_MICROPHONE, State::IDLE);
    this->defer([this]() {
      this->error_trigger_.trigger("mic-channel-stalled", "A microphone channel stopped producing audio");
    });
  }
}

void VoiceAssistant::loop() {
  if (this->api_client_ == nullptr && this->state_ != State::IDLE && this->state_ != State::STOP_MICROPHONE &&
      this->state_ != State::STOPPING_MICROPHONE) {
    if (this->mic_source_->is_running() || (this->mic_source2_ && this->mic_source2_->is_running()) ||
        this->state_ == State::STARTING_MICROPHONE) {
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
        this->idle_trigger_.trigger();
        this->set_state_(State::START_MICROPHONE, State::START_PIPELINE);
      } else {
        this->deallocate_buffers_();
      }
      break;
    }
    case State::START_MICROPHONE: {
      ESP_LOGD(TAG, "Starting Microphone");
      if (!this->allocate_buffers_()) {
        this->status_set_error(LOG_STR("Failed to allocate buffers"));
        return;
      }
      if (this->status_has_error()) {
        this->status_clear_error();
      }
      this->clear_buffers_();

      this->mic_source_->start();
      if (this->mic_source2_) {
        this->mic_source2_->start();
      }
      this->set_state_(State::STARTING_MICROPHONE);
      break;
    }
    case State::STARTING_MICROPHONE: {
      if (this->mic_source_->is_running() && (!this->mic_source2_ || this->mic_source2_->is_running())) {
        this->set_state_(this->desired_state_);
      }
      break;
    }
    case State::START_PIPELINE: {
      ESP_LOGD(TAG, "Requesting start");
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
      msg.conversation_id = StringRef(this->conversation_id_);
      msg.flags = flags;
      msg.audio_settings = audio_settings;
      msg.wake_word_phrase = StringRef(this->wake_word_);

      // Reset media player state tracking
#ifdef USE_MEDIA_PLAYER
      if (this->media_player_ != nullptr) {
        this->media_player_response_state_ = MediaPlayerResponseState::IDLE;
      }
#endif

      if (this->api_client_ == nullptr || !this->api_client_->send_message(msg)) {
        ESP_LOGW(TAG, "Could not request start");
        this->error_trigger_.trigger("not-connected", "Could not request start");
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
      // pre_shift is ignored by RingBufferAudioSource (no intermediate transfer buffer to compact).
      if (this->audio_mode_ == AUDIO_MODE_API) {
        this->stream_api_audio_();
      } else {
        // UDP (will eventually be deprecated)
        // Only the primary microphone channel is used
        while (true) {
          this->audio_source_->fill(0, false);
          size_t available = this->audio_source_->available();
          if (available == 0) {
            break;
          }
          if (!this->udp_socket_running_) {
            if (!this->start_udp_socket_()) {
              this->set_state_(State::STOP_MICROPHONE, State::IDLE);
              break;
            }
          }
          this->socket_->sendto(this->audio_source_->data(), available, 0, (struct sockaddr *) &this->dest_addr_,
                                sizeof(this->dest_addr_));
          this->audio_source_->consume(available);
        }
      }  // audio mode
      break;
    }
    case State::STOP_MICROPHONE: {
      // Check both microphone channels
      bool is_running = this->mic_source_->is_running();
      bool is_running2 = false;
      if (this->mic_source2_) {
        is_running2 = this->mic_source2_->is_running();
      }
      if (is_running || is_running2) {
        if (is_running) {
          this->mic_source_->stop();
        }
        if (is_running2) {
          this->mic_source2_->stop();
        }
        this->set_state_(State::STOPPING_MICROPHONE);
      } else {
        this->set_state_(this->desired_state_);
      }
      break;
    }
    case State::STOPPING_MICROPHONE: {
      // Check both microphone channels
      bool is_stopped = this->mic_source_->is_stopped();
      bool is_stopped2 = true;
      if (this->mic_source2_) {
        is_stopped2 = this->mic_source2_->is_stopped();
      }
      if (is_stopped && is_stopped2) {
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
        playing = (this->media_player_response_state_ == MediaPlayerResponseState::PLAYING);

        if (this->media_player_response_state_ == MediaPlayerResponseState::FINISHED) {
          this->media_player_response_state_ = MediaPlayerResponseState::IDLE;
          this->cancel_timeout("playing");
          ESP_LOGD(TAG, "Announcement finished playing");
          this->set_state_(State::RESPONSE_FINISHED, State::RESPONSE_FINISHED);

          api::VoiceAssistantAnnounceFinished msg;
          msg.success = true;
          this->api_client_->send_message(msg);
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

        this->tts_stream_end_trigger_.trigger();
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
    this->client_disconnected_trigger_.trigger();
    return;
  }

  if (this->api_client_ != nullptr) {
    char current_peername[socket::SOCKADDR_STR_LEN];
    char new_peername[socket::SOCKADDR_STR_LEN];
    ESP_LOGE(TAG,
             "Multiple API Clients attempting to connect to Voice Assistant\n"
             "  Current client: %s (%s)\n"
             "  New client: %s (%s)",
             this->api_client_->get_name(), this->api_client_->get_peername_to(current_peername), client->get_name(),
             client->get_peername_to(new_peername));
    return;
  }

  this->api_client_ = client;
  this->client_connected_trigger_.trigger();
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
  this->error_trigger_.trigger("failed-to-start", "Failed to start server. See Home Assistant logs for more details.");
  this->set_state_(State::STOP_MICROPHONE, State::IDLE);
}

void VoiceAssistant::start_streaming() {
  if (this->state_ != State::STARTING_PIPELINE) {
    this->signal_stop_();
    return;
  }

  ESP_LOGD(TAG, "Client started, streaming microphone");
  this->audio_mode_ = AUDIO_MODE_API;

  // Both microphone channels
  if (this->mic_source_->is_running() && (!this->mic_source2_ || this->mic_source2_->is_running())) {
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

  if (this->mic_source2_ != nullptr) {
    ESP_LOGW(TAG, "UDP audio mode does not support a second microphone channel; only the primary will be streamed");
  }

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

  // Only primary microphone channel over UDP
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
      if (this->started_streaming_tts_) {
        // Haven't reached the TTS_END stage, so send the stop signal to HA.
        this->signal_stop_();
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
  ESP_LOGD(TAG, "Signaling stop");
  api::VoiceAssistantRequest msg;
  msg.start = false;
  this->api_client_->send_message(msg);
}

void VoiceAssistant::start_playback_timeout_() {
  this->set_timeout("playing", 2000, [this]() {
    this->cancel_timeout("speaker-timeout");
    this->set_state_(State::RESPONSE_FINISHED, State::RESPONSE_FINISHED);

    if (this->api_client_ == nullptr)
      return;
    api::VoiceAssistantAnnounceFinished msg;
    msg.success = true;
    this->api_client_->send_message(msg);
  });
}

void VoiceAssistant::on_event(const api::VoiceAssistantEventResponse &msg) {
  ESP_LOGD(TAG, "Event Type: %" PRId32, msg.event_type);
  switch (msg.event_type) {
    case api::enums::VOICE_ASSISTANT_RUN_START:
      ESP_LOGD(TAG, "Assist Pipeline running");
#ifdef USE_MEDIA_PLAYER
      this->started_streaming_tts_ = false;
      for (const auto &arg : msg.data) {
        if (arg.name == "url") {
          this->tts_response_url_ = arg.value;
        }
      }
#endif
      this->defer([this]() { this->start_trigger_.trigger(); });
      break;
    case api::enums::VOICE_ASSISTANT_WAKE_WORD_START:
      break;
    case api::enums::VOICE_ASSISTANT_WAKE_WORD_END: {
      ESP_LOGD(TAG, "Wake word detected");
      this->defer([this]() { this->wake_word_detected_trigger_.trigger(); });
      break;
    }
    case api::enums::VOICE_ASSISTANT_STT_START:
      ESP_LOGD(TAG, "STT started");
      this->defer([this]() { this->listening_trigger_.trigger(); });
      break;
    case api::enums::VOICE_ASSISTANT_STT_END: {
      std::string text;
      for (const auto &arg : msg.data) {
        if (arg.name == "text") {
          text = arg.value;
        }
      }
      if (text.empty()) {
        ESP_LOGW(TAG, "No text in STT_END event");
        return;
      } else if (text.length() > 500) {
        text.resize(497);
        text += "...";
      }
      ESP_LOGD(TAG, "Speech recognised as: \"%s\"", text.c_str());
      this->defer([this, text]() { this->stt_end_trigger_.trigger(text); });
      break;
    }
    case api::enums::VOICE_ASSISTANT_INTENT_START:
      ESP_LOGD(TAG, "Intent started");
      this->defer([this]() { this->intent_start_trigger_.trigger(); });
      break;
    case api::enums::VOICE_ASSISTANT_INTENT_PROGRESS: {
      ESP_LOGD(TAG, "Intent progress");
      std::string tts_url_for_trigger;
#ifdef USE_MEDIA_PLAYER
      if (this->media_player_ != nullptr) {
        for (const auto &arg : msg.data) {
          if ((arg.name == "tts_start_streaming") && (arg.value == "1") && !this->tts_response_url_.empty()) {
            this->media_player_response_state_ = MediaPlayerResponseState::URL_SENT;

            this->media_player_->make_call().set_media_url(this->tts_response_url_).set_announcement(true).perform();

            this->started_streaming_tts_ = true;
            this->start_playback_timeout_();

            tts_url_for_trigger = this->tts_response_url_;
            this->tts_response_url_.clear();  // Reset streaming URL
            this->set_state_(State::STREAMING_RESPONSE, State::STREAMING_RESPONSE);
          }
        }
      }
#endif
      this->defer([this, tts_url_for_trigger]() { this->intent_progress_trigger_.trigger(tts_url_for_trigger); });
      break;
    }
    case api::enums::VOICE_ASSISTANT_INTENT_END: {
      for (const auto &arg : msg.data) {
        if (arg.name == "conversation_id") {
          this->conversation_id_ = arg.value;
        } else if (arg.name == "continue_conversation") {
          this->continue_conversation_ = (arg.value == "1");
        }
      }
      this->defer([this]() { this->intent_end_trigger_.trigger(); });
      break;
    }
    case api::enums::VOICE_ASSISTANT_TTS_START: {
      std::string text;
      for (const auto &arg : msg.data) {
        if (arg.name == "text") {
          text = arg.value;
        }
      }
      if (text.empty()) {
        ESP_LOGW(TAG, "No text in TTS_START event");
        return;
      }
      if (text.length() > 500) {
        text.resize(497);
        text += "...";
      }
      ESP_LOGD(TAG, "Response: \"%s\"", text.c_str());
      this->defer([this, text]() {
        this->tts_start_trigger_.trigger(text);
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
      for (const auto &arg : msg.data) {
        if (arg.name == "url") {
          url = arg.value;
        }
      }
      if (url.empty()) {
        ESP_LOGW(TAG, "No url in TTS_END event");
        return;
      }
      ESP_LOGD(TAG, "Response URL: \"%s\"", url.c_str());
      this->defer([this, url]() {
#ifdef USE_MEDIA_PLAYER
        if ((this->media_player_ != nullptr) && (!this->started_streaming_tts_)) {
          this->media_player_response_state_ = MediaPlayerResponseState::URL_SENT;

          this->media_player_->make_call().set_media_url(url).set_announcement(true).perform();

          this->start_playback_timeout_();
        }
        this->started_streaming_tts_ = false;  // Helps indicate reaching the TTS_END stage
#endif
        this->tts_end_trigger_.trigger(url);
      });
      State new_state = this->local_output_ ? State::STREAMING_RESPONSE : State::IDLE;
      if (new_state != this->state_) {
        // Don't needlessly change the state. The intent progress stage may have already changed the state to
        // streaming response.
        this->set_state_(new_state, new_state);
      }
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
      this->defer([this]() { this->end_trigger_.trigger(); });
      break;
    }
    case api::enums::VOICE_ASSISTANT_ERROR: {
      std::string code;
      std::string message;
      for (const auto &arg : msg.data) {
        if (arg.name == "code") {
          code = arg.value;
        } else if (arg.name == "message") {
          message = arg.value;
        }
      }
      if (code == "wake-word-timeout" || code == "wake_word_detection_aborted" || code == "no_wake_word") {
        // Don't change state here since either the "tts-end" or "run-end" events will do it.
        return;
      } else if (code == "wake-provider-missing" || code == "wake-engine-missing") {
        // Wake word is not set up or not ready on Home Assistant so stop and do not retry until user starts again.
        this->defer([this, code, message]() {
          this->request_stop();
          this->error_trigger_.trigger(code, message);
        });
        return;
      }
      ESP_LOGE(TAG, "Error: %s - %s", code.c_str(), message.c_str());
      if (this->state_ != State::IDLE) {
        this->signal_stop_();
        this->set_state_(State::STOP_MICROPHONE, State::IDLE);
      }
      this->defer([this, code, message]() { this->error_trigger_.trigger(code, message); });
      break;
    }
    case api::enums::VOICE_ASSISTANT_TTS_STREAM_START: {
#ifdef USE_SPEAKER
      if (this->speaker_ != nullptr) {
        this->wait_for_stream_end_ = true;
        ESP_LOGD(TAG, "TTS stream start");
        this->defer([this] { this->tts_stream_start_trigger_.trigger(); });
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
      this->defer([this]() { this->stt_vad_start_trigger_.trigger(); });
      break;
    case api::enums::VOICE_ASSISTANT_STT_VAD_END:
      ESP_LOGD(TAG, "STT by VAD end");
      this->set_state_(State::STOP_MICROPHONE, State::AWAITING_RESPONSE);
      this->defer([this]() { this->stt_vad_end_trigger_.trigger(); });
      break;
    default:
      ESP_LOGD(TAG, "Unhandled event type: %" PRId32, msg.event_type);
      break;
  }
}

void VoiceAssistant::on_audio(const api::VoiceAssistantAudio &msg) {
#ifdef USE_SPEAKER  // We should never get to this function if there is no speaker anyway
  if ((this->speaker_ != nullptr) && (this->speaker_buffer_ != nullptr)) {
    if (this->speaker_buffer_index_ + msg.data_len <= SPEAKER_BUFFER_SIZE) {
      memcpy(this->speaker_buffer_ + this->speaker_buffer_index_, msg.data, msg.data_len);
      this->speaker_buffer_index_ += msg.data_len;
      this->speaker_buffer_size_ += msg.data_len;
      this->speaker_bytes_received_ += msg.data_len;
      this->write_speaker_();
      ESP_LOGV(TAG, "Received audio: %u bytes from API", msg.data_len);
    } else {
      ESP_LOGE(TAG, "Cannot receive audio, buffer is full");
    }
  }
#endif
}

void VoiceAssistant::on_timer_event(const api::VoiceAssistantTimerEventResponse &msg) {
  // Find existing timer or add a new one
  auto it = this->timers_.begin();
  for (; it != this->timers_.end(); ++it) {
    if (it->id == msg.timer_id)
      break;
  }
  if (it == this->timers_.end()) {
    this->timers_.push_back({});
    it = this->timers_.end() - 1;
  }
  it->id = msg.timer_id;
  it->name = msg.name;
  it->total_seconds = msg.total_seconds;
  it->seconds_left = msg.seconds_left;
  it->is_active = msg.is_active;

  char timer_buf[Timer::TO_STR_BUFFER_SIZE];
  ESP_LOGD(TAG,
           "Timer Event\n"
           "  Type: %" PRId32 "\n"
           "  %s",
           msg.event_type, it->to_str(timer_buf));

  switch (msg.event_type) {
    case api::enums::VOICE_ASSISTANT_TIMER_STARTED:
      this->timer_started_trigger_.trigger(*it);
      break;
    case api::enums::VOICE_ASSISTANT_TIMER_UPDATED:
      this->timer_updated_trigger_.trigger(*it);
      break;
    case api::enums::VOICE_ASSISTANT_TIMER_CANCELLED:
      this->timer_cancelled_trigger_.trigger(*it);
      this->timers_.erase(it);
      break;
    case api::enums::VOICE_ASSISTANT_TIMER_FINISHED:
      this->timer_finished_trigger_.trigger(*it);
      this->timers_.erase(it);
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
  for (auto &timer : this->timers_) {
    if (timer.is_active && timer.seconds_left > 0) {
      timer.seconds_left--;
    }
  }
  this->timer_tick_trigger_.trigger(this->timers_);
}

void VoiceAssistant::on_announce(const api::VoiceAssistantAnnounceRequest &msg) {
#ifdef USE_MEDIA_PLAYER
  if (this->media_player_ != nullptr) {
    this->tts_start_trigger_.trigger(msg.text);

    this->media_player_response_state_ = MediaPlayerResponseState::URL_SENT;

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

    this->start_playback_timeout_();

    if (this->continuous_) {
      this->set_state_(State::STOP_MICROPHONE, State::STREAMING_RESPONSE);
    } else {
      this->set_state_(State::STREAMING_RESPONSE, State::STREAMING_RESPONSE);
    }

    this->tts_end_trigger_.trigger(msg.media_id);
    this->end_trigger_.trigger();
  }
#endif
}

void VoiceAssistant::on_set_configuration(const std::vector<std::string> &active_wake_words) {
#ifdef USE_MICRO_WAKE_WORD
  if (this->micro_wake_word_) {
    // Disable every wake word first. disable() persists the state for runtime models via the unified pref path.
    for (auto &model : this->micro_wake_word_->get_wake_words()) {
      model->disable();
    }

#ifdef USE_VOICE_ASSISTANT_RUNTIME_MODEL
    // Reset the optimistic pending list, so it tracks the most recent request only.
    this->pending_active_wake_words_.clear();

    // Evict runtime (downloaded) models that are no longer active, freeing their PSRAM buffer immediately
    // rather than leaving it resident behind a merely-disabled model. Compiled-in models are only disabled
    // (above); switching back to an evicted model re-downloads it. get_runtime_model_ids() returns a copy,
    // so removing while iterating is safe.
    for (const auto &id : this->micro_wake_word_->get_runtime_model_ids()) {
      if (std::find(active_wake_words.begin(), active_wake_words.end(), id) == active_wake_words.end()) {
        this->micro_wake_word_->remove_runtime_model(id);
      }
    }
#endif

    // Enable the requested wake words.
    for (const auto &ww_id : active_wake_words) {
      // Already loaded (compiled or previously downloaded) enable() persists the state.
      if (auto *model = this->micro_wake_word_->get_model_by_id(ww_id)) {
        model->enable();
        ESP_LOGD(TAG, "Enabled wake word: %s (id=%s)", model->get_wake_word().c_str(), model->get_id().c_str());
        continue;
      }

#ifdef USE_VOICE_ASSISTANT_RUNTIME_MODEL
      // Not loaded, so it must be an external model we can download.
      CachedExternalWakeWord *cached = this->find_cached_wake_word_(ww_id);
      if (cached == nullptr) {
        ESP_LOGE(TAG, "Unknown wake word ID: %s", ww_id.c_str());
        continue;
      }
      if (!this->is_wake_word_pending_(ww_id)) {
        ESP_LOGD(TAG, "Queuing download for wake word %s", ww_id.c_str());
        this->model_download_queue_.push_back(*cached);     // copy: the task holds its own entries
        this->pending_active_wake_words_.push_back(ww_id);  // report active until the load resolves
      }
#else
      ESP_LOGE(TAG, "Unknown wake word ID: %s (runtime model loading not enabled)", ww_id.c_str());
#endif
    }

#ifdef USE_VOICE_ASSISTANT_RUNTIME_MODEL
    this->try_start_model_load_task_();
#endif
  }
#endif
};

const Configuration &VoiceAssistant::get_configuration(
    const std::vector<api::VoiceAssistantExternalWakeWord> &external_wake_words) {
  this->config_.available_wake_words.clear();
  this->config_.active_wake_words.clear();

#ifdef USE_MICRO_WAKE_WORD
  if (this->micro_wake_word_) {
    this->config_.max_active_wake_words = 1;

#ifdef USE_VOICE_ASSISTANT_RUNTIME_MODEL
    // Rebuild the external wake word cache from this request (drops entries HA no longer advertises).
    this->cache_external_wake_words_(external_wake_words);

    this->remove_stale_runtime_models_();

    this->restore_runtime_models_();
#endif

    // Add built-in wake words (already loaded models)
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

#ifdef USE_VOICE_ASSISTANT_RUNTIME_MODEL
    // Advertise cached external wake words that aren't already loaded (loaded ones are listed above).
    for (const auto &cached_ww : this->external_wake_words_cache_) {
      if (this->micro_wake_word_->get_model_by_id(cached_ww.id) != nullptr) {
        continue;
      }

      WakeWord wake_word;
      wake_word.id = cached_ww.id;
      wake_word.wake_word = cached_ww.wake_word;
      for (const auto &lang : cached_ww.trained_languages) {
        wake_word.trained_languages.push_back(lang);
      }
      this->config_.available_wake_words.push_back(std::move(wake_word));
    }

    // Optimistically include pending (queued but not yet loaded) wake words as active.
    for (const auto &pending_id : this->pending_active_wake_words_) {
      if (std::find(this->config_.active_wake_words.begin(), this->config_.active_wake_words.end(), pending_id) ==
          this->config_.active_wake_words.end()) {
        this->config_.active_wake_words.push_back(pending_id);
      }
    }
#endif
  } else {
#endif
    // No microWakeWord
    this->config_.max_active_wake_words = 0;
#ifdef USE_MICRO_WAKE_WORD
  }
#endif

  return this->config_;
};

#if defined(USE_MICRO_WAKE_WORD) && defined(USE_VOICE_ASSISTANT_RUNTIME_MODEL)
namespace {

// Background download task stack. TLS handshakes require a large stack.
constexpr uint32_t MODEL_LOAD_TASK_STACK_SIZE = 8192;
// Manifests are small JSON documents; reject anything implausibly large before allocating for it.
constexpr size_t MAX_MANIFEST_SIZE = 8192;
// Chunk size for streaming an HTTP body into its destination buffer.
constexpr size_t MODEL_DOWNLOAD_CHUNK_SIZE = 1024;
// Sanity bounds for the model parameters declared in the manifest.
constexpr size_t MAX_SLIDING_WINDOW_SIZE = 50;
constexpr size_t MAX_TENSOR_ARENA_SIZE = 1024 * 1024;

// Verifies a buffer against an expected hex-encoded SHA256. A free function (not a method) so it can never
// read VoiceAssistant state, and so the hasher stays within a single stack frame as the hardware-accelerated
// SHA path requires.
bool verify_model_sha256(const uint8_t *data, size_t size, const std::string &expected_hex) {
  sha256::SHA256 hasher;
  hasher.init();
  hasher.add(data, size);
  hasher.calculate();
  if (hasher.equals_hex(expected_hex.c_str())) {
    return true;
  }
  char actual_hex[65];
  hasher.get_hex(actual_hex);
  ESP_LOGE(TAG, "Model hash mismatch: expected %s, got %s", expected_hex.c_str(), actual_hex);
  return false;
}

}  // namespace

void VoiceAssistant::cache_external_wake_words_(const std::vector<api::VoiceAssistantExternalWakeWord> &wake_words) {
  // Rebuild from scratch so entries HA no longer advertises drop out. In-flight downloads are unaffected: the
  // load task holds its own copies of the entries it is working on.
  this->external_wake_words_cache_.clear();
  for (const auto &ww : wake_words) {
    if (ww.model_type != "micro") {
      continue;  // microWakeWord only
    }
    // Copy every StringRef into an owning string; the proto StringRefs point into the receive buffer and
    // dangle once this handler returns.
    CachedExternalWakeWord entry;
    entry.id = ww.id.str();
    entry.wake_word = ww.wake_word.str();
    entry.trained_languages = ww.trained_languages;
    entry.model_type = ww.model_type.str();
    entry.model_size = ww.model_size;
    entry.model_hash = ww.model_hash.str();
    entry.url = ww.url.str();
    ESP_LOGD(TAG, "Cached external wake word: %s (manifest: %s)", entry.id.c_str(), entry.url.c_str());
    this->external_wake_words_cache_.push_back(std::move(entry));
  }
}

void VoiceAssistant::remove_stale_runtime_models_() {
  // Runtime models whose wake word HA no longer advertises are unloaded entirely, freeing the interpreter,
  // arenas, and the PSRAM model buffer. The enabled preference is deliberately left alone: if HA ever
  // advertises the wake word again, restore_runtime_models_ re-downloads it in the state the user left it.
  for (const auto &id : this->micro_wake_word_->get_runtime_model_ids()) {
    if (this->find_cached_wake_word_(id) == nullptr) {
      this->micro_wake_word_->remove_runtime_model(id);
    }
  }
}

void VoiceAssistant::restore_runtime_models_() {
  for (const auto &cached_ww : this->external_wake_words_cache_) {
    // Skip anything already loaded or already queued/downloading.
    if (this->micro_wake_word_->get_model_by_id(cached_ww.id) != nullptr || this->is_wake_word_pending_(cached_ww.id)) {
      continue;
    }

    // Only re-download models the user had enabled before the reboot. Read the key directly: make_preference
    // allocates a backend that is never freed, and this runs for every advertised model on every request.
    bool enabled = false;
    if (global_preferences->load_from_key(fnv1_hash(cached_ww.id), reinterpret_cast<uint8_t *>(&enabled),
                                          sizeof(enabled)) &&
        enabled) {
      ESP_LOGD(TAG, "Restoring runtime model %s", cached_ww.id.c_str());
      this->model_download_queue_.push_back(cached_ww);
      this->pending_active_wake_words_.push_back(cached_ww.id);
    }
  }

  this->try_start_model_load_task_();
}

CachedExternalWakeWord *VoiceAssistant::find_cached_wake_word_(const std::string &id) {
  for (auto &entry : this->external_wake_words_cache_) {
    if (entry.id == id) {
      return &entry;
    }
  }
  return nullptr;
}

bool VoiceAssistant::is_wake_word_pending_(const std::string &id) const {
  return std::find(this->pending_active_wake_words_.begin(), this->pending_active_wake_words_.end(), id) !=
         this->pending_active_wake_words_.end();
}

void VoiceAssistant::erase_pending_wake_word_(const std::string &id) {
  auto it = std::find(this->pending_active_wake_words_.begin(), this->pending_active_wake_words_.end(), id);
  if (it != this->pending_active_wake_words_.end()) {
    this->pending_active_wake_words_.erase(it);
  }
}

void VoiceAssistant::mark_model_load_failed_(const std::string &id) {
  // Persist disabled so a broken model isn't retried on every boot, and drop the optimistic active entry so
  // HA sees the real (inactive) state.
  auto pref = global_preferences->make_preference<bool>(fnv1_hash(id));
  bool enabled = false;
  pref.save(&enabled);
  this->erase_pending_wake_word_(id);
}

void VoiceAssistant::try_start_model_load_task_() {
  if (this->http_request_ == nullptr || this->micro_wake_word_ == nullptr) {
    return;
  }
  // One task at a time; nothing to do if it is already running or there is no queued work.
  if (this->model_load_task_handle_ != nullptr || this->model_download_queue_.empty()) {
    return;
  }

  // Drop queued entries that are already loaded (a repeated activation raced with an in-flight download).
  auto &queue = this->model_download_queue_;
  queue.erase(std::remove_if(queue.begin(), queue.end(),
                             [this](const CachedExternalWakeWord &ww) {
                               return this->micro_wake_word_->get_model_by_id(ww.id) != nullptr;
                             }),
              queue.end());
  if (queue.empty()) {
    return;
  }

  auto *params = new ModelLoadTaskParams{this, std::move(this->model_download_queue_),
                                         this->micro_wake_word_->get_features_step_size()};
  this->model_download_queue_.clear();  // moved-from vector: make it definitively empty

  BaseType_t result = xTaskCreate(VoiceAssistant::model_load_task, "model_load", MODEL_LOAD_TASK_STACK_SIZE, params, 1,
                                  &this->model_load_task_handle_);

  if (result != pdPASS || this->model_load_task_handle_ == nullptr) {
    ESP_LOGE(TAG, "Failed to create model load task");
    this->model_load_task_handle_ = nullptr;
    // We're on the main loop: do the failure bookkeeping inline for every queued model.
    for (const auto &cached_ww : params->models_to_load) {
      this->mark_model_load_failed_(cached_ww.id);
    }
    delete params;
    return;
  }
}

void VoiceAssistant::model_load_task(void *params) {
  ModelLoadTaskParams *task_params = static_cast<ModelLoadTaskParams *>(params);
  VoiceAssistant *this_va = task_params->voice_assistant;
  const uint8_t features_step_size = task_params->features_step_size;

  ESP_LOGD(TAG, "Model load task started for %zu model(s)", task_params->models_to_load.size());

  for (const auto &cached_ww : task_params->models_to_load) {
    // Copy everything the handoffs need out of cached_ww up front. cached_ww is owned by task_params (freed
    // when this task exits), so no deferred lambda may capture it by reference.
    const std::string id = cached_ww.id;
    ESP_LOGD(TAG, "Processing model: %s", id.c_str());

    // Shared failure step: persist disabled and drop the optimistic active entry, both on the main loop.
    auto fail = [this_va, id]() { this_va->defer([this_va, id]() { this_va->mark_model_load_failed_(id); }); };

    // 1. Download the manifest.
    auto manifest_container = this_va->http_request_->get(cached_ww.url);
    if (!manifest_container || manifest_container->status_code != 200) {
      ESP_LOGW(TAG, "Failed to download manifest for %s from %s", id.c_str(), cached_ww.url.c_str());
      if (manifest_container) {
        manifest_container->end();
      }
      fail();
      continue;
    }
    // A chunked response carries no usable content length: ESP-IDF reports 0 and Arduino reports SIZE_MAX.
    // Read up to the cap in that case and rely on get_bytes_read() below for the size that actually arrived.
    const size_t manifest_length = manifest_container->content_length;
    const bool manifest_length_known = manifest_length != 0 && manifest_length != SIZE_MAX;
    if (manifest_length_known && manifest_length > MAX_MANIFEST_SIZE) {
      ESP_LOGW(TAG, "Manifest for %s is larger than %zu bytes", id.c_str(), MAX_MANIFEST_SIZE);
      manifest_container->end();
      fail();
      continue;
    }
    const size_t manifest_size = manifest_length_known ? manifest_length : MAX_MANIFEST_SIZE;
    std::string manifest_str;
    manifest_str.resize(manifest_size);
    auto manifest_read =
        http_request::http_read_fully(manifest_container.get(), reinterpret_cast<uint8_t *>(manifest_str.data()),
                                      manifest_size, MODEL_DOWNLOAD_CHUNK_SIZE, this_va->http_request_->get_timeout());
    size_t manifest_bytes = manifest_container->get_bytes_read();
    manifest_container->end();
    if (manifest_read.status != http_request::HttpReadStatus::OK || manifest_bytes == 0) {
      ESP_LOGW(TAG, "Failed to read manifest for %s", id.c_str());
      fail();
      continue;
    }
    manifest_str.resize(manifest_bytes);  // trim to what actually arrived

    // 2. Parse the manifest.
    std::string model_url;
    std::string wake_word;
    float probability_cutoff = 0.0f;
    uint32_t sliding_window_size = 0;
    uint32_t tensor_arena_size = 0;
    int manifest_feature_step_size = -1;
    bool parse_success = json::parse_json(manifest_str, [&](JsonObject root) -> bool {
      if (!root["model"].is<const char *>() || !root["wake_word"].is<const char *>() ||
          !root["micro"].is<JsonObject>()) {
        ESP_LOGE(TAG, "Manifest does not contain required fields");
        return false;
      }
      model_url = root["model"].as<std::string>();
      wake_word = root["wake_word"].as<std::string>();

      JsonObject micro = root["micro"];
      if (!micro["probability_cutoff"].is<float>() || !micro["sliding_window_size"].is<uint32_t>() ||
          !micro["tensor_arena_size"].is<uint32_t>() || !micro["feature_step_size"].is<int>()) {
        ESP_LOGE(TAG, "Manifest micro section does not contain required fields");
        return false;
      }
      probability_cutoff = micro["probability_cutoff"];
      sliding_window_size = micro["sliding_window_size"];
      tensor_arena_size = micro["tensor_arena_size"];
      manifest_feature_step_size = micro["feature_step_size"];
      return true;
    });
    if (!parse_success) {
      ESP_LOGW(TAG, "Failed to parse manifest JSON for %s", id.c_str());
      fail();
      continue;
    }

    // A model trained with a different feature step size would silently produce garbage inferences.
    if (manifest_feature_step_size != static_cast<int>(features_step_size)) {
      ESP_LOGE(TAG, "Model %s feature step size %d does not match device's %u; rejecting", id.c_str(),
               manifest_feature_step_size, features_step_size);
      fail();
      continue;
    }

    // Validate model hyper-parameters for sanity: a probability cutoff outside [0, 1] overflows the
    // uint8_t quantization, a zero sliding window divides by zero when averaging, and an
    // out-of-range arena could fail to allocate memory.
    if (probability_cutoff < 0.0f || probability_cutoff > 1.0f || sliding_window_size == 0 ||
        sliding_window_size > MAX_SLIDING_WINDOW_SIZE || tensor_arena_size == 0 ||
        tensor_arena_size > MAX_TENSOR_ARENA_SIZE) {
      ESP_LOGE(TAG, "Model %s has out-of-range parameters (cutoff %.3f, window %" PRIu32 ", arena %" PRIu32 ")",
               id.c_str(), probability_cutoff, sliding_window_size, tensor_arena_size);
      fail();
      continue;
    }

    // 3. Resolve a relative "model" filename against the manifest URL; absolute URLs are used as-is.
    if (!model_url.starts_with("http://") && !model_url.starts_with("https://")) {
      size_t slash_pos = cached_ww.url.find_last_of('/');
      if (slash_pos != std::string::npos) {
        // Prepend in place: building the prefix separately would allocate two temporary strings.
        model_url.insert(0, cached_ww.url, 0, slash_pos + 1);
      }
    }
    ESP_LOGD(TAG, "Resolved model URL for %s: %s", id.c_str(), model_url.c_str());

    // 4. Download the model.
    auto container = this_va->http_request_->get(model_url);
    if (!container || container->status_code != 200) {
      ESP_LOGW(TAG, "Failed to connect to model URL for %s", id.c_str());
      if (container) {
        container->end();
      }
      fail();
      continue;
    }
    // Bound the read by the size Home Assistant advertised. A chunked response carries no usable content
    // length (0 on ESP-IDF, SIZE_MAX on Arduino), so it cannot size the buffer on its own.
    const size_t content_length = container->content_length;
    const bool content_length_known = content_length != 0 && content_length != SIZE_MAX;
    size_t model_size = cached_ww.model_size;
    if (model_size == 0) {
      // Home Assistant advertised no size, so the content length is all we have to go on.
      model_size = content_length_known ? content_length : 0;
    } else if (content_length_known && content_length != model_size) {
      ESP_LOGW(TAG, "Model %s content length %zu disagrees with the advertised %" PRIu32 " (SHA256 is authoritative)",
               id.c_str(), content_length, cached_ww.model_size);
    }
    if (model_size == 0) {
      ESP_LOGW(TAG, "Model %s has no known size", id.c_str());
      container->end();
      fail();
      continue;
    }
    auto model_data = std::make_shared<micro_wake_word::ModelData>();
    if (!model_data->allocate(model_size)) {
      ESP_LOGW(TAG, "Failed to allocate %zu bytes for model %s", model_size, id.c_str());
      container->end();
      fail();
      continue;
    }
    auto model_read = http_request::http_read_fully(container.get(), model_data->get_write_pointer(), model_size,
                                                    MODEL_DOWNLOAD_CHUNK_SIZE, this_va->http_request_->get_timeout());
    size_t model_bytes = container->get_bytes_read();
    container->end();
    if (model_read.status != http_request::HttpReadStatus::OK || model_bytes != model_size) {
      ESP_LOGW(TAG, "Failed to read model %s (%zu of %zu bytes)", id.c_str(), model_bytes, model_size);
      fail();
      continue;
    }

    // 5. Verify the SHA256 (static helper: never touches VA state, keeps the hasher in one stack frame).
    if (!verify_model_sha256(model_data->get_write_pointer(), model_size, cached_ww.model_hash)) {
      ESP_LOGW(TAG, "SHA256 validation failed for model %s", id.c_str());
      fail();
      continue;
    }

    // 6. Validate the TFLite header.
    if (!model_data->validate_and_mark_ready()) {
      ESP_LOGW(TAG, "TFLite validation failed for model %s", id.c_str());
      fail();
      continue;
    }

    // 7. Hand off to the main loop. Every capture is by value (strings, POD, the shared_ptr).
    ESP_LOGI(TAG, "Loaded model %s (%zu bytes); handing off to micro_wake_word", id.c_str(), model_size);
    const std::string wake_word_copy = wake_word;
    const std::vector<std::string> trained_languages = cached_ww.trained_languages;
    const uint8_t quantized_cutoff = static_cast<uint8_t>(probability_cutoff * 255);
    const size_t window = sliding_window_size;
    const size_t arena = tensor_arena_size;
    this_va->defer([this_va, id, wake_word_copy, trained_languages, model_data, quantized_cutoff, window, arena]() {
      // The world may have changed while the download was in flight; re-check against current main-loop state.
      if (this_va->find_cached_wake_word_(id) == nullptr) {
        // HA stopped advertising this wake word: discard the download. The model buffer is freed when the
        // last shared_ptr reference (this lambda's capture) drops.
        ESP_LOGW(TAG, "Discarding downloaded model %s: no longer advertised", id.c_str());
        this_va->erase_pending_wake_word_(id);
        return;
      }
      if (this_va->micro_wake_word_->get_model_by_id(id) != nullptr) {
        // A duplicate download slipped through (an earlier pass already added the model, e.g. a config
        // change re-queued it while it was in flight). Benign: leave the existing model enabled. Check
        // before building the model, because a WakeWordModel claims a preference backend that is never
        // freed, so a model built only to be rejected costs internal RAM permanently.
        ESP_LOGD(TAG, "Discarding downloaded model %s: already loaded", id.c_str());
        this_va->erase_pending_wake_word_(id);
        return;
      }

      // A set_configuration while the download was in flight may have withdrawn the activation request.
      const bool still_wanted = this_va->is_wake_word_pending_(id);

      auto model = make_unique<micro_wake_word::WakeWordModel>(id, model_data, quantized_cutoff, window, wake_word_copy,
                                                               trained_languages, arena);
      auto *raw = model.get();
      if (!this_va->micro_wake_word_->add_runtime_model(std::move(model))) {
        // The id was free a moment ago and this is the main loop, so a duplicate is no longer possible:
        // this is a genuine failure (e.g. the pause handshake timed out). Don't retry it on every boot.
        this_va->mark_model_load_failed_(id);
        return;
      }
      if (still_wanted) {
        raw->enable();  // persists pref = true via the unified path
        ESP_LOGI(TAG, "Enabled runtime model %s", id.c_str());
      } else {
        // Deactivated while downloading: keep the model loaded for instant re-enable, but leave it off.
        raw->disable();  // persists pref = false
        ESP_LOGI(TAG, "Loaded runtime model %s (left disabled: activation was withdrawn)", id.c_str());
      }
      this_va->erase_pending_wake_word_(id);
    });
  }

  ESP_LOGD(TAG, "Model load task completed");

  // Drain anything queued while the task was busy, then let the next request start a fresh task. Deleting
  // task_params before these deferred lambdas run is safe because every capture above is by value.
  this_va->defer([this_va]() {
    this_va->model_load_task_handle_ = nullptr;
    this_va->try_start_model_load_task_();
  });

  delete task_params;
  vTaskDelete(nullptr);
}
#endif  // USE_MICRO_WAKE_WORD && USE_VOICE_ASSISTANT_RUNTIME_MODEL

VoiceAssistant *global_voice_assistant = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esphome::voice_assistant

#endif  // USE_VOICE_ASSISTANT
