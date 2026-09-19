#ifdef USE_HOST
#include "snapshot.h"
#include "gif.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#include <fcntl.h>
#include <strings.h>
#include <unistd.h>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <memory>
#include <utility>

namespace esphome::snapshot {

namespace {

constexpr const char *const TAG = "snapshot";

// Longest name we will build a path from. NAME_MAX is 255 and we may append a collision suffix.
constexpr size_t MAX_NAME_LENGTH = 200;
// Give up rather than spin forever if every candidate name is taken.
constexpr unsigned MAX_NAME_ATTEMPTS = 1000;
// A BMP file header followed by a BITMAPINFOHEADER, which is where the pixels start.
constexpr size_t BMP_HEADER_SIZE = 54;
constexpr size_t BMP_INFO_HEADER_SIZE = 40;
constexpr int BMP_BITS_PER_PIXEL = 24;

// The largest width or height a GIF file can hold.
constexpr int GIF_MAX_SIZE = 0xFFFF;

/// True if the name already ends in the extension, which is four characters including the dot. The
/// comparison ignores case, so "shot.BMP" is left alone rather than turned into "shot.BMP.bmp".
bool has_extension(const std::string &name, const char *extension) {
  return name.size() >= 4 && strcasecmp(name.c_str() + name.size() - 4, extension) == 0;
}

/// Reduce a user supplied name to a single safe path component. Everything outside the allowed set
/// is replaced, so "..", "/" and absolute paths cannot escape the snapshot directory.
/// Returns an empty string if nothing usable is left.
std::string sanitise_filename(const char *const name, const char *extension, bool *name_changed) {
  std::string result;
  bool all_dots = true;
  bool changed = false;
  for (const char *p = name; *p != '\0'; p++) {
    if (result.size() >= MAX_NAME_LENGTH) {
      changed = true;
      break;
    }
    char c = *p;
    if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '.' || c == '_' || c == '-')) {
      c = '_';
      changed = true;
    }
    if (c != '.')
      all_dots = false;
    result.push_back(c);
  }
  if (all_dots) {
    *name_changed = true;
    return "";
  }
  if (!has_extension(result, extension))
    result += extension;
  *name_changed = changed;
  return result;
}

/// Insert "-<attempt>" before the file extension, e.g. "shot.bmp" -> "shot-1.bmp".
std::string add_suffix(const std::string &name, unsigned attempt) {
  char suffix[12];
  snprintf(suffix, sizeof(suffix), "-%u", attempt);
  auto dot = name.rfind('.');
  if (dot == std::string::npos)
    return name + suffix;
  return name.substr(0, dot) + suffix + name.substr(dot);
}

/// Directory snapshots are written to. The environment variable lets a test redirect output
/// without rebuilding, matching how the host platform handles ESPHOME_PREFDIR.
const char *snapshot_dir() {
  const char *dir = getenv("ESPHOME_SNAPSHOT_DIR");  // NOLINT(concurrency-mt-unsafe)
  return dir != nullptr && dir[0] != '\0' ? dir : ESPHOME_SNAPSHOT_DIR;
}

/// Store a value in as many bytes, least significant first, and step the pointer past it.
/// BMP is a little endian format whatever the machine writing it uses.
void put_le(uint8_t *&dest, uint32_t value, size_t bytes) {
  for (size_t i = 0; i != bytes; i++)
    *dest++ = static_cast<uint8_t>(value >> (8 * i));
}

/// The number of bytes one row of `width` pixels takes up in a picture buffer. Rows are padded out
/// to a multiple of four bytes, which is also what a BMP file wants.
size_t padded_row_size(int width) { return (static_cast<size_t>(width) * 3 + 3) & ~size_t{3}; }

