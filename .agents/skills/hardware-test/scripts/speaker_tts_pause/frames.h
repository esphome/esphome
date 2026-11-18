#pragma once
// Played-frame counters for tts-common.yaml, written from the resampler speakers' audio output callbacks.
#include <atomic>
#include <cstdint>

namespace hwt_frames {
inline std::atomic<uint32_t> ann{0};
inline std::atomic<uint32_t> med{0};
}  // namespace hwt_frames
