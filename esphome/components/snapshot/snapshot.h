#pragma once

#ifdef USE_HOST
#include "esphome/core/automation.h"

#include <cstddef>
#include <cstdint>
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
/// name, staying inside the snapshot directory, not writing over anything, and encoding the file -
/// is done here, so every component that can take a snapshot behaves the same way.
class Snapshot {
 public:
  virtual ~Snapshot() = default;

  /// Set the word generated names start with. Codegen passes the component id, so with more than
  /// one display in a device it is clear which one a file came from.
  void set_snapshot_prefix(const char *prefix) { this->snapshot_prefix_ = prefix; }

  /// Write the current picture to a BMP file in the snapshot directory.
  ///
  /// Pass nullptr to have a name made up from the prefix and the current time. A file that is
  /// already there is never written over. Returns true if a file was written.
  bool take_snapshot(const char *filename);

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
};

template<typename... Ts> class SnapshotAction final : public Action<Ts...>, public Parented<Snapshot> {
 public:
  TEMPLATABLE_VALUE(std::string, filename)

 protected:
  void play(const Ts &...x) override {
    bool ok;
    if (this->filename_.has_value()) {
      ok = this->parent_->take_snapshot(this->filename_.value(x...).c_str());
    } else {
      ok = this->parent_->take_snapshot(nullptr);
    }
    if (!ok)
      this->parent_->log_action_failed();
  }
};

}  // namespace esphome::snapshot

#endif
