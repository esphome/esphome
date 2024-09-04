#pragma once

#include "esphome/core/defines.h"

#ifdef USE_VOICE_ASSISTANT

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/ring_buffer.h"

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
#endif

#include <unordered_map>
#include <vector>

namespace esphome {
namespace voice_assistant {

// Version 1: Initial version
// Version 2: Adds raw speaker support
static const uint32_t LEGACY_INITIAL_VERSION = 1;
static const uint32_t LEGACY_SPEAKER_SUPPORT = 2;

enum VoiceAssistantFeature : uint32_t {
  FEATURE_VOICE_ASSISTANT = 1 << 0,
  FEATURE_SPEAKER = 1 << 1,
  FEATURE_API_AUDIO = 1 << 2,
  FEATURE_TIMERS = 1 << 3,
};

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

enum AudioMode : uint8_t {
  AUDIO_MODE_UDP,
  AUDIO_MODE_API,
};

struct Timer {
  std::string id;
  std::string name;
  uint32_t total_seconds;
  uint32_t seconds_left;
  bool is_active;

  std::string to_string() const {
    return str_sprintf("Timer(id=%s, name=%s, total_seconds=%" PRIu32 ", seconds_left=%" PRIu32 ", is_active=%s)",
                       this->id.c_str(), this->name.c_str(), this->total_seconds, this->seconds_left,
                       YESNO(this->is_active));
  }
};

class VoiceAssistant : public Component {
 public:
  void setup() override;
  void loop() override;
  float get_setup_priority() const override;
  void start_streaming();
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

  uint32_t get_legacy_version() const {
#ifdef USE_SPEAKER
    if (this->speaker_ != nullptr) {
      return LEGACY_SPEAKER_SUPPORT;
    }
#endif
    return LEGACY_INITIAL_VERSION;
  }

  uint32_t get_feature_flags() const {
    uint32_t flags = 0;
    flags |= VoiceAssistantFeature::FEATURE_VOICE_ASSISTANT;
    flags |= VoiceAssistantFeature::FEATURE_API_AUDIO;
#ifdef USE_SPEAKER
    if (this->speaker_ != nullptr) {
      flags |= VoiceAssistantFeature::FEATURE_SPEAKER;
    }
#endif

    if (this->has_timers_) {
      flags |= VoiceAssistantFeature::FEATURE_TIMERS;
    }

    return flags;
  }

  void request_start(bool continuous, bool silence_detection);
  void request_stop();

  void on_event(const api::VoiceAssistantEventResponse &msg);
  void on_audio(const api::VoiceAssistantAudio &msg);
  void on_timer_event(const api::VoiceAssistantTimerEventResponse &msg);

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
  void set_conversation_timeout(uint32_t conversation_timeout) { this->conversation_timeout_ = conversation_timeout; }
  void reset_conversation_id();

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
  Trigger<> *get_idle_trigger() const { return this->idle_trigger_; }

  Trigger<> *get_client_connected_trigger() const { return this->client_connected_trigger_; }
  Trigger<> *get_client_disconnected_trigger() const { return this->client_disconnected_trigger_; }

  void client_subscription(api::APIConnection *client, bool subscribe);
  api::APIConnection *get_api_connection() const { return this->api_client_; }

  void set_wake_word(const std::string &wake_word) { this->wake_word_ = wake_word; }

  Trigger<Timer> *get_timer_started_trigger() const { return this->timer_started_trigger_; }
  Trigger<Timer> *get_timer_updated_trigger() const { return this->timer_updated_trigger_; }
  Trigger<Timer> *get_timer_cancelled_trigger() const { return this->timer_cancelled_trigger_; }
  Trigger<Timer> *get_timer_finished_trigger() const { return this->timer_finished_trigger_; }
  Trigger<std::vector<Timer>> *get_timer_tick_trigger() const { return this->timer_tick_trigger_; }
  void set_has_timers(bool has_timers) { this->has_timers_ = has_timers; }
  const std::unordered_map<std::string, Timer> &get_timers() const { return this->timers_; }

 protected:
  bool allocate_buffers_();
  void clear_buffers_();
  void deallocate_buffers_();

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
  Trigger<> *idle_trigger_ = new Trigger<>();

  Trigger<> *client_connected_trigger_ = new Trigger<>();
  Trigger<> *client_disconnected_trigger_ = new Trigger<>();

  api::APIConnection *api_client_{nullptr};

  std::unordered_map<std::string, Timer> timers_;
  void timer_tick_();
  Trigger<Timer> *timer_started_trigger_ = new Trigger<Timer>();
  Trigger<Timer> *timer_finished_trigger_ = new Trigger<Timer>();
  Trigger<Timer> *timer_updated_trigger_ = new Trigger<Timer>();
  Trigger<Timer> *timer_cancelled_trigger_ = new Trigger<Timer>();
  Trigger<std::vector<Timer>> *timer_tick_trigger_ = new Trigger<std::vector<Timer>>();
  bool has_timers_{false};
  bool timer_tick_running_{false};

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

  std::string wake_word_{""};

  HighFrequencyLoopRequester high_freq_;

#ifdef USE_ESP_ADF
  vad_handle_t vad_instance_;
  uint8_t vad_threshold_{5};
  uint8_t vad_counter_{0};
#endif
  std::unique_ptr<RingBuffer> ring_buffer_;

  bool use_wake_word_;
  uint8_t noise_suppression_level_;
  uint8_t auto_gain_;
  float volume_multiplier_;
  uint32_t conversation_timeout_;

  uint8_t *send_buffer_;
  int16_t *input_buffer_;

  bool continuous_{false};
  bool silence_detection_;

  State state_{State::IDLE};
  State desired_state_{State::IDLE};

  AudioMode audio_mode_{AUDIO_MODE_UDP};
  bool udp_socket_running_{false};
  bool start_udp_socket_();
};

template<typename... Ts> class StartAction : public Action<Ts...>, public Parented<VoiceAssistant> {
  TEMPLATABLE_VALUE(std::string, wake_word);

 public:
  void play(Ts... x) override {
    this->parent_->set_wake_word(this->wake_word_.value(x...));
    this->parent_->request_start(false, this->silence_detection_);
  }

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