/// Write pixels out as a 24 bit BMP. The rows given start with the topmost and are `row_stride`
/// bytes apart, which must leave room for a whole padded row; a BMP holds its rows the other way
/// up, so they go out last first.
bool write_bmp(FILE *file, const uint8_t *pixels, int width, int height, size_t row_stride) {
  const size_t row_size = padded_row_size(width);
  const size_t pixel_bytes = row_size * height;

  uint8_t header[BMP_HEADER_SIZE];
  uint8_t *pos = header;
  *pos++ = 'B';
  *pos++ = 'M';
  put_le(pos, static_cast<uint32_t>(BMP_HEADER_SIZE + pixel_bytes), 4);
  put_le(pos, 0, 4);  // reserved
  put_le(pos, BMP_HEADER_SIZE, 4);
  put_le(pos, BMP_INFO_HEADER_SIZE, 4);
  put_le(pos, static_cast<uint32_t>(width), 4);
  put_le(pos, static_cast<uint32_t>(height), 4);
  put_le(pos, 1, 2);  // one plane
  put_le(pos, BMP_BITS_PER_PIXEL, 2);
  put_le(pos, 0, 4);  // not compressed
  put_le(pos, static_cast<uint32_t>(pixel_bytes), 4);
  put_le(pos, 0, 4);  // pixels per metre across, unspecified
  put_le(pos, 0, 4);  // pixels per metre down, unspecified
  put_le(pos, 0, 4);  // no palette
  put_le(pos, 0, 4);  // so no palette entry matters more than another

  if (fwrite(header, 1, sizeof(header), file) != sizeof(header))
    return false;
  for (int y = height - 1; y >= 0; y--) {
    if (fwrite(pixels + static_cast<size_t>(y) * row_stride, 1, row_size, file) != row_size)
      return false;
  }
  return true;
}

/// Reserve a name in the snapshot directory and open a new file under it.
/// With `exact` set the given name is the only one tried; otherwise a number is added on
/// collision. Returns nullptr, having logged why, if there is no file. `path` is where it is.
FILE *create_snapshot_file(const std::string &name, bool exact, std::string &path) {
  const std::string dir = snapshot_dir();
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  if (ec) {
    ESP_LOGE(TAG, "Could not create snapshot directory %s: %s", dir.c_str(), ec.message().c_str());
    return nullptr;
  }

  // O_EXCL guarantees we never write over a file that is already there.
  int fd = -1;
  for (unsigned attempt = 0; attempt < MAX_NAME_ATTEMPTS; attempt++) {
    path = dir + "/" + (attempt == 0 ? name : add_suffix(name, attempt));
    fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0644);
    if (fd >= 0)
      break;
    if (errno != EEXIST) {
      ESP_LOGE(TAG, "Could not create %s: %s", path.c_str(), strerror(errno));
      return nullptr;
    }
    if (exact) {
      // The caller asked for this exact name, so silently writing somewhere else would be worse
      // than failing - a test asserting on the path would pick up a stale file.
      ESP_LOGE(TAG, "Snapshot %s already exists, not overwriting", path.c_str());
      return nullptr;
    }
  }
  if (fd < 0) {
    ESP_LOGE(TAG, "Could not find an unused name for %s in %s", name.c_str(), dir.c_str());
    return nullptr;
  }

  FILE *file = fdopen(fd, "wb");
  if (file == nullptr) {
    ESP_LOGE(TAG, "Could not open %s: %s", path.c_str(), strerror(errno));
    ::close(fd);
    ::unlink(path.c_str());
  }
  return file;
}

/// Close a file that is meant to be finished. `ok` says whether everything written to it went well.
/// If not, or if closing shows that the data did not all get out, say why and remove the file: a
/// truncated one would block a retry under the same name. Returns true if the file is complete.
bool close_snapshot_file(FILE *file, const std::string &path, bool ok) {
  int saved_errno = ok ? 0 : errno;
  // Closing can fail in its own right - the last of the data is still on its way out.
  if (fclose(file) != 0) {
    if (ok)
      saved_errno = errno;
    ok = false;
  }
  if (!ok) {
    ESP_LOGE(TAG, "Could not write %s: %s", path.c_str(), strerror(saved_errno));
    ::unlink(path.c_str());
  }
  return ok;
}

