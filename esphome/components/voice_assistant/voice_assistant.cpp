#include "voice_assistant.h"

#ifdef USE_VOICE_ASSISTANT

#include "esphome/core/log.h"

#include <cstdio>

namespace esphome {
namespace voice_assistant {

static const char *const TAG = "voice_assistant";

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
    server.ss_family = AF_INET;

    err = socket_->bind((struct sockaddr *) &server, sizeof(server));
    if (err != 0) {
      ESP_LOGW(TAG, "Socket unable to bind: errno %d", errno);
      this->mark_failed();
      return;
    }
  }
#endif

  this->mic_->add_data_callback([this](const std::vector<int16_t> &data) {
    if (!this->running_) {
      return;
    }
    this->socket_->sendto(data.data(), data.size() * sizeof(int16_t), 0, (struct sockaddr *) &this->dest_addr_,
                          sizeof(this->dest_addr_));
  });
}

void VoiceAssistant::loop() {
#ifdef USE_SPEAKER
  if (this->speaker_ != nullptr) {
    uint8_t buf[1024];
    auto len = this->socket_->read(buf, sizeof(buf));
    if (len == -1) {
      return;
    }
    this->speaker_->play(buf, len);
    this->set_timeout("data-incoming", 200, [this]() {
      if (this->continuous_) {
        this->request_start(true);
      }
    });
    return;
  }
#endif
#ifdef USE_MEDIA_PLAYER
  if (this->media_player_ != nullptr) {
    if (!this->playing_tts_ ||
        this->media_player_->state == media_player::MediaPlayerState::MEDIA_PLAYER_STATE_PLAYING) {
      return;
    }
    this->set_timeout("playing-media", 1000, [this]() {
      this->playing_tts_ = false;
      if (this->continuous_) {
        this->request_start(true);
      }
    });
    return;
  }
#endif
  // Set a 1 second timeout to start the voice assistant again.
  this->set_timeout("continuous-no-sound", 1000, [this]() {
    if (this->continuous_) {
      this->request_start(true);
    }
  });
}

void VoiceAssistant::start(struct sockaddr_storage *addr, uint16_t port) {
  ESP_LOGD(TAG, "Starting...");

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
  this->running_ = true;
  this->mic_->start();
  this->listening_trigger_->trigger();
}

void VoiceAssistant::request_start(bool continuous) {
  ESP_LOGD(TAG, "Requesting start...");
  if (!api::global_api_server->start_voice_assistant(this->conversation_id_)) {
    ESP_LOGW(TAG, "Could not request start.");
    this->error_trigger_->trigger("not-connected", "Could not request start.");
    this->continuous_ = false;
    return;
  }
  this->continuous_ = continuous;
  this->set_timeout("reset-conversation_id", 5 * 60 * 1000, [this]() { this->conversation_id_ = ""; });
}

void VoiceAssistant::signal_stop() {
  ESP_LOGD(TAG, "Signaling stop...");
  this->mic_->stop();
  this->running_ = false;
  api::global_api_server->stop_voice_assistant();
  memset(&this->dest_addr_, 0, sizeof(this->dest_addr_));
}

void VoiceAssistant::on_event(const api::VoiceAssistantEventResponse &msg) {
  switch (msg.event_type) {
    case api::enums::VOICE_ASSISTANT_RUN_START:
      ESP_LOGD(TAG, "Assist Pipeline running");
      this->start_trigger_->trigger();
      break;
    case api::enums::VOICE_ASSISTANT_STT_END: {
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
      this->signal_stop();
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
        this->playing_tts_ = true;
        this->media_player_->make_call().set_media_url(url).perform();
      }
#endif
      this->tts_end_trigger_->trigger(url);
      break;
    }
    case api::enums::VOICE_ASSISTANT_RUN_END:
      ESP_LOGD(TAG, "Assist Pipeline ended");
      this->end_trigger_->trigger();
      break;
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
      ESP_LOGE(TAG, "Error: %s - %s", code.c_str(), message.c_str());
      this->continuous_ = false;
      this->signal_stop();
      this->error_trigger_->trigger(code, message);
    }
    default:
      break;
  }
}

VoiceAssistant *global_voice_assistant = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace voice_assistant
}  // namespace esphome

#endif  // USE_VOICE_ASSISTANT
