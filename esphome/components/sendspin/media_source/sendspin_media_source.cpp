#include "sendspin_media_source.h"

#if defined(USE_ESP32) && defined(USE_SENDSPIN_CONTROLLER) && defined(USE_SENDSPIN_PLAYER)

#include "esphome/components/audio/audio.h"
#include "esphome/core/log.h"

#include <cmath>

namespace esphome::sendspin_ {

static const char *const TAG = "sendspin.media_source";

static constexpr char URI_PREFIX[] = "sendspin://";

void SendspinMediaSource::setup() {
  this->player_role_ = this->parent_->get_player_role();
  if (!this->player_role_) {
    ESP_LOGE(TAG, "Failed to get player role from hub");
    this->mark_failed();
    return;
  }

  // Push cached states to player role. They may have been set before setup() ran.
  this->player_role_->update_volume(std::roundf(this->cached_volume_ * 100.0f));
  this->player_role_->update_muted(this->cached_muted_);
  this->player_role_->set_static_delay_adjustable(this->static_delay_adjustable_);
}

void SendspinMediaSource::dump_config() {
  ESP_LOGCONFIG(TAG, "Sendspin Media Source: static_delay_adjustable=%s", YESNO(this->static_delay_adjustable_));
}

// THREAD CONTEXT: Main loop (invoked from ESPHome actions / config)
void SendspinMediaSource::set_static_delay_adjustable(bool adjustable) {
  this->static_delay_adjustable_ = adjustable;
  if (this->player_role_) {
    this->player_role_->set_static_delay_adjustable(adjustable);
  }
}

// --- MediaSource interface ---

bool SendspinMediaSource::can_handle(const std::string &uri) const { return uri.starts_with(URI_PREFIX); }

// THREAD CONTEXT: Main loop (media_source.h documents play_uri as main-loop only)
bool SendspinMediaSource::play_uri(const std::string &uri) {
  if (!this->is_ready() || this->is_failed() || !this->has_listener()) {
    return false;
  }

  if (this->get_state() != media_source::MediaSourceState::IDLE) {
    ESP_LOGE(TAG, "Cannot play '%s': source is busy", uri.c_str());
    return false;
  }

  if (!uri.starts_with(URI_PREFIX)) {
    ESP_LOGE(TAG, "Invalid URI: '%s'", uri.c_str());
    return false;
  }

  std::string sendspin_id = uri.substr(sizeof(URI_PREFIX) - 1);

  if (sendspin_id.empty()) {
    ESP_LOGE(TAG, "Invalid URI: '%s'", uri.c_str());
    return false;
  }

  ESP_LOGD(TAG, "sendspin_id: %s", sendspin_id.c_str());

  if (sendspin_id != "current") {
    // Connect to a new server as a websocket client
    this->parent_->connect_to_server("ws://" + sendspin_id);
  }

  // Tell the orchestrator we're now playing so it routes audio output from us
  this->pending_start_ = false;
  this->set_state_(media_source::MediaSourceState::PLAYING);

  return true;
}

// THREAD CONTEXT: Main loop (media_source.h documents handle_command as main-loop only)
void SendspinMediaSource::handle_command(media_source::MediaSourceCommand command) {
  switch (command) {
    case media_source::MediaSourceCommand::STOP: {
      if (!this->pending_start_) {
        // Ignore stop commands if we have a pending start, since the orchestrator may send a stop command before
        // play_uri
        ESP_LOGD(TAG, "Received STOP command, updating Sendspin state to EXTERNAL_SOURCE");
        this->parent_->update_state(sendspin::SendspinClientState::EXTERNAL_SOURCE);
      }
      break;
    }
    case media_source::MediaSourceCommand::PLAY:  // NOLINT(bugprone-branch-clone)
      this->parent_->send_client_command(sendspin::SendspinControllerCommand::PLAY, std::nullopt, std::nullopt);
      break;
    case media_source::MediaSourceCommand::PAUSE:
      this->parent_->send_client_command(sendspin::SendspinControllerCommand::PAUSE, std::nullopt, std::nullopt);
      break;
    case media_source::MediaSourceCommand::NEXT:
      this->parent_->send_client_command(sendspin::SendspinControllerCommand::NEXT, std::nullopt, std::nullopt);
      break;
    case media_source::MediaSourceCommand::PREVIOUS:
      this->parent_->send_client_command(sendspin::SendspinControllerCommand::PREVIOUS, std::nullopt, std::nullopt);
      break;
    case media_source::MediaSourceCommand::REPEAT_ALL:
      this->parent_->send_client_command(sendspin::SendspinControllerCommand::REPEAT_ALL, std::nullopt, std::nullopt);
      break;
    case media_source::MediaSourceCommand::REPEAT_ONE:
      this->parent_->send_client_command(sendspin::SendspinControllerCommand::REPEAT_ONE, std::nullopt, std::nullopt);
      break;
    case media_source::MediaSourceCommand::REPEAT_OFF:
      this->parent_->send_client_command(sendspin::SendspinControllerCommand::REPEAT_OFF, std::nullopt, std::nullopt);
      break;
    case media_source::MediaSourceCommand::SHUFFLE:
      this->parent_->send_client_command(sendspin::SendspinControllerCommand::SHUFFLE, std::nullopt, std::nullopt);
      break;
    case media_source::MediaSourceCommand::UNSHUFFLE:
      this->parent_->send_client_command(sendspin::SendspinControllerCommand::UNSHUFFLE, std::nullopt, std::nullopt);
      break;
    default:
      break;
  }
}

// THREAD CONTEXT: Main loop (orchestrator -> source notification)
void SendspinMediaSource::notify_volume_changed(float volume) {
  this->cached_volume_ = volume;
  if (this->player_role_) {
    this->player_role_->update_volume(std::roundf(volume * 100.0f));
  }
}

// THREAD CONTEXT: Main loop (orchestrator -> source notification)
void SendspinMediaSource::notify_mute_changed(bool is_muted) {
  this->cached_muted_ = is_muted;
  if (this->player_role_) {
    this->player_role_->update_muted(is_muted);
  }
}

// THREAD CONTEXT: Speaker playback callback thread (forwarded from the speaker).
// PlayerRole::notify_audio_played() is documented as thread-safe for this use.
void SendspinMediaSource::notify_audio_played(uint32_t frames, int64_t timestamp) {
  if (this->player_role_) {
    this->player_role_->notify_audio_played(frames, timestamp);
  }
}

// --- Sendspin PlayerRoleListener overrides ---

// THREAD CONTEXT: Sendspin sync task background thread. May block up to timeout_ms.
size_t SendspinMediaSource::on_audio_write(uint8_t *data, size_t length, uint32_t timeout_ms) {
  if (!this->has_listener() || (this->get_state() != media_source::MediaSourceState::PLAYING)) {
    vTaskDelay(pdMS_TO_TICKS(timeout_ms));
    return 0;
  }

  // PlayerRole::get_current_stream_params() is safe to call from the sync task.
  auto &params = this->player_role_->get_current_stream_params();
  if (!params.bit_depth.has_value() || !params.channels.has_value() || !params.sample_rate.has_value()) {
    vTaskDelay(pdMS_TO_TICKS(timeout_ms));
    return 0;
  }
  audio::AudioStreamInfo stream_info(*params.bit_depth, *params.channels, *params.sample_rate);

  return this->write_output(data, length, timeout_ms, stream_info);
}

// THREAD CONTEXT: Main loop (PlayerRoleListener lifecycle callback)
void SendspinMediaSource::on_stream_start() {
  this->parent_->update_state(sendspin::SendspinClientState::SYNCHRONIZED);

  if (!this->pending_start_) {
    // Dedup rapid on_stream_start() calls
    this->pending_start_ = true;
    // Request the orchestrator to start this source
    this->request_play_uri_("sendspin://current");
  }
}

// THREAD CONTEXT: Main loop (PlayerRoleListener lifecycle callback)
void SendspinMediaSource::on_stream_end() {
  if (this->get_state() != media_source::MediaSourceState::IDLE) {
    // Only set to IDLE if we were previously in a non-IDLE state, to avoid duplicate state changes
    this->set_state_(media_source::MediaSourceState::IDLE);
  }
}

// THREAD CONTEXT: Main loop (PlayerRoleListener callback)
void SendspinMediaSource::on_volume_changed(uint8_t volume) { this->request_volume_(volume / 100.0f); }

// THREAD CONTEXT: Main loop (PlayerRoleListener callback)
void SendspinMediaSource::on_mute_changed(bool muted) { this->request_mute_(muted); }

}  // namespace esphome::sendspin_

#endif  // USE_ESP32 && USE_SENDSPIN_PLAYER && USE_SENDSPIN_CONTROLLER