/// Write pixels to a new BMP file. See create_snapshot_file() for `name` and `exact`.
bool write_snapshot_file(const uint8_t *pixels, int width, int height, size_t row_stride, const std::string &name,
                         bool exact) {
  std::string path;
  FILE *file = create_snapshot_file(name, exact, path);
  if (file == nullptr)
    return false;
  if (!close_snapshot_file(file, path, write_bmp(file, pixels, width, height, row_stride)))
    return false;
  ESP_LOGI(TAG, "Snapshot written to %s", path.c_str());
  return true;
}

/// Work out the file name for a request. A name of the caller's own is made safe and is the only
/// one tried (`exact`); otherwise one is made up from the prefix and the current time.
std::string choose_name(const char *filename, const char *prefix, const char *extension, bool &exact) {
  std::string name;
  exact = false;
  if (filename != nullptr) {
    bool name_changed = false;
    name = sanitise_filename(filename, extension, &name_changed);
    exact = !name.empty();
    if (name_changed) {
      ESP_LOGW(TAG, "Requested snapshot name '%s' is not an acceptable file name, using '%s' instead", filename,
               name.empty() ? LOG_STR_LITERAL("a name made from the time") : name.c_str());
    }
  }
  if (!name.empty())
    return name;

  struct timespec now {};
  if (clock_gettime(CLOCK_REALTIME, &now) != 0)
    now = {};
  struct tm tm_buf {};
  if (localtime_r(&now.tv_sec, &tm_buf) == nullptr)
    tm_buf = {};
  char stamp[32]{};
  // ::strftime to be sure of the one from <ctime>; display has an unrelated member of that name
  if (::strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", &tm_buf) == 0)
    snprintf(stamp, sizeof(stamp), "unknown-time");
  char buffer[MAX_NAME_LENGTH];
  int written = snprintf(buffer, sizeof(buffer), "%s-%s-%03ld%s", prefix, stamp, now.tv_nsec / 1000000, extension);
  if (written < 0 || static_cast<size_t>(written) >= sizeof(buffer)) {
    ESP_LOGW(TAG, "Could not build a timestamped snapshot name, using a fallback");
    snprintf(buffer, sizeof(buffer), "snapshot%s", extension);
  }
  return buffer;
}

}  // namespace

// helper function since ESP_LOGW is disallowed in a header file
/// An animation being written: the file, and where in the timetable it has got to.
struct Snapshot::Recording {
  Recording(FILE *file, std::string path, int width, int height, uint32_t frames, float frame_rate)
      : file(file),
        path(std::move(path)),
        gif(file, width, height),
        row_stride(padded_row_size(width)),
        pixels(std::make_unique<uint8_t[]>(this->row_stride * height)),
        frames(frames),
        frame_rate(frame_rate) {}
  ~Recording() {
    if (this->file != nullptr)
      fclose(this->file);
  }

  /// When the frame with this number is due, in milliseconds after the recording began.
  uint32_t due_ms(uint32_t frame) const {
    return static_cast<uint32_t>(std::lround(frame * 1000.0 / this->frame_rate));
  }
  /// How long to show the frame just taken, in hundredths of a second, as a GIF file counts time.
  /// Each frame's time is worked out from the start rather than by rounding its own length, so a
  /// rate that does not divide evenly into hundredths still averages out to what was asked for.
  unsigned delay_centiseconds() const {
    auto centiseconds_at = [this](uint32_t frame) { return std::lround(frame * 100.0 / this->frame_rate); };
    return static_cast<unsigned>(centiseconds_at(this->frames_done + 1) - centiseconds_at(this->frames_done));
  }

