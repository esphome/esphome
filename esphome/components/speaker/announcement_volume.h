#pragma once

#include <cmath>
#include <utility>

namespace esphome::speaker {

/// Tracks a separate volume for announcements that start while nothing else is playing.
/// The methods that change the volume take `apply`, called with the volume the speakers must switch to.
class AnnouncementVolume {
 public:
  /// Stores a new announcement volume and applies it right away if it is in use.
  template<typename F> void set_volume(float volume, F &&apply) {
    this->volume_ = volume;
    if (this->active_) {
      std::forward<F>(apply)(volume);
    }
  }
  float get_volume() const { return this->volume_; }
  bool is_configured() const { return !std::isnan(this->volume_); }
  bool is_active() const { return this->active_; }

  /// Called when an announcement starts. Switches to the announcement volume unless other audio is playing.
  /// @param other_audio_playing Media, or an announcement that does not use the announcement volume, is playing
  template<typename F> void begin(bool other_audio_playing, F &&apply) {
    if (this->active_ || other_audio_playing || !this->is_configured()) {
      return;
    }
    this->active_ = true;
    this->log_begin_();
    std::forward<F>(apply)(this->volume_);
  }

  /// Switches back to the media player volume if the announcement volume is in use.
  template<typename F> void end(float media_volume, F &&apply) {
    if (!this->active_) {
      return;
    }
    this->active_ = false;
    this->log_end_(media_volume);
    std::forward<F>(apply)(media_volume);
  }

  /// The media player volume was set and already sent to the speakers; it applies until the next announcement.
  void cancel() { this->active_ = false; }

  /// Called when an item starts on a playlist that holds announcements, and may hold media too.
  /// Media switches back to the media volume. An announcement that follows media, or starts the playlist, switches to
  /// the announcement volume unless other audio is playing. One that follows another announcement keeps the volume
  /// that one used, including a volume change made while it played.
  template<typename F>
  void item_started(bool is_announcement, bool other_audio_playing, float media_volume, F &&apply) {
    if (!is_announcement) {
      this->end(media_volume, apply);
    } else if (!this->last_item_announcement_) {
      this->begin(other_audio_playing, apply);
    }
    this->last_item_announcement_ = is_announcement;
  }

  /// Called when the playlist that holds announcements has finished. Switches back to the media volume.
  template<typename F> void playlist_finished(float media_volume, F &&apply) {
    this->last_item_announcement_ = false;
    this->end(media_volume, std::forward<F>(apply));
  }

 protected:
  void log_begin_() const;
  void log_end_(float media_volume) const;

  float volume_{NAN};
  bool active_{false};
  bool last_item_announcement_{false};
};

}  // namespace esphome::speaker
