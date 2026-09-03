#pragma once

#include "esphome/core/defines.h"

#ifdef AUDIO_FILE_MAX_FILES

#include "esphome/components/audio/audio.h"
#include "esphome/core/helpers.h"

namespace esphome::audio_file {

struct NamedAudioFile {
  audio::AudioFile *file;
  const char *file_id;
};

inline StaticVector<NamedAudioFile, AUDIO_FILE_MAX_FILES>
    named_audio_files;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

inline void add_named_audio_file(audio::AudioFile *file, const char *file_id) {
  named_audio_files.push_back({file, file_id});
}

inline const StaticVector<NamedAudioFile, AUDIO_FILE_MAX_FILES> &get_named_audio_files() { return named_audio_files; }

}  // namespace esphome::audio_file

#endif  // AUDIO_FILE_MAX_FILES
