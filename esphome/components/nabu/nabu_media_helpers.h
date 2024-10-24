#pragma once

#ifdef USE_ESP_IDF

#include <cstdint>
#include <cstddef>

namespace esphome {
namespace nabu {

enum class MediaFileType : uint8_t {
  NONE = 0,
  WAV,
  MP3,
  FLAC,
};
const char *media_player_file_type_to_string(MediaFileType file_type);

struct MediaFile {
  const uint8_t *data;
  size_t length;
  MediaFileType file_type;
};

}  // namespace nabu
}  // namespace esphome

#endif
