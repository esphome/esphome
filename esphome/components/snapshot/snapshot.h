#pragma once

#ifdef USE_HOST
#include "esphome/core/automation.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

// Directory snapshots are written to. Normally set by codegen to a folder under .esphome; the
// fallback keeps the component compiling for static analysis, where no defines.h is generated.
#ifndef ESPHOME_SNAPSHOT_DIR
#define ESPHOME_SNAPSHOT_DIR "."
#endif

namespace esphome::snapshot {

/// Base for anything that can hand over the picture it is showing so it can be written to a file.
///
/// A subclass says how big the picture is and fills in the pixels. Everything else - picking a
/// name, staying inside the snapshot directory, not writing over anything, encoding the file, and
/// timing the frames of an animation - is done here, so every component that can take a snapshot
/// behaves the same way.
class Snapshot {
 public:
  // Declared here and defined where Recording is complete, which unique_ptr needs.
  Snapshot();
  virtual ~Snapshot();

  /// Set the word generated names start with. Codegen passes the component id, so with more than
  /// one display in a device it is clear which one a file came from.
  void set_snapshot_prefix(const char *prefix) { this->snapshot_prefix_ = prefix; }

  /// Write the current picture to a BMP file in the snapshot directory.
  ///
  /// Pass nullptr to have a name made up from the prefix and the current time. A file that is
  /// already there is never written over. Returns true if a file was written.
  bool take_snapshot(const char *filename);

  /// Record what is shown as an animated GIF file in the snapshot directory.
  ///
  /// The first frame is taken now and the rest follow at `frame_rate` frames a second, in the
  /// background. Names work as for take_snapshot(), with ".gif" in place of ".bmp". Only one
  /// recording can run at a time. Returns true if the recording started.
  bool take_animation(const char *filename, uint32_t frames, float frame_rate);

  /// Log that an action-triggered snapshot did not write a file.
  static void log_action_failed();

 protected:
  /// Width of the picture in pixels.
  virtual int snapshot_width() = 0;
  /// Height of the picture in pixels.
  virtual int snapshot_height() = 0;
  /// Fill in the picture: three bytes per pixel in blue, green, red order, topmost row first, with
  /// `row_stride` bytes from the start of one row to the start of the next. Returns false, having
  /// logged why, if the picture could not be read.
  virtual bool capture_bgr(uint8_t *dest, size_t row_stride) = 0;

  const char *snapshot_prefix_{"snapshot"};

 private:
  struct Recording;

  /// Add a frame to the recording and arrange for the next one. Returns false if the recording
  /// had to be given up.
  bool record_frame_();

  std::unique_ptr<Recording> recording_;
};

template<typename... Ts> class SnapshotAction final : public Action<Ts...>, public Parented<Snapshot> {
 public:
  TEMPLATABLE_VALUE(std::string, filename)

  /// Make the action record an animation instead of taking a single picture.
  void set_animation(uint32_t frames, float frame_rate) {
    this->frames_ = frames;
    this->frame_rate_ = frame_rate;
  }

 protected:
  void play(const Ts &...x) override {
    std::string filename;
    if (this->filename_.has_value())
      filename = this->filename_.value(x...);
    const char *name = this->filename_.has_value() ? filename.c_str() : nullptr;
    const bool ok = this->frames_ == 0 ? this->parent_->take_snapshot(name)
                                       : this->parent_->take_animation(name, this->frames_, this->frame_rate_);
    if (!ok)
      this->parent_->log_action_failed();
  }

  uint32_t frames_{0};
  float frame_rate_{0};
};

}  // namespace esphome::snapshot

#endif
