#pragma once

#include "esphome/core/defines.h"

#if defined(USE_ESP32) && defined(USE_SENDSPIN_CONTROLLER) && defined(USE_SENDSPIN_PLAYER)

#include "esphome/components/sendspin/sendspin_hub.h"

#include "esphome/components/audio/audio.h"
#include "esphome/components/media_source/media_source.h"

#include <sendspin/player_role.h>

namespace esphome::sendspin_ {

/// @brief Thin adapter media source for Sendspin.
///
/// Implements PlayerRoleListener to receive audio data from the sendspin-cpp library's
/// SyncTask and bridges it to ESPHome's MediaSource output pipeline. Also forwards
/// transport commands to the hub's controller role.
class SendspinMediaSource : public SendspinChild,
                            public media_source::MediaSource,
                            public sendspin::PlayerRoleListener {
 public:
  void setup() override;
  void dump_config() override;

  void set_static_delay_adjustable(bool adjustable);

  // MediaSource interface implementation
  bool play_uri(const std::string &uri) override;
  void handle_command(media_source::MediaSourceCommand command) override;
  bool can_handle(const std::string &uri) const override;
  bool has_internal_playlist() const override { return true; }

  void notify_volume_changed(float volume) override;
  void notify_mute_changed(bool is_muted) override;
  void notify_audio_played(uint32_t frames, int64_t timestamp) override;

 protected:
  // --- Sendspin PlayerRoleListener overrides ---

  /// @brief Writes decoded PCM audio to ESPHome's media source output pipeline.
  /// Called from the sync task's background thread.
  size_t on_audio_write(uint8_t *data, size_t length, uint32_t timeout_ms) override;

  /// @brief Called when a new audio stream starts (main loop thread).
  void on_stream_start() override;

  /// @brief Called when the audio stream ends (main loop thread).
  void on_stream_end() override;

  /// @brief Called when the audio stream is cleared (main loop thread).
  void on_stream_clear() override;

  /// @brief Called when volume changes (main loop thread).
  void on_volume_changed(uint8_t volume) override;

  /// @brief Called when mute state changes (main loop thread).
  void on_mute_changed(bool muted) override;

  sendspin::PlayerRole *player_role_{nullptr};

  /// @brief Stream parameters cached for use in the sync task's on_audio_write().
  ///
  /// Written on the main loop in on_stream_start(), read on the sync task in on_audio_write().
  /// This is not guarded by an atomic/lock, which is accepted for the following reasons:
  ///   - The field is written once per stream (typically once per boot for a fixed speaker config)
  ///     and the sample rate / channel count rarely change at runtime.
  ///   - The write in on_stream_start() completes before the media source transitions to PLAYING
  ///     (via an orchestrator round-trip through request_play_uri_ -> play_uri), and on_audio_write()
  ///     early-returns unless the state is PLAYING. The FreeRTOS queue primitives used by that
  ///     round-trip inject the memory barriers that publish the write.
  ///   - A torn read at a rare parameter change would at worst produce one distorted output buffer
  ///     of audio before the speaker reconfigures itself.
  audio::AudioStreamInfo stream_info_{};

  float cached_volume_{0.0f};

  bool cached_muted_{false};
  bool pending_start_{false};
  bool static_delay_adjustable_{false};
};

}  // namespace esphome::sendspin_

#endif
