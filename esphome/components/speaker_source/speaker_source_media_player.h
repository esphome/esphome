#pragma once

#ifdef USE_ESP_IDF

#include "esphome/components/audio/audio.h"

#include "esphome/components/media_source/media_source.h"
#include "esphome/components/media_player/media_player.h"
#include "esphome/components/speaker/speaker.h"

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/preferences.h"

#include <vector>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

namespace esphome {
namespace speaker_source {

enum Pipeline : size_t {
  MEDIA_PIPELINE = 0,
  ANNOUNCEMENT_PIPELINE = 1,
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
    PLAY_URI,
    ENQUEUE_URI,
    SEND_COMMAND,
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

class SpeakerSourceMediaPlayer : public Component, public media_player::MediaPlayer {
 public:
  float get_setup_priority() const override { return esphome::setup_priority::PROCESSOR; }
  void setup() override;
  void loop() override;

  // MediaPlayer implementations
  media_player::MediaPlayerTraits get_traits() override;
  bool is_muted() const override { return this->is_muted_; }

  void set_task_stack_in_psram(bool task_stack_in_psram) { this->task_stack_in_psram_ = task_stack_in_psram; }

  // Percentage to increase or decrease the volume for volume up or volume down commands
  void set_volume_increment(float volume_increment) { this->volume_increment_ = volume_increment; }

  // Volume used initially on first boot when no volume had been previously saved
  void set_volume_initial(float volume_initial) { this->volume_initial_ = volume_initial; }

  void set_volume_max(float volume_max) { this->volume_max_ = volume_max; }
  void set_volume_min(float volume_min) { this->volume_min_ = volume_min; }

  void set_announcement_speaker(speaker::Speaker *announcement_speaker) {
    this->announcement_speaker_ = announcement_speaker;
  }

  void add_media_source(media_source::MediaSource *media_source) { this->media_sources_.push_back(media_source); }
  void set_media_speaker(speaker::Speaker *media_speaker) { this->media_speaker_ = media_speaker; }

  void set_announcement_format(const media_player::MediaPlayerSupportedFormat &announcement_format) {
    this->announcement_format_ = announcement_format;
  }
  void set_media_format(const media_player::MediaPlayerSupportedFormat &media_format) {
    this->media_format_ = media_format;
  }

  Trigger<> *get_mute_trigger() const { return this->mute_trigger_; }
  Trigger<> *get_unmute_trigger() const { return this->unmute_trigger_; }
  Trigger<float> *get_volume_trigger() const { return this->volume_trigger_; }

  void set_playlist_delay_ms(size_t pipeline, uint32_t delay_ms);

 protected:
  size_t handle_media_output_callback_(media_source::MediaSource *source, uint8_t *data, size_t length,
                                       TickType_t ticks, audio::AudioStreamInfo stream_info, size_t pipeline);
  void handle_media_state_callback_(media_source::MediaSource *source, media_source::MediaSourceState state,
                                    size_t pipeline);
  void handle_speaker_playback_callback_(uint32_t frames, int64_t timestamp, size_t pipeline);
  void handle_volume_request_(media_source::MediaSource *source, float volume);
  void handle_mute_request_(media_source::MediaSource *source, bool is_muted);
  void handle_play_uri_request_(media_source::MediaSource *source, const std::string &uri, size_t pipeline);

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
  void clear_local_playlist_(size_t pipeline, bool keep_current_item);

  std::vector<media_source::MediaSource *> media_sources_;
  speaker::Speaker *media_speaker_{nullptr};
  speaker::Speaker *announcement_speaker_{nullptr};
  media_source::MediaSource *announcement_active_source_{nullptr};
  media_source::MediaSource *announcement_last_source_{nullptr};
  media_source::MediaSource *announcement_stopping_source_{nullptr};  // Source we've asked to stop, awaiting IDLE
  media_source::MediaSource *media_active_source_{nullptr};
  media_source::MediaSource *media_last_source_{nullptr};
  media_source::MediaSource *media_stopping_source_{nullptr};  // Source we've asked to stop, awaiting IDLE

  optional<media_player::MediaPlayerSupportedFormat> media_format_;

  optional<media_player::MediaPlayerSupportedFormat> announcement_format_;

  QueueHandle_t media_control_command_queue_;

  // Playlists use std::vector instead of std::deque because:
  // 1. Lower memory overhead (important for embedded systems)
  // 2. Better cache locality for frequent iteration in loop()
  // 3. erase(begin()) only happens once per song (~3 min intervals)
  // 4. Typical playlists are small (< 20 songs)
  std::vector<std::string> media_playlist_;
  std::vector<std::string> announcement_playlist_;
  SemaphoreHandle_t playlist_mutex_;
  bool media_repeat_one_{false};
  bool announcement_repeat_one_{false};
  uint32_t media_playlist_delay_ms_{0};
  uint32_t announcement_playlist_delay_ms_{0};

  // Track frames sent to speaker per pipeline to correlate with playback callbacks
  uint32_t media_pending_frames_{0};
  uint32_t announcement_pending_frames_{0};

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
