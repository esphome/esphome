#pragma once

#include "esphome/core/defines.h"

#ifdef USE_VOICE_ASSISTANT

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"

#include "esphome/components/api/api_pb2.h"
#include "esphome/components/api/api_server.h"
#include "esphome/components/microphone/microphone.h"
#ifdef USE_SPEAKER
#include "esphome/components/speaker/speaker.h"
#endif
#ifdef USE_MEDIA_PLAYER
#include "esphome/components/media_player/media_player.h"
#endif
#include "esphome/components/socket/socket.h"

namespace esphome {
namespace voice_assistant {

// Version 1: Initial version
// Version 2: Adds raw speaker support
// Version 3: Adds continuous support
static const uint32_t INITIAL_VERSION = 1;
static const uint32_t SPEAKER_SUPPORT = 2;
static const uint32_t SILENCE_DETECTION_SUPPORT = 3;

class VoiceAssistant : public Component {
 public:
  void setup() override;
  void loop() override;
  float get_setup_priority() const override;
  void start(struct sockaddr_storage *addr, uint16_t port);

  void set_microphone(microphone::Microphone *mic) { this->mic_ = mic; }
#ifdef USE_SPEAKER
  void set_speaker(speaker::Speaker *speaker) { this->speaker_ = speaker; }
#endif
#ifdef USE_MEDIA_PLAYER
  void set_media_player(media_player::MediaPlayer *media_player) { this->media_player_ = media_player; }
#endif

  uint32_t get_version() const {
#ifdef USE_SPEAKER
    if (this->speaker_ != nullptr) {
      if (this->silence_detection_) {
        return SILENCE_DETECTION_SUPPORT;
      }
      return SPEAKER_SUPPORT;
    }
#endif
    return INITIAL_VERSION;
  }

  void request_start(bool continuous = false);
  void signal_stop();

  void on_event(const api::VoiceAssistantEventResponse &msg);

  bool is_running() const { return this->running_; }
  void set_continuous(bool continuous) { this->continuous_ = continuous; }
  bool is_continuous() const { return this->continuous_; }

  void set_silence_detection(bool silence_detection) { this->silence_detection_ = silence_detection; }

  Trigger<> *get_listening_trigger() const { return this->listening_trigger_; }
  Trigger<> *get_start_trigger() const { return this->start_trigger_; }
  Trigger<std::string> *get_stt_end_trigger() const { return this->stt_end_trigger_; }
  Trigger<std::string> *get_tts_start_trigger() const { return this->tts_start_trigger_; }
  Trigger<std::string> *get_tts_end_trigger() const { return this->tts_end_trigger_; }
  Trigger<> *get_end_trigger() const { return this->end_trigger_; }
  Trigger<std::string, std::string> *get_error_trigger() const { return this->error_trigger_; }

 protected:
  std::unique_ptr<socket::Socket> socket_ = nullptr;
  struct sockaddr_storage dest_addr_;

  Trigger<> *listening_trigger_ = new Trigger<>();
  Trigger<> *start_trigger_ = new Trigger<>();
  Trigger<std::string> *stt_end_trigger_ = new Trigger<std::string>();
  Trigger<std::string> *tts_start_trigger_ = new Trigger<std::string>();
  Trigger<std::string> *tts_end_trigger_ = new Trigger<std::string>();
  Trigger<> *end_trigger_ = new Trigger<>();
  Trigger<std::string, std::string> *error_trigger_ = new Trigger<std::string, std::string>();

  microphone::Microphone *mic_{nullptr};
#ifdef USE_SPEAKER
  speaker::Speaker *speaker_{nullptr};
#endif
#ifdef USE_MEDIA_PLAYER
  media_player::MediaPlayer *media_player_{nullptr};
  bool playing_tts_{false};
#endif

  std::string conversation_id_{""};

  bool running_{false};
  bool continuous_{false};
  bool silence_detection_;
};

template<typename... Ts> class StartAction : public Action<Ts...>, public Parented<VoiceAssistant> {
 public:
  void play(Ts... x) override { this->parent_->request_start(); }
};

template<typename... Ts> class StartContinuousAction : public Action<Ts...>, public Parented<VoiceAssistant> {
 public:
  void play(Ts... x) override { this->parent_->request_start(true); }
};

template<typename... Ts> class StopAction : public Action<Ts...>, public Parented<VoiceAssistant> {
 public:
  void play(Ts... x) override {
    this->parent_->set_continuous(false);
    this->parent_->signal_stop();
  }
};

template<typename... Ts> class IsRunningCondition : public Condition<Ts...>, public Parented<VoiceAssistant> {
 public:
  bool check(Ts... x) override { return this->parent_->is_running() || this->parent_->is_continuous(); }
};

extern VoiceAssistant *global_voice_assistant;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace voice_assistant
}  // namespace esphome

#endif  // USE_VOICE_ASSISTANT
