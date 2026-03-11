#pragma once

#include "esphome/core/defines.h"

#ifdef USE_ESP32

#include "esphome/components/audio/audio.h"
#include "esphome/components/media_source/media_source.h"
#include "esphome/components/media_player/media_player.h"
#include "esphome/components/speaker/speaker.h"

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/preferences.h"

#include <array>
#include <atomic>
#include <memory>
#include <vector>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

namespace esphome::speaker_source {

// THREADING MODEL:
// This component coordinates media sources that run their own decode tasks with speakers
// that have their own playback callback tasks. Three thread contexts exist:
//
// - Main loop task: setup(), loop(), dump_config(), handle_media_state_changed_(),
//   handle_volume_request_(), handle_mute_request_(), handle_play_uri_request_(),
//   set_volume_(), set_mute_state_(), control(), get_media_pipeline_state_(),
//   find_source_for_uri_(), try_execute_play_uri_(), save_volume_restore_state_(),
//   process_control_queue_(), handle_player_command_(), queue_command_(), queue_play_current_()
//
// - Media source task(s): handle_media_output_() via SourceBinding::write_audio().
//   Called from each source's decode task thread when streaming audio data.
//   Reads ps.active_source (atomic), writes ps.pending_frames (atomic), and calls
//   ps.speaker methods (speaker pointer is immutable after setup).
//
// - Speaker callback task: handle_speaker_playback_callback_() via speaker's
//   add_audio_output_callback(). Called when the speaker finishes writing frames to the DAC.
//   Reads ps.active_source (atomic), writes ps.pending_frames (atomic), and calls
//   active_source->notify_audio_played().
//
// control() is only called from the main loop (HA/automation commands).
// Source tasks use defer() for all requests (volume, mute, play_uri).
//
// Thread-safe communication:
// - FreeRTOS queue (media_control_command_queue_): control() -> loop() for play/command dispatch
// - defer(): SourceBinding::request_volume/request_mute/request_play_uri -> main loop
// - Atomic fields (active_source, pending_frames): shared between all three thread contexts
//
// Non-atomic pipeline fields (last_source, stopping_source, pending_source, playlist,
// playlist_index, repeat_mode) are only accessed from the main loop thread.

enum Pipeline : uint8_t {
  MEDIA_PIPELINE = 0,
};

enum RepeatMode : uint8_t {
  REPEAT_OFF = 0,
  REPEAT_ONE = 1,
  REPEAT_ALL = 2,
};

// Forward declaration
class SpeakerSourceMediaPlayer;

/// @brief Per-source listener binding that captures the source pointer at registration time.
/// Each binding implements MediaSourceListener and forwards callbacks to the player with the source identified.
/// Defined before PipelineContext so pipelines can own their bindings directly.
struct SourceBinding : public media_source::MediaSourceListener {
  SourceBinding(SpeakerSourceMediaPlayer *player, media_source::MediaSource *source, uint8_t pipeline)
      : player(player), source(source), pipeline(pipeline) {}
  SpeakerSourceMediaPlayer *player;
  media_source::MediaSource *source;
  uint8_t pipeline;

  // Implementations are in the .cpp file because SpeakerSourceMediaPlayer is only forward-declared here
  size_t write_audio(const uint8_t *data, size_t length, uint32_t timeout_ms,
                     const audio::AudioStreamInfo &stream_info) override;
  void report_state(media_source::MediaSourceState state) override;
  void request_volume(float volume) override;
  void request_mute(bool is_muted) override;
  void request_play_uri(const std::string &uri) override;
};

struct PipelineContext {
  /// @brief Timeout IDs for playlist delay, indexed by Pipeline enum
  static constexpr const char *const TIMEOUT_IDS[] = {"next_media"};

  speaker::Speaker *speaker{nullptr};
  optional<media_player::MediaPlayerSupportedFormat> format;

  std::atomic<media_source::MediaSource *> active_source{nullptr};
  media_source::MediaSource *last_source{nullptr};
  media_source::MediaSource *stopping_source{nullptr};  // Source we've asked to stop, awaiting IDLE
  media_source::MediaSource *pending_source{nullptr};   // Source we've asked to play, awaiting PLAYING

  // Each SourceBinding pairs a MediaSource* with its listener implementation.
  // Uses unique_ptr so binding addresses are stable and set_listener() can be called in add_media_source().
  // Uses std::vector because the count varies across instances (multiple speaker_source media players may exist).
  std::vector<std::unique_ptr<SourceBinding>> sources;

  // Dynamic allocation is unavoidable here: URIs from Home Assistant are arbitrary-length strings
  // (media URLs with tokens can easily exceed 500 bytes), and playlist size is unbounded.
  // Pre-allocating fixed buffers would waste significant RAM when idle without covering worst cases.
  std::vector<std::string> playlist;
  size_t playlist_index{0};
  RepeatMode repeat_mode{REPEAT_OFF};
  uint32_t playlist_delay_ms{0};

  // Track frames sent to speaker to correlate with playback callbacks.
  // Atomic because it is written from the main loop/source tasks and read/decremented from the speaker playback
  // callback.
  std::atomic<uint32_t> pending_frames{0};

