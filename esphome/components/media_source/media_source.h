#pragma once

#include "esphome/components/audio/audio.h"
#include "esphome/core/helpers.h"

#include <cstdint>
#include <functional>
#include <string>

#ifdef USE_ESP32
#include <freertos/FreeRTOS.h>
#endif

namespace esphome {
namespace media_source {

/// @brief Represents the current state of a media source
enum class MediaSourceState : uint8_t {
  IDLE = 0,       // Not playing, ready to accept play_uri
  PLAYING = 1,    // Currently playing media
  PAUSED = 2,     // Playback paused, can be resumed
  BUFFERING = 3,  // Buffering data (for streaming sources)
  ERROR = 4,      // Error occurred during playback
};

/// @brief Commands that can be sent to a media source
enum MediaSourceCommand : uint8_t {
  MEDIA_SOURCE_COMMAND_NOP = 0,
  MEDIA_SOURCE_COMMAND_END,  // Indicates source should end
  MEDIA_SOURCE_COMMAND_PLAY,
  MEDIA_SOURCE_COMMAND_PAUSE,
  MEDIA_SOURCE_COMMAND_TOGGLE,  // Toggle play/pause (media player converts to PLAY or PAUSE)
  MEDIA_SOURCE_COMMAND_STOP,    // Indicates source should end, and, if a smart, forward the command for the group
  MEDIA_SOURCE_COMMAND_NEXT,
  MEDIA_SOURCE_COMMAND_PREVIOUS,
  MEDIA_SOURCE_COMMAND_ENQUEUE,         // Add URI to internal playlist
  MEDIA_SOURCE_COMMAND_CLEAR_PLAYLIST,  // Clear internal playlist
  MEDIA_SOURCE_COMMAND_REPEAT_ALL,      // Enable repeat-all mode
  MEDIA_SOURCE_COMMAND_REPEAT_ONE,      // Enable repeat-one mode
  MEDIA_SOURCE_COMMAND_REPEAT_OFF,      // Disable repeat mode
  MEDIA_SOURCE_COMMAND_SHUFFLE,         // Shuffle playlist
  MEDIA_SOURCE_COMMAND_UNSHUFFLE,       // Unshuffle playlist
  MEDIA_SOURCE_COMMAND_GROUP_JOIN,      // Join another group
};

/// @brief Capabilities that a media source can advertise
struct MediaSourceCapabilities {
  bool supports_pause{false};           // Can pause/resume playback
  bool supports_next_track{false};      // Can skip to next track
  bool supports_previous_track{false};  // Can skip to previous track
  bool supports_volume_control{false};  // Source needs volume notifications
  bool has_internal_playlist{false};    // Source manages its own playlist
  bool supports_group_join{false};      // Can join a group
};

/// @brief Abstract base class for media sources
/// MediaSource provides audio data to a MediaPlayer. Sources are "dumb" - they don't
/// automatically stop themselves or switch tracks. The MediaPlayer is responsible for
/// orchestrating multiple sources.
///
/// Sources support multiple pipelines (e.g., media + announcement) by maintaining
/// independent state per pipeline. This allows a single HTTP source to stream music
/// on pipeline 0 while simultaneously streaming a TTS announcement on pipeline 1.
class MediaSource {
 public:
  virtual ~MediaSource() = default;

  // === Pipeline Initialization ===

  /// @brief Initialize pipeline support
  /// Must be called before any playback operations. Allocates resources for the specified
  /// number of pipelines. Each pipeline maintains independent state and can play different content.
  /// @param pipeline_count Number of pipelines to support (typically 1 or 2)
  virtual void init_pipelines(size_t pipeline_count) {
    this->pipeline_states_.init(pipeline_count);
    for (size_t i = 0; i < pipeline_count; i++) {
      this->pipeline_states_.push_back(MediaSourceState::IDLE);
    }
  }

  // === Playback Control ===

  /// @brief Start playing the given URI on a specific pipeline
  /// Sources should validate the URI and pipeline state, returning false if the pipeline is busy.
  /// The MediaPlayer is responsible for stopping active sources before starting a new one.
  /// @param uri The URI to play (e.g., "file://my_audio", "http://stream_url")
  /// @param pipeline The pipeline index to use (0 = media, 1 = announcement, etc.)
  /// @return true if playback started successfully, false otherwise
  virtual bool play_uri(const std::string &uri, size_t pipeline) = 0;

  /// @brief Enqueue a URI to the source's internal playlist (for sources with has_internal_playlist)
  /// Sources without internal playlist support can ignore this (default implementation does nothing).
  /// @param uri The URI to enqueue
  /// @param pipeline The pipeline index to use
  /// @return true if enqueue was successful, false otherwise
  virtual bool enqueue_uri(const std::string &uri, size_t pipeline) { return false; }

  /// @brief Handle playback commands (pause, stop, next, etc.) for a specific pipeline
  /// @param command The command to execute
  /// @param pipeline The pipeline index to control
  virtual void handle_command(MediaSourceCommand command, size_t pipeline) = 0;

  /// @brief Get current capabilities
  /// Can change dynamically (e.g., pause becomes available after playback starts)
  /// @return Current capabilities of this source
  virtual MediaSourceCapabilities get_capabilities() = 0;

  // === State Access ===