  FILE *file;
  std::string path;
  GifWriter gif;
  size_t row_stride;
  std::unique_ptr<uint8_t[]> pixels;
  uint32_t frames;
  float frame_rate;
  uint32_t frames_done{0};
  uint32_t start_ms{0};
};

Snapshot::Snapshot() = default;
Snapshot::~Snapshot() = default;

void Snapshot::log_action_failed() { ESP_LOGW(TAG, "snapshot.take did not write a file"); }

bool Snapshot::take_snapshot(const char *filename) {
  const int width = this->snapshot_width();
  const int height = this->snapshot_height();
  if (width <= 0 || height <= 0) {
    ESP_LOGE(TAG, "Snapshot requested but the display is %dx%d", width, height);
    return false;
  }

  bool exact;
  const std::string name = choose_name(filename, this->snapshot_prefix_, ".bmp", exact);

  // Rows are padded out to a multiple of four bytes, as the file wants them, so each one can be
  // written straight from the buffer. Zeroed on allocation, which is what the padding must be.
  const size_t row_stride = padded_row_size(width);
  auto pixels = std::make_unique<uint8_t[]>(row_stride * height);
  if (!this->capture_bgr(pixels.get(), row_stride))
    return false;
  return write_snapshot_file(pixels.get(), width, height, row_stride, name, exact);
}

bool Snapshot::take_animation(const char *filename, uint32_t frames, float frame_rate) {
  if (this->recording_ != nullptr) {
    ESP_LOGW(TAG, "Already recording %s, not starting another", this->recording_->path.c_str());
    return false;
  }
  const int width = this->snapshot_width();
  const int height = this->snapshot_height();
  if (width <= 0 || height <= 0 || width > GIF_MAX_SIZE || height > GIF_MAX_SIZE) {
    ESP_LOGE(TAG, "Animation requested but the display is %dx%d, and a GIF holds up to %d", width, height,
             GIF_MAX_SIZE);
    return false;
  }

  bool exact;
  const std::string name = choose_name(filename, this->snapshot_prefix_, ".gif", exact);
  std::string path;
  FILE *file = create_snapshot_file(name, exact, path);
  if (file == nullptr)
    return false;
  auto recording = std::make_unique<Recording>(file, path, width, height, frames, frame_rate);
  if (!recording->gif.write_header()) {
    recording->file = nullptr;
    close_snapshot_file(file, path, false);
    return false;
  }
  ESP_LOGI(TAG, "Recording %u frames at %.1f frames a second to %s", frames, frame_rate, path.c_str());
  this->recording_ = std::move(recording);
  this->recording_->start_ms = millis();
  return this->record_frame_();
}

bool Snapshot::record_frame_() {
  Recording &recording = *this->recording_;
  if (!this->capture_bgr(recording.pixels.get(), recording.row_stride)) {
    // capture_bgr() has said why. Leave no half-finished file behind.
    fclose(std::exchange(recording.file, nullptr));
    ::unlink(recording.path.c_str());
    this->recording_.reset();
    return false;
  }

  bool ok = recording.gif.write_frame(recording.pixels.get(), recording.row_stride, recording.delay_centiseconds());
  recording.frames_done++;
  const bool finished = recording.frames_done == recording.frames;
  if (ok && finished)
    ok = recording.gif.write_trailer();
  if (!ok || finished) {
    ok = close_snapshot_file(std::exchange(recording.file, nullptr), recording.path, ok);
    if (ok) {
      ESP_LOGI(TAG, "Animation written to %s", recording.path.c_str());
    }
    this->recording_.reset();
    return ok;
  }

  // Aim for the time each frame is due, not a fixed gap after the last, so delays do not add up.
  const int32_t wait = static_cast<int32_t>(recording.start_ms + recording.due_ms(recording.frames_done) - millis());
  App.scheduler.set_timeout(this, wait > 0 ? static_cast<uint32_t>(wait) : 0, [this]() { this->record_frame_(); });
  return true;
}

}  // namespace esphome::snapshot
#endif
