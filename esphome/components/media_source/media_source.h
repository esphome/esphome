#pragma once

#include "esphome/components/audio/audio.h"
#include "esphome/core/helpers.h"

#include <cstdint>
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
  MEDIA_SOURCE_COMMAND_END = 0,  // Indicates source should end
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

// Forward declaration
class MediaSource;

/// @brief Interface for receiving callbacks from a MediaSource.
/// Replaces std::function callbacks with a single listener pointer to minimize overhead.
class MediaSourceListener {
 public:
  virtual size_t on_media_output(MediaSource *source, uint8_t *data, size_t length, TickType_t ticks_to_wait,
                                 audio::AudioStreamInfo stream_info, size_t pipeline) = 0;
  virtual void on_media_state_changed(MediaSource *source, MediaSourceState state, size_t pipeline) = 0;
  virtual void on_capabilities_changed(MediaSource *source, MediaSourceCapabilities capabilities) = 0;
  virtual void on_volume_request(MediaSource *source, float volume) = 0;
  virtual void on_mute_request(MediaSource *source, bool is_muted) = 0;
  virtual void on_play_uri_request(MediaSource *source, const std::string &uri, size_t pipeline) = 0;
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

  // === Listener: Source → Player ===

  /// @brief Set the listener that receives callbacks from this source
  /// @param listener Pointer to the MediaSourceListener implementation. Caller must ensure it outlives this source.
  void set_listener(MediaSourceListener *listener) { this->listener_ = listener; }

  /// @brief Get the current listener
  MediaSourceListener *get_listener() const { return this->listener_; }

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
  /// @brief Helper to update state and notify listener for a specific pipeline
  /// Sources should use this instead of directly modifying pipeline_states_
  /// @param state New state to set
  /// @param pipeline Pipeline index to update
  void set_state_(MediaSourceState state, size_t pipeline) {
    if (pipeline < this->pipeline_states_.size()) {
      if (this->pipeline_states_[pipeline] != state) {
        this->pipeline_states_[pipeline] = state;
        if (this->listener_ != nullptr) {
          this->listener_->on_media_state_changed(this, state, pipeline);
        }
      }
    }
  }

  std::string uri_prefix_;
  FixedVector<MediaSourceState> pipeline_states_;
  MediaSourceListener *listener_{nullptr};
};

}  // namespace media_source
}  // namespace esphome