  /// @brief Get current playback state for a specific pipeline
  /// @param pipeline The pipeline index to query
  /// @return Current state of this pipeline, or IDLE if pipeline index is invalid
  MediaSourceState get_state(size_t pipeline) const {
    return pipeline < this->pipeline_states_.size() ? this->pipeline_states_[pipeline] : MediaSourceState::IDLE;
  }

  // === Configuration ===

  /// @brief Set the URI prefix this source handles (e.g., "file://", "http://")
  void set_uri_prefix(const std::string &prefix) { this->uri_prefix_ = prefix; }

  /// @brief Get the URI prefix this source handles
  const std::string &get_uri_prefix() const { return this->uri_prefix_; }

  // === Callbacks: Source → Player ===

  /// @brief Set callback for audio data output
  /// Called when source has PCM audio data ready to play
  /// @param callback Function that writes data to speaker, includes stream info and pipeline, returns bytes written
  void set_output_callback(
      std::function<size_t(MediaSource *, uint8_t *, size_t, TickType_t, audio::AudioStreamInfo, size_t)> &&callback) {
    this->output_callback_ = [callback, this](uint8_t *data, size_t len, TickType_t ticks, audio::AudioStreamInfo info,
                                              size_t pipeline) {
      return callback(this, data, len, ticks, info, pipeline);
    };
  }

  /// @brief Set callback for state changes
  /// Called whenever playback state changes (IDLE → PLAYING, etc.)
  /// @param callback Function receiving source, new state, and pipeline index
  void set_state_callback(std::function<void(MediaSource *, MediaSourceState, size_t)> &&callback) {
    this->state_callback_ = [callback, this](MediaSourceState state, size_t pipeline) {
      callback(this, state, pipeline);
    };
  }

  /// @brief Set callback for capability changes
  /// Called when source capabilities change dynamically
  void set_capabilities_callback(std::function<void(MediaSource *, MediaSourceCapabilities)> &&callback) {
    this->capabilities_callback_ = [callback, this](MediaSourceCapabilities caps) { callback(this, caps); };
  }

  // === Callbacks: Source → Player (Smart Sources Only) ===

  /// @brief Set callback for volume change requests
  /// Used by smart sources (e.g., snapcast) to request volume changes from the player
  /// The player will then call notify_volume_changed() on all sources
  void set_volume_request_callback(std::function<void(MediaSource *, float)> &&callback) {
    this->volume_request_callback_ = [callback, this](float volume) { callback(this, volume); };
  }

  /// @brief Set callback for mute state change requests
  /// Used by smart sources (e.g., snapcast) to request mute state changes from the player
  /// The player will then call notify_mute_changed() on all sources
  void set_mute_request_callback(std::function<void(MediaSource *, bool)> &&callback) {
    this->mute_request_callback_ = [callback, this](bool is_muted) { callback(this, is_muted); };
  }

  /// @brief Set callback for play URI requests
  /// Used by smart sources to request the player start playing a specific URI
  /// The URI may be routed to a different source (e.g., server requests HTTP stream)
  /// @param callback Function receiving source, URI string, and pipeline index
  void set_play_uri_request_callback(std::function<void(MediaSource *, const std::string &, size_t)> &&callback) {
    this->play_uri_request_callback_ = [callback, this](const std::string &uri, size_t pipeline) {
      callback(this, uri, pipeline);
    };
  }

  // === Callbacks: Player → Source ===

  /// @brief Notify source that volume changed
  /// Called when volume changes from Home Assistant or another source
  /// Most sources can ignore this. Override for smart sources like snapcast.
  /// @param volume New volume level (0.0 to 1.0)
  virtual void notify_volume_changed(float volume) {}

  /// @brief Notify source that mute state changed
  /// Most sources can ignore this. Override for smart sources like snapcast.
  /// @param is_muted New mute state
  virtual void notify_mute_changed(bool is_muted) {}

  /// @brief Notify source about audio that has been played
  /// Called when the speaker reports that audio frames have been written to the DAC.
  /// Sources can override this to track playback progress for synchronization or logging.
  /// @param frames Number of audio frames that were played
  /// @param timestamp System time in microseconds when the frames were written to the DAC
  /// @param pipeline Pipeline index where the audio was played
  virtual void notify_audio_played(uint32_t frames, int64_t timestamp, size_t pipeline) {}

 protected:
  /// @brief Helper to update state and trigger callback for a specific pipeline
  /// Sources should use this instead of directly modifying pipeline_states_
  /// @param state New state to set
  /// @param pipeline Pipeline index to update
  void set_state_(MediaSourceState state, size_t pipeline) {
    if (pipeline < this->pipeline_states_.size()) {
      if (this->pipeline_states_[pipeline] != state) {
        this->pipeline_states_[pipeline] = state;
        if (this->state_callback_) {
          this->state_callback_(state, pipeline);
        }
      }
    }
  }

  std::string uri_prefix_;
  FixedVector<MediaSourceState> pipeline_states_;

  // Callbacks to MediaPlayer
  std::function<size_t(uint8_t *, size_t, TickType_t, audio::AudioStreamInfo, size_t)> output_callback_;
  std::function<void(MediaSourceState, size_t)> state_callback_;
  std::function<void(MediaSourceCapabilities)> capabilities_callback_;
  std::function<void(float)> volume_request_callback_;
  std::function<void(bool)> mute_request_callback_;
  std::function<void(const std::string &, size_t)> play_uri_request_callback_;
};

}  // namespace media_source
}  // namespace esphome
