#pragma once

#ifdef USE_ESP_IDF

#include "esphome/components/audio/audio.h"

#include "esphome/components/media_source/media_source.h"
#include "esphome/components/media_player/media_player.h"
#include "esphome/components/speaker/speaker.h"

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/preferences.h"

#include <array>
#include <vector>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

namespace esphome {
namespace speaker_source {

enum Pipeline : size_t {
  MEDIA_PIPELINE = 0,
  ANNOUNCEMENT_PIPELINE = 1,
};

enum RepeatMode : uint8_t {
  REPEAT_OFF = 0,
  REPEAT_ONE = 1,
  REPEAT_ALL = 2,
};

struct PipelineState {
  /// @brief Timeout IDs for playlist delay, indexed by Pipeline enum
  static constexpr const char *const TIMEOUT_IDS[] = {"next_media", "next_ann"};

  speaker::Speaker *speaker{nullptr};
  optional<media_player::MediaPlayerSupportedFormat> format;

  media_source::MediaSource *active_source{nullptr};
  media_source::MediaSource *last_source{nullptr};
  media_source::MediaSource *stopping_source{nullptr};  // Source we've asked to stop, awaiting IDLE

  std::vector<std::string> playlist;
  size_t playlist_index{0};
  RepeatMode repeat_mode{REPEAT_OFF};
  uint32_t playlist_delay_ms{0};

  // When non-empty, playlist_index indexes into these vectors
  // which contain the actual playlist indices in shuffled order
  std::vector<size_t> shuffle_indices;

  // Track frames sent to speaker to correlate with playback callbacks
  uint32_t pending_frames{0};

  /// @brief Check if this pipeline is configured (has a speaker assigned)
  bool is_configured() const { return this->speaker != nullptr; }
};

struct MediaCallCommand {
  optional<media_player::MediaPlayerCommand> command;
  optional<float> volume;
  optional<bool> announce;
  optional<std::string *> url;  // Must be manually deleted after receiving this struct from a queue
  optional<audio::AudioFile *> file;
  optional<bool> enqueue;
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
  size_t pipeline;  // MEDIA_PIPELINE or ANNOUNCEMENT_PIPELINE

  union {
    std::string *uri;  // Owned pointer, must delete after xQueueReceive (for PLAY_URI and ENQUEUE_URI)
    media_source::MediaSourceCommand source_command;
  } data;
};

struct VolumeRestoreState {
  float volume;
  bool is_muted;
};

class SpeakerSourceMediaPlayer : public Component,
                                 public media_player::MediaPlayer,
                                 public media_source::MediaSourceListener {
 public:
  float get_setup_priority() const override { return esphome::setup_priority::PROCESSOR; }
  void setup() override;
  void loop() override;

  // MediaPlayer implementations
  media_player::MediaPlayerTraits get_traits() override;
  bool is_muted() const override { return this->is_muted_; }

  // MediaSourceListener implementations
  size_t on_media_output(media_source::MediaSource *source, uint8_t *data, size_t length, TickType_t ticks,
                         audio::AudioStreamInfo stream_info, size_t pipeline) override;
  void on_media_state_changed(media_source::MediaSource *source, media_source::MediaSourceState state,
                              size_t pipeline) override;
  void on_capabilities_changed(media_source::MediaSource *source,
                               media_source::MediaSourceCapabilities capabilities) override;
  void on_volume_request(media_source::MediaSource *source, float volume) override;
  void on_mute_request(media_source::MediaSource *source, bool is_muted) override;
  void on_play_uri_request(media_source::MediaSource *source, const std::string &uri, size_t pipeline) override;

  void set_task_stack_in_psram(bool task_stack_in_psram) { this->task_stack_in_psram_ = task_stack_in_psram; }

  // Percentage to increase or decrease the volume for volume up or volume down commands
  void set_volume_increment(float volume_increment) { this->volume_increment_ = volume_increment; }

  // Volume used initially on first boot when no volume had been previously saved
  void set_volume_initial(float volume_initial) { this->volume_initial_ = volume_initial; }

  void set_volume_max(float volume_max) { this->volume_max_ = volume_max; }
  void set_volume_min(float volume_min) { this->volume_min_ = volume_min; }

  void add_media_source(media_source::MediaSource *media_source) { this->media_sources_.push_back(media_source); }

  void set_speaker(size_t pipeline, speaker::Speaker *speaker) { this->pipelines_[pipeline].speaker = speaker; }
  void set_format(size_t pipeline, const media_player::MediaPlayerSupportedFormat &format) {
    this->pipelines_[pipeline].format = format;
  }

  Trigger<> *get_mute_trigger() const { return this->mute_trigger_; }
  Trigger<> *get_unmute_trigger() const { return this->unmute_trigger_; }
  Trigger<float> *get_volume_trigger() const { return this->volume_trigger_; }

  void set_playlist_delay_ms(size_t pipeline, uint32_t delay_ms);

 protected:
  void handle_speaker_playback_callback_(uint32_t frames, int64_t timestamp, size_t pipeline);

  // Receives commands from HA or from the voice assistant component
  // Sends commands to the media_control_commanda_queue_
  void control(const media_player::MediaPlayerCall &call) override;

  /// @brief Updates this->volume and saves volume/mute state to flash for restortation if publish is true.
  void set_volume_(float volume, bool publish = true);

  /// @brief Sets the mute state. Restores previous volume if unmuting. Always saves volume/mute state to flash for
  /// restoration.
  /// @param mute_state If true, audio will be muted. If false, audio will be unmuted
  void set_mute_state_(bool mute_state);

  /// @brief Saves the current volume and mute state to the flash for restoration.
  void save_volume_restore_state_();

  void process_control_queue_();
  bool try_execute_play_uri_(const std::string &uri, size_t pipeline);
  media_source::MediaSource *find_source_for_uri_(const std::string &uri, size_t pipeline);
  void queue_command_(MediaPlayerControlCommand::Type type, size_t pipeline);
  void queue_play_current_(size_t pipeline, uint32_t delay_ms = 0);

  /// @brief Maps playlist_index through shuffle indices if shuffle is active
  /// @param pipeline The pipeline to get position for
  /// @return The actual playlist index to use
  size_t get_playlist_position_(size_t pipeline) const;

  /// @brief Generates shuffled indices for the playlist, keeping current track at current position
  void shuffle_playlist_(size_t pipeline);

  /// @brief Clears shuffle indices and adjusts playlist_index to maintain current track
  void unshuffle_playlist_(size_t pipeline);

  std::vector<media_source::MediaSource *> media_sources_;

  QueueHandle_t media_control_command_queue_;

  // Pipeline state for media (index 0) and announcement (index 1) pipelines
  // Note: No mutex needed - pipelines are only accessed from the main loop thread
  std::array<PipelineState, 2> pipelines_;

  bool task_stack_in_psram_;

  bool is_paused_{false};
  bool is_muted_{false};

  // The amount to change the volume on volume up/down commands
  float volume_increment_;

  // The initial volume used by Setup when no previous volume was saved
  float volume_initial_;

  float volume_max_;
  float volume_min_;

  // Used to save volume/mute state for restoration on reboot
  ESPPreferenceObject pref_;

  Trigger<> *mute_trigger_ = new Trigger<>();
  Trigger<> *unmute_trigger_ = new Trigger<>();
  Trigger<float> *volume_trigger_ = new Trigger<float>();
};

}  // namespace speaker_source
}  // namespace esphome

#endif
