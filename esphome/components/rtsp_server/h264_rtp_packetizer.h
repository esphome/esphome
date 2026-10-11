#pragma once

#include "esphome/components/camera_video/h264_annexb.h"

#include <cstddef>
#include <cstdint>

namespace esphome::rtsp_server {

static constexpr uint8_t H264_FU_A_NAL_TYPE = 28;
static constexpr uint8_t H264_FU_A_HEADER_BYTES = 2;
static constexpr uint8_t H264_FU_START_BIT = 0x80;
static constexpr uint8_t H264_FU_END_BIT = 0x40;
static constexpr size_t H264_NAL_HEADER_BYTES = 1;

struct H264RtpFragment {
  uint8_t prefix[H264_FU_A_HEADER_BYTES]{};
  size_t prefix_length{0};
  const uint8_t *payload{nullptr};
  size_t payload_length{0};
  bool marker{false};
};

template<typename Callback>
bool packetize_h264_nal(const uint8_t *nal, size_t nal_length, size_t maximum_payload, bool marker,
                        Callback &&callback) {
  if (nal == nullptr || nal_length == 0 || maximum_payload == 0) {
    return false;
  }
  auto &&emit = callback;
  if (nal_length <= maximum_payload) {
    H264RtpFragment output;
    output.payload = nal;
    output.payload_length = nal_length;
    output.marker = marker;
    return emit(output);
  }
  if (nal_length <= H264_NAL_HEADER_BYTES || maximum_payload <= H264_FU_A_HEADER_BYTES) {
    return false;
  }

  const uint8_t fu_indicator = static_cast<uint8_t>((nal[0] & 0xE0U) | H264_FU_A_NAL_TYPE);
  const uint8_t nal_type = nal[0] & camera_video::H264_NAL_TYPE_MASK;
  const size_t maximum_fragment = maximum_payload - H264_FU_A_HEADER_BYTES;
  const uint8_t *fragment = nal + H264_NAL_HEADER_BYTES;
  size_t remaining = nal_length - H264_NAL_HEADER_BYTES;
  bool first = true;

  while (remaining != 0) {
    const size_t fragment_length = remaining < maximum_fragment ? remaining : maximum_fragment;
    const bool last = fragment_length == remaining;
    H264RtpFragment output;
    output.prefix[0] = fu_indicator;
    output.prefix[1] =
        static_cast<uint8_t>(nal_type | (first ? H264_FU_START_BIT : 0U) | (last ? H264_FU_END_BIT : 0U));
    output.prefix_length = H264_FU_A_HEADER_BYTES;
    output.payload = fragment;
    output.payload_length = fragment_length;
    output.marker = marker && last;
    if (!emit(output)) {
      return false;
    }
    fragment += fragment_length;
    remaining -= fragment_length;
    first = false;
  }
  return true;
}

}  // namespace esphome::rtsp_server
