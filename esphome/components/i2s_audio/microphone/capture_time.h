#pragma once

#include "esphome/components/audio/audio.h"

#include <algorithm>
#include <cstdint>

namespace esphome::i2s_audio {

/// @brief Returns when the first frame of a read was captured, counted back from the newest DMA buffer, or 0 when
/// the time is unknown
///
/// All byte and buffer counts are since the channel was enabled and wrap at 2^32.
/// @param bytes_received Bytes the interrupt has received, taken after the read
/// @param drops Buffers dropped from the driver's full queue, taken with bytes_received
/// @param drops_before_read Buffers dropped, taken before the read
/// @param receive_time_us When the newest buffer was received, taken with bytes_received, or 0 if none has been
/// @param bytes_read_before Bytes read before this read
/// @param bytes_read Bytes this read returned
/// @param dma_desc_num Number of DMA buffers
/// @param dma_buffer_bytes Size of each DMA buffer
/// @param stream_info Format of the audio read
inline int64_t dma_capture_time_us(uint32_t bytes_received, uint32_t drops, uint32_t drops_before_read,
                                   int64_t receive_time_us, uint32_t bytes_read_before, uint32_t bytes_read,
                                   uint32_t dma_desc_num, uint32_t dma_buffer_bytes,
                                   const audio::AudioStreamInfo &stream_info) {
  // A drop between the two snapshots may have removed buffers newer than this read's first frame, which the count
  // back would skip, so the capture time is unknown. It is also unknown if no buffer was recorded because the
  // callbacks failed to register.
  if ((drops != drops_before_read) || (receive_time_us == 0)) {
    return 0;
  }
  // Count back from the newest DMA buffer to this read's first frame. At most dma_desc_num buffers follow the read: a
  // full queue plus one the driver has recorded but not queued yet.
  const uint32_t bytes_queued = bytes_received - drops * dma_buffer_bytes;
  const uint32_t bytes_after_first =
      std::min<uint32_t>(bytes_queued - bytes_read_before, bytes_read + dma_desc_num * dma_buffer_bytes);
  const uint32_t frames_after_first = stream_info.bytes_to_frames(bytes_after_first);
  return receive_time_us - static_cast<int64_t>(frames_after_first) * 1000000 / stream_info.get_sample_rate();
}

}  // namespace esphome::i2s_audio
