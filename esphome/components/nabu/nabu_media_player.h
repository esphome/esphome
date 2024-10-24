#pragma once

#ifdef USE_ESP_IDF

#include "audio_mixer.h"
#include "audio_pipeline.h"

#include "nabu_media_helpers.h"

#include "esphome/components/media_player/media_player.h"
#include "esphome/components/speaker/speaker.h"

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/preferences.h"

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include <esp_http_client.h>

namespace esphome {
namespace nabu {

struct MediaCallCommand {
  optional<media_player::MediaPlayerCommand> command;
  optional<float> volume;
  optional<bool> announce;
  optional<bool> new_url;
  optional<bool> new_file;
};

struct VolumeRestoreState {
  float volume;
  bool is_muted;
};

class NabuMediaPlayer : public Component, public media_player::MediaPlayer {
 public:
  float get_setup_priority() const override { return esphome::setup_priority::LATE; }
  void setup() override;
  void loop() override;

  // MediaPlayer implementations
  media_player::MediaPlayerTraits get_traits() override;
  bool is_muted() const override { return this->is_muted_; }

  /// @brief Sets the ducking level for the media stream in the mixer
  /// @param decibel_reduction (uint8_t) The dB reduction level. For example, 0 is no change, 10 is a reduction by 10 dB
  /// @param duration (float) The duration (in seconds) for transitioning to the new ducking level
  void set_ducking_reduction(uint8_t decibel_reduction, float duration);

  void set_sample_rate(uint32_t sample_rate) { this->sample_rate_ = sample_rate; }

  // Percentage to increase or decrease the volume for volume up or volume down commands
  void set_volume_increment(float volume_increment) { this->volume_increment_ = volume_increment; }

  void set_volume_max(float volume_max) { this->volume_max_ = volume_max; }
  void set_volume_min(float volume_min) { this->volume_min_ = volume_min; }

  void set_speaker(speaker::Speaker *speaker) { this->speaker_ = speaker; }

  Trigger<> *get_mute_trigger() const { return this->mute_trigger_; }
  Trigger<> *get_unmute_trigger() const { return this->unmute_trigger_; }
  Trigger<float> *get_volume_trigger() const { return this->volume_trigger_; }

  void play_file(MediaFile *media_file, bool announcement);

 protected:
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

  // Reads commands from media_control_command_queue_. Starts pipelines and mixer if necessary.
  void watch_media_commands_();

  std::unique_ptr<AudioPipeline> media_pipeline_;
  std::unique_ptr<AudioPipeline> announcement_pipeline_;
  std::unique_ptr<AudioMixer> audio_mixer_;

  speaker::Speaker *speaker_{nullptr};

  // Monitors the mixer task
  void watch_mixer_();

  // Starts the ``type`` pipeline with a ``url`` or file. Starts the mixer, pipeline, and speaker tasks if necessary.
  // Unpauses if starting media in paused state
  esp_err_t start_pipeline_(AudioPipelineType type, bool url);

  AudioPipelineState media_pipeline_state_{AudioPipelineState::STOPPED};
  AudioPipelineState announcement_pipeline_state_{AudioPipelineState::STOPPED};

  optional<std::string> media_url_{};          // only modified by control function
  optional<std::string> announcement_url_{};   // only modified by control function
  optional<MediaFile *> media_file_{};         // only modified by play_file function
  optional<MediaFile *> announcement_file_{};  // only modified by play_file function

  QueueHandle_t media_control_command_queue_;

  uint32_t sample_rate_;

  bool is_paused_{false};
  bool is_muted_{false};

  // The amount to change the volume on volume up/down commands
  float volume_increment_;

  float volume_max_;
  float volume_min_;

  // Used to save volume/mute state for restoration on reboot
  ESPPreferenceObject pref_;

  Trigger<> *mute_trigger_ = new Trigger<>();
  Trigger<> *unmute_trigger_ = new Trigger<>();
  Trigger<float> *volume_trigger_ = new Trigger<float>();
};

}  // namespace nabu
}  // namespace esphome

#endif
