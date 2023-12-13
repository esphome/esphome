#pragma once

#include "esphome/core/defines.h"

#ifdef USE_VOICE_ASSISTANT

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#include "esphome/components/api/api_connection.h"
#include "esphome/components/api/api_pb2.h"
#include "esphome/components/microphone/microphone.h"
#ifdef USE_SPEAKER
#include "esphome/components/speaker/speaker.h"
#endif
#ifdef USE_MEDIA_PLAYER
#include "esphome/components/media_player/media_player.h"
#endif
#include "esphome/components/socket/socket.h"

#ifdef USE_ESP_ADF
#include <esp_vad.h>
#include <ringbuf.h>
#endif

namespace esphome {
namespace voice_assistant {

// Version 1: Initial version
// Version 2: Adds raw speaker support
// Version 3: Unused/skip
static const uint32_t INITIAL_VERSION = 1;
static const uint32_t SPEAKER_SUPPORT = 2;

enum class State {
  IDLE,
  START_MICROPHONE,
  STARTING_MICROPHONE,
  WAIT_FOR_VAD,
  WAITING_FOR_VAD,
  START_PIPELINE,
  STARTING_PIPELINE,
  STREAMING_MICROPHONE,
  STOP_MICROPHONE,
  STOPPING_MICROPHONE,
  AWAITING_RESPONSE,
  STREAMING_RESPONSE,
  RESPONSE_FINISHED,
};

class VoiceAssistant : public Component {
 public:
  void setup() override;
  void loop() override;
  float get_setup_priority() const override;
  void start_streaming(struct sockaddr_storage *addr, uint16_t port);
  void failed_to_start();

  void set_microphone(microphone::Microphone *mic) { this->mic_ = mic; }
#ifdef USE_SPEAKER
  void set_speaker(speaker::Speaker *speaker) {
    this->speaker_ = speaker;
    this->local_output_ = true;
  }
#endif
#ifdef USE_MEDIA_PLAYER
  void set_media_player(media_player::MediaPlayer *media_player) {
    this->media_player_ = media_player;
    this->local_output_ = true;
  }
#endif

  uint32_t get_version() const {
#ifdef USE_SPEAKER
    if (this->speaker_ != nullptr) {
      return SPEAKER_SUPPORT;
    }
#endif
    return INITIAL_VERSION;
  }

  void request_start(bool continuous, bool silence_detection);
  void request_stop();

  void on_event(const api::VoiceAssistantEventResponse &msg);

  bool is_running() const { return this->state_ != State::IDLE; }
  void set_continuous(bool continuous) { this->continuous_ = continuous; }
  bool is_continuous() const { return this->continuous_; }

  void set_use_wake_word(bool use_wake_word) { this->use_wake_word_ = use_wake_word; }
#ifdef USE_ESP_ADF
  void set_vad_threshold(uint8_t vad_threshold) { this->vad_threshold_ = vad_threshold; }
#endif

  void set_noise_suppression_level(uint8_t noise_suppression_level) {
    this->noise_suppression_level_ = noise_suppression_level;
  }
  void set_auto_gain(uint8_t auto_gain) { this->auto_gain_ = auto_gain; }
  void set_volume_multiplier(float volume_multiplier) { this->volume_multiplier_ = volume_multiplier; }

  Trigger<> *get_intent_end_trigger() const { return this->intent_end_trigger_; }
  Trigger<> *get_intent_start_trigger() const { return this->intent_start_trigger_; }
  Trigger<> *get_listening_trigger() const { return this->listening_trigger_; }
  Trigger<> *get_end_trigger() const { return this->end_trigger_; }
  Trigger<> *get_start_trigger() const { return this->start_trigger_; }
  Trigger<> *get_stt_vad_end_trigger() const { return this->stt_vad_end_trigger_; }
  Trigger<> *get_stt_vad_start_trigger() const { return this->stt_vad_start_trigger_; }
#ifdef USE_SPEAKER
  Trigger<> *get_tts_stream_start_trigger() const { return this->tts_stream_start_trigger_; }
  Trigger<> *get_tts_stream_end_trigger() const { return this->tts_stream_end_trigger_; }
#endif
  Trigger<> *get_wake_word_detected_trigger() const { return this->wake_word_detected_trigger_; }
  Trigger<std::string> *get_stt_end_trigger() const { return this->stt_end_trigger_; }
  Trigger<std::string> *get_tts_end_trigger() const { return this->tts_end_trigger_; }
  Trigger<std::string> *get_tts_start_trigger() const { return this->tts_start_trigger_; }
  Trigger<std::string, std::string> *get_error_trigger() const { return this->error_trigger_; }

