#pragma once

#include <cstddef>
#include <cstdint>

namespace esphome::camera_video {

static constexpr uint8_t H264_NAL_TYPE_MASK = 0x1F;
static constexpr uint8_t H264_NAL_TYPE_SPS = 7;
static constexpr uint8_t H264_NAL_TYPE_PPS = 8;
static constexpr size_t H264_ANNEXB_SHORT_START_CODE_BYTES = 3;
static constexpr size_t H264_ANNEXB_LONG_START_CODE_BYTES = 4;
static constexpr size_t H264_ANNEXB_MIN_START_CODE_BYTES = H264_ANNEXB_SHORT_START_CODE_BYTES;

struct H264AnnexBNal {
  const uint8_t *data{nullptr};
  size_t len{0};

  uint8_t type() const { return this->len == 0 || this->data == nullptr ? 0 : this->data[0] & H264_NAL_TYPE_MASK; }
};

inline const uint8_t *find_annexb_start_code(const uint8_t *cursor, const uint8_t *end) {
  if (cursor == nullptr || end == nullptr || cursor > end) {
    return end;
  }
  while (static_cast<size_t>(end - cursor) >= H264_ANNEXB_MIN_START_CODE_BYTES) {
    if (cursor[0] == 0 && cursor[1] == 0 && cursor[2] == 1) {
      return cursor;
    }
    if (static_cast<size_t>(end - cursor) >= H264_ANNEXB_LONG_START_CODE_BYTES && cursor[0] == 0 && cursor[1] == 0 &&
        cursor[2] == 0 && cursor[3] == 1) {
      return cursor;
    }
    ++cursor;
  }
  return end;
}

inline const uint8_t *skip_annexb_start_code(const uint8_t *start_code, const uint8_t *end) {
  if (start_code == nullptr || end == nullptr || start_code > end) {
    return end;
  }
  const size_t remaining = static_cast<size_t>(end - start_code);
  if (remaining >= H264_ANNEXB_LONG_START_CODE_BYTES && start_code[0] == 0 && start_code[1] == 0 &&
      start_code[2] == 0 && start_code[3] == 1) {
    return start_code + H264_ANNEXB_LONG_START_CODE_BYTES;
  }
  if (remaining >= H264_ANNEXB_SHORT_START_CODE_BYTES && start_code[0] == 0 && start_code[1] == 0 &&
      start_code[2] == 1) {
    return start_code + H264_ANNEXB_SHORT_START_CODE_BYTES;
  }
  return end;
}

inline const uint8_t *trim_annexb_trailing_zero_bytes(const uint8_t *begin, const uint8_t *end) {
  while (end > begin && end[-1] == 0) {
    --end;
  }
  return end;
}

/** Iterates non-empty NAL units from an Annex-B byte stream.
 *
 * Consecutive start codes and trailing_zero_8bits are skipped. On return,
 * cursor always advances or is set to end.
 */
inline bool next_annexb_nal(const uint8_t **cursor, const uint8_t *end, H264AnnexBNal *nal) {
  if (cursor == nullptr || *cursor == nullptr || end == nullptr || nal == nullptr || *cursor > end) {
    return false;
  }

  const uint8_t *search = *cursor;
  *nal = {};
  while (search < end) {
    const uint8_t *start_code = find_annexb_start_code(search, end);
    if (start_code == end) {
      *cursor = end;
      return false;
    }

    const uint8_t *nal_start = skip_annexb_start_code(start_code, end);
    if (nal_start >= end) {
      *cursor = end;
      return false;
    }
    const uint8_t *next_start_code = find_annexb_start_code(nal_start, end);
    const uint8_t *nal_end = trim_annexb_trailing_zero_bytes(nal_start, next_start_code);
    *cursor = next_start_code;
    if (nal_start < nal_end) {
      *nal = {nal_start, static_cast<size_t>(nal_end - nal_start)};
      return true;
    }

    // Empty NAL: the next start code begins the following NAL. Search from
    // that code rather than skipping it, otherwise its payload is lost.
    search = next_start_code;
    *cursor = search;
  }

  *cursor = end;
  return false;
}

}  // namespace esphome::camera_video
