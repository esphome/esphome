#pragma once

#include "esphome/core/log.h"

#include "esphome/components/json/json_util.h"

#include <cstdint>
#include <string>

namespace esphome::http_request {

/// What an UpdateTarget reports about the install it was asked to perform.
enum class UpdateTargetState : uint8_t {
  UPDATE_TARGET_STATE_PROGRESS,
  UPDATE_TARGET_STATE_COMPLETE,
  UPDATE_TARGET_STATE_FAILED,
};

/** Receives install progress from an UpdateTarget.
 *
 * Implemented by the http_request update platform. Targets reach it through
 * UpdateTarget's report_* helpers, always from the main loop.
 */
class UpdateTargetListener {
 public:
  /// @p progress is meaningful for PROGRESS, @p message only for FAILED.
  virtual void on_target_state(UpdateTargetState state, float progress, const LogString *message) = 0;
};

/** Installs an image announced by an update manifest.
 *
 * The http_request update platform fetches and parses the manifest and then
 * hands the resolved image over to its target. Implement this in any component
 * that can flash something - this device, a secondary MCU, a display module -
 * to reuse the manifest handling instead of duplicating it.
 */
class UpdateTarget {
 public:
  /** Whether this entry from the manifest's `builds` array is for this target.
   *
   * The whole build object is passed so that each target decides for itself
   * which fields identify it. Called while parsing the manifest, which happens
   * in a separate task on ESP32, so implementations must not touch state
   * shared with the main loop.
   */
  virtual bool matches_build(JsonObjectConst build) const = 0;

  /// Key inside a matching build entry that holds the image `path` and `md5`.
  virtual const char *get_manifest_key() const { return "ota"; }

  /// Start installing the image at @p url, which must hash to @p md5.
  virtual void install(const std::string &url, const std::string &md5) = 0;

  void set_update_listener(UpdateTargetListener *listener) { this->listener_ = listener; }

 protected:
  void report_progress_(float progress) {
    this->report_(UpdateTargetState::UPDATE_TARGET_STATE_PROGRESS, progress, nullptr);
  }
  void report_complete_() { this->report_(UpdateTargetState::UPDATE_TARGET_STATE_COMPLETE, 0.0f, nullptr); }
  void report_failed_(const LogString *message) {
    this->report_(UpdateTargetState::UPDATE_TARGET_STATE_FAILED, 0.0f, message);
  }

  void report_(UpdateTargetState state, float progress, const LogString *message) {
    if (this->listener_ != nullptr)
      this->listener_->on_target_state(state, progress, message);
  }

  UpdateTargetListener *listener_{nullptr};
};

}  // namespace esphome::http_request