  Trigger<> *get_client_connected_trigger() const { return this->client_connected_trigger_; }
  Trigger<> *get_client_disconnected_trigger() const { return this->client_disconnected_trigger_; }

  void client_subscription(api::APIConnection *client, bool subscribe);
  api::APIConnection *get_api_connection() const { return this->api_client_; }

 protected:
  int read_microphone_();
  void set_state_(State state);
  void set_state_(State state, State desired_state);
  void signal_stop_();

  std::unique_ptr<socket::Socket> socket_ = nullptr;
  struct sockaddr_storage dest_addr_;

  Trigger<> *intent_end_trigger_ = new Trigger<>();
  Trigger<> *intent_start_trigger_ = new Trigger<>();
  Trigger<> *listening_trigger_ = new Trigger<>();
  Trigger<> *end_trigger_ = new Trigger<>();
  Trigger<> *start_trigger_ = new Trigger<>();
  Trigger<> *stt_vad_start_trigger_ = new Trigger<>();
  Trigger<> *stt_vad_end_trigger_ = new Trigger<>();
#ifdef USE_SPEAKER
  Trigger<> *tts_stream_start_trigger_ = new Trigger<>();
  Trigger<> *tts_stream_end_trigger_ = new Trigger<>();
#endif
  Trigger<> *wake_word_detected_trigger_ = new Trigger<>();
  Trigger<std::string> *stt_end_trigger_ = new Trigger<std::string>();
  Trigger<std::string> *tts_end_trigger_ = new Trigger<std::string>();
  Trigger<std::string> *tts_start_trigger_ = new Trigger<std::string>();
  Trigger<std::string, std::string> *error_trigger_ = new Trigger<std::string, std::string>();

  Trigger<> *client_connected_trigger_ = new Trigger<>();
  Trigger<> *client_disconnected_trigger_ = new Trigger<>();

  api::APIConnection *api_client_{nullptr};

  microphone::Microphone *mic_{nullptr};
#ifdef USE_SPEAKER
  void write_speaker_();
  speaker::Speaker *speaker_{nullptr};
  uint8_t *speaker_buffer_;
  size_t speaker_buffer_index_{0};
  size_t speaker_buffer_size_{0};
  size_t speaker_bytes_received_{0};
  bool wait_for_stream_end_{false};
  bool stream_ended_{false};
#endif
#ifdef USE_MEDIA_PLAYER
  media_player::MediaPlayer *media_player_{nullptr};
#endif

  bool local_output_{false};

  std::string conversation_id_{""};

  HighFrequencyLoopRequester high_freq_;

#ifdef USE_ESP_ADF
  vad_handle_t vad_instance_;
  ringbuf_handle_t ring_buffer_;
  uint8_t vad_threshold_{5};
  uint8_t vad_counter_{0};
#endif

  bool use_wake_word_;
  uint8_t noise_suppression_level_;
  uint8_t auto_gain_;
  float volume_multiplier_;

  uint8_t *send_buffer_;
  int16_t *input_buffer_;

  bool continuous_{false};
  bool silence_detection_;

  State state_{State::IDLE};
  State desired_state_{State::IDLE};
};

template<typename... Ts> class StartAction : public Action<Ts...>, public Parented<VoiceAssistant> {
 public:
  void play(Ts... x) override { this->parent_->request_start(false, this->silence_detection_); }

  void set_silence_detection(bool silence_detection) { this->silence_detection_ = silence_detection; }

 protected:
  bool silence_detection_;
};

template<typename... Ts> class StartContinuousAction : public Action<Ts...>, public Parented<VoiceAssistant> {
 public:
  void play(Ts... x) override { this->parent_->request_start(true, true); }
};

template<typename... Ts> class StopAction : public Action<Ts...>, public Parented<VoiceAssistant> {
 public:
  void play(Ts... x) override { this->parent_->request_stop(); }
};

template<typename... Ts> class IsRunningCondition : public Condition<Ts...>, public Parented<VoiceAssistant> {
 public:
  bool check(Ts... x) override { return this->parent_->is_running() || this->parent_->is_continuous(); }
};

template<typename... Ts> class ConnectedCondition : public Condition<Ts...>, public Parented<VoiceAssistant> {
 public:
  bool check(Ts... x) override { return this->parent_->get_api_connection() != nullptr; }
};

extern VoiceAssistant *global_voice_assistant;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace voice_assistant
}  // namespace esphome

#endif  // USE_VOICE_ASSISTANT
