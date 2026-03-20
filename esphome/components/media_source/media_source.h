#pragma once

#include "esphome/components/audio/audio.h"
#include "esphome/core/helpers.h"

#include <cstdint>
#include <string>

namespace esphome::media_source {

enum class MediaSourceState : uint8_t {
  IDLE,     // Not playing, ready to accept play_uri
  PLAYING,  // Currently playing media
  PAUSED,   // Playback paused, can be resumed
  ERROR,    // Error occurred during playback; sources are responsible for logging their own error details
};

/// @brief Commands that are sent from the orchestrator to a media source
enum class MediaSourceCommand : uint8_t {
  // All sources should support these basic commands.
  PLAY,
  PAUSE,
  STOP,

  // Only sources with internal playlists will handle these; simple sources should ignore them.
  NEXT,
  PREVIOUS,
  CLEAR_PLAYLIST,
  REPEAT_ALL,
  REPEAT_ONE,
  REPEAT_OFF,
  SHUFFLE,
  UNSHUFFLE,
};

/// @brief Callbacks from a MediaSource to its orchestrator
class MediaSourceListener {
 public:
  virtual ~MediaSourceListener() = default;

  // Callbacks that all sources use to send data and state changes to the orchestrator.
  /// @brief Send audio data to the listener
  virtual size_t write_audio(const uint8_t *data, size_t length, uint32_t timeout_ms,
                             const audio::AudioStreamInfo &stream_info) = 0;
  /// @brief Notify listener of state changes
  virtual void report_state(MediaSourceState state) = 0;

  // Callbacks from smart sources requesting the orchestrator to change volume, mute, or start a new URI.
  // Simple sources never invoke these.
  /// @brief Request the orchestrator to change volume
  virtual void request_volume(float volume) {}
  /// @brief Request the orchestrator to change mute state
  virtual void request_mute(bool is_muted) {}
  /// @brief Request the orchestrator to play a new URI
  virtual void request_play_uri(const std::string &uri) {}
};

/// @brief Abstract base class for media sources
/// MediaSource provides audio data to an orchestrator via the MediaSourceListener interface. It also receives commands
/// from the orchestrator to control playback.
class MediaSource {
 public:
  virtual ~MediaSource() = default;

  // === Playback Control ===

  /// @brief Start playing the given URI
  /// Sources should validate the URI and state, returning false if the source is busy.
  /// The orchestrator is responsible for stopping active sources before starting a new one.
  /// @note Must only be called from the main loop.
  /// @param uri URI to play; e.g., "http://stream_url"
  /// @return true if playback started successfully, false otherwise
  virtual bool play_uri(const std::string &uri) = 0;

  /// @brief Handle playback commands; e.g., pause, stop, next, etc.
  /// @note Must only be called from the main loop.
  /// @param command Command to execute
  virtual void handle_command(MediaSourceCommand command) = 0;

  /// @brief Whether this source manages its own playlist internally
  /// Smart sources that handle next/previous/repeat/shuffle should override this to return true.
  virtual bool has_internal_playlist() const { return false; }

  // === State Access ===

  /// @brief Get current playback state
  /// @note Must only be called from the main loop.
  /// @return Current state of this source
  MediaSourceState get_state() const { return this->state_; }

  // === URI Matching ===

  /// @brief Check if this source can handle the given URI
  /// Each source must override this to match its supported URI scheme(s).
  /// @param uri URI to check
  /// @return true if this source can handle the URI
  virtual bool can_handle(const std::string &uri) const = 0;

  // === Listener: Source -> Orchestrator ===

  /// @brief Set the listener that receives callbacks from this source
  /// @param listener Pointer to the MediaSourceListener implementation
  void set_listener(MediaSourceListener *listener) { this->listener_ = listener; }

  /// @brief Check if a listener has been registered
  bool has_listener() const { return this->listener_ != nullptr; }

  /// @brief Write audio data to the listener
  /// @param data Pointer to audio data buffer (not modified by this method)
  /// @param length Number of bytes to write
  /// @param timeout_ms Milliseconds to wait if the listener can't accept data immediately
  /// @param stream_info Audio stream format information
  /// @return Number of bytes written, or 0 if no listener is set
  size_t write_output(const uint8_t *data, size_t length, uint32_t timeout_ms,
                      const audio::AudioStreamInfo &stream_info) {
    if (this->listener_ != nullptr) {
      return this->listener_->write_audio(data, length, timeout_ms, stream_info);
    }
    return 0;
  }

  // === Callbacks: Orchestrator -> Source ===

  /// @brief Notify the source that volume changed
  /// Simple sources ignore this. Override for smart sources that track volume state.
  /// @param volume New volume level (0.0 to 1.0)
  virtual void notify_volume_changed(float volume) {}

  /// @brief Notify the source that mute state changed
  /// Simple sources ignore this. Override for smart sources that track mute state.
  /// @param is_muted New mute state
  virtual void notify_mute_changed(bool is_muted) {}

  /// @brief Notify the source about audio that has been played
  /// Called when the speaker reports that audio frames have been written to the DAC.
  /// Sources can override this to track playback progress for synchronization.
  /// @param frames Number of audio frames that were played
  /// @param timestamp System time in microseconds when the frames finished writing to the DAC
  virtual void notify_audio_played(uint32_t frames, int64_t timestamp) {}

 protected:
  /// @brief Update state and notify listener
  /// This is the only way to change state_, ensuring listener notifications always fire.
  /// Sources running FreeRTOS tasks should signal via event groups and call this from loop().
  /// @note Must only be called from the main loop.
  /// @param state New state to set
  void set_state_(MediaSourceState state) {
    if (this->state_ != state) {
      this->state_ = state;
      if (this->listener_ != nullptr) {
        this->listener_->report_state(state);
      }
    }
  }

  // === Listener request helpers for smart sources ===
  // Smart sources (those with internal playlists or external control) use these to request
  // the orchestrator to take actions. Simple sources never call these.

  /// @brief Request the orchestrator to play a new URI
  void request_play_uri_(const std::string &uri) {
    if (this->listener_ != nullptr) {
      this->listener_->request_play_uri(uri);
    }
  }

  /// @brief Request the orchestrator to change volume
  void request_volume_(float volume) {
    if (this->listener_ != nullptr) {
      this->listener_->request_volume(volume);
    }
  }

  /// @brief Request the orchestrator to change mute state
  void request_mute_(bool is_muted) {
    if (this->listener_ != nullptr) {
      this->listener_->request_mute(is_muted);
    }
  }

 private:
  // Private to enforce the invariant that listener notifications always fire on state changes.
  // All state transitions must go through set_state_() which couples the update with notification.
  MediaSourceState state_{MediaSourceState::IDLE};
  MediaSourceListener *listener_{nullptr};
};

}  // namespace esphome::media_source
