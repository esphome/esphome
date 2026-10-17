#pragma once

#include <cmath>

namespace esphome::speaker {

/// Tracks a separate volume for announcements that start while nothing else is playing.
/// Each method that returns true means the caller must send a new volume to its speakers: get_volume() while
/// is_active(), otherwise the media player volume.
class AnnouncementVolume {
 public:
  /// Stores a new announcement volume. Returns true if it is in use and must be applied right away.
  bool set_volume(float volume) {
    this->volume_ = volume;
    return this->active_;
  }
  float get_volume() const { return this->volume_; }
  bool is_configured() const { return !std::isnan(this->volume_); }
  bool is_active() const { return this->active_; }

  /// Called when an announcement starts. Returns true if the speakers must switch to the announcement volume.
  /// @param other_audio_playing Media, or an announcement that does not use the announcement volume, is playing
  bool begin(bool other_audio_playing) {
    if (this->active_ || other_audio_playing || !this->is_configured()) {
      return false;
    }
    this->active_ = true;
    return true;
  }

  /// Returns true if the speakers must switch back to the media player volume.
  bool end() {
    if (!this->active_) {
      return false;
    }
    this->active_ = false;
    return true;
  }

  /// The media player volume was set and already sent to the speakers; it applies until the next announcement.
  void cancel() { this->active_ = false; }

 protected:
  float volume_{NAN};
  bool active_{false};
};

}  // namespace esphome::speaker