  /// @brief Check if this pipeline is configured (has a speaker assigned)
  bool is_configured() const { return this->speaker != nullptr; }
};

struct MediaPlayerControlCommand {
  enum Type : uint8_t {
    PLAY_URI,          // Clear playlist, reset index, add URI, queue PLAY_CURRENT
    ENQUEUE_URI,       // Add URI to playlist, queue PLAY_CURRENT if idle
    PLAYLIST_ADVANCE,  // Advance index (or wrap for repeat_all), queue PLAY_CURRENT if more items
    PLAY_CURRENT,      // Play item at current playlist index (can retry if speaker not ready)
    SEND_COMMAND,      // Send command to active source
  };
  Type type;
  uint8_t pipeline;

  union {
    std::string *uri;  // Owned pointer, must delete after xQueueReceive (for PLAY_URI and ENQUEUE_URI)
    media_player::MediaPlayerCommand command;
  } data;
};

struct VolumeRestoreState {
  float volume;
  bool is_muted;
};

class SpeakerSourceMediaPlayer : public Component, public media_player::MediaPlayer {
  friend struct SourceBinding;

 public:
  float get_setup_priority() const override { return esphome::setup_priority::PROCESSOR; }
  void setup() override;
  void loop() override;
  void dump_config() override;

  // MediaPlayer implementations
  media_player::MediaPlayerTraits get_traits() override;
  bool is_muted() const override { return this->is_muted_; }

  // Percentage to increase or decrease the volume for volume up or volume down commands
  void set_volume_increment(float volume_increment) { this->volume_increment_ = volume_increment; }

  // Volume used initially on first boot when no volume had been previously saved
  void set_volume_initial(float volume_initial) { this->volume_initial_ = volume_initial; }

  void set_volume_max(float volume_max) { this->volume_max_ = volume_max; }
  void set_volume_min(float volume_min) { this->volume_min_ = volume_min; }

  /// @brief Adds a media source to a pipeline and registers this player as its listener
  void add_media_source(uint8_t pipeline, media_source::MediaSource *media_source);

  void set_speaker(uint8_t pipeline, speaker::Speaker *speaker) { this->pipelines_[pipeline].speaker = speaker; }
  void set_format(uint8_t pipeline, const media_player::MediaPlayerSupportedFormat &format) {
    this->pipelines_[pipeline].format = format;
  }

  Trigger<> *get_mute_trigger() { return &this->mute_trigger_; }
  Trigger<> *get_unmute_trigger() { return &this->unmute_trigger_; }
  Trigger<float> *get_volume_trigger() { return &this->volume_trigger_; }

  void set_playlist_delay_ms(uint8_t pipeline, uint32_t delay_ms);

 protected:
  // Callbacks from source bindings (pipeline index is captured at binding creation time)
  size_t handle_media_output_(uint8_t pipeline, media_source::MediaSource *source, const uint8_t *data, size_t length,
                              uint32_t timeout_ms, const audio::AudioStreamInfo &stream_info);
  void handle_media_state_changed_(uint8_t pipeline, media_source::MediaSource *source,
                                   media_source::MediaSourceState state);
  void handle_volume_request_(float volume);
  void handle_mute_request_(bool is_muted);
  void handle_play_uri_request_(uint8_t pipeline, const std::string &uri);

  void handle_speaker_playback_callback_(uint32_t frames, int64_t timestamp, uint8_t pipeline);

  // Receives commands from HA or from the voice assistant component
  // Sends commands to the media_control_command_queue_
  void control(const media_player::MediaPlayerCall &call) override;

  /// @brief Updates this->volume and saves volume/mute state to flash for restoration if publish is true.
  void set_volume_(float volume, bool publish = true);

  /// @brief Sets the mute state.
  /// @param mute_state If true, audio will be muted. If false, audio will be unmuted
  /// @param publish If true, saves volume/mute state to flash for restoration
  void set_mute_state_(bool mute_state, bool publish = true);

  /// @brief Saves the current volume and mute state to the flash for restoration.
  void save_volume_restore_state_();

  /// @brief Determine media player state from the media pipeline's active source
  /// @param media_source Active source for the media pipeline (may be nullptr)
  /// @param playlist_active Whether the media pipeline's playlist is in progress
  /// @param old_state Previous media player state (used for transition smoothing)
  /// @return The appropriate MediaPlayerState
  media_player::MediaPlayerState get_media_pipeline_state_(media_source::MediaSource *media_source,
                                                           bool playlist_active,
                                                           media_player::MediaPlayerState old_state) const;

  void process_control_queue_();
  void handle_player_command_(media_player::MediaPlayerCommand player_command, uint8_t pipeline);
  bool try_execute_play_uri_(const std::string &uri, uint8_t pipeline);
  media_source::MediaSource *find_source_for_uri_(const std::string &uri, uint8_t pipeline);
  void queue_command_(MediaPlayerControlCommand::Type type, uint8_t pipeline);
  void queue_play_current_(uint8_t pipeline, uint32_t delay_ms = 0);

  QueueHandle_t media_control_command_queue_;

  // Pipeline context for media pipeline. See THREADING MODEL at top of namespace for access rules.
  std::array<PipelineContext, 1> pipelines_;

  // Used to save volume/mute state for restoration on reboot
  ESPPreferenceObject pref_;

  Trigger<> mute_trigger_;
  Trigger<> unmute_trigger_;
  Trigger<float> volume_trigger_;

  // The amount to change the volume on volume up/down commands
  float volume_increment_;

  // The initial volume used by Setup when no previous volume was saved
  float volume_initial_;

  float volume_max_;
  float volume_min_;

  bool is_muted_{false};
};

}  // namespace esphome::speaker_source

#endif  // USE_ESP32
