#pragma once

#include "esphome/core/defines.h"

#ifdef USE_ESP32

#include <cstddef>
#include <cstdint>

namespace esphome::http_request {

// Inter-task communication overview
//
// Three components coordinate via FreeRTOS event bits and a shared ring buffer:
//
//   Main Loop (http_request_media_source.cpp)
//     - Orchestrates lifecycle: creates/destroys tasks, monitors state
//     - Sets REQUEST_START, COMMAND_STOP, COMMAND_PAUSE
//     - Reads DECODER_* and READER_* bits to track state
//
//   Read Task (reader_task.cpp)              Decode Task (decoder_task.cpp)
//     - Fetches audio over HTTP               - Decodes raw audio stream
//     - Writes raw bytes to ring buffer       - Reads from ring buffer
//     - Signals READER_READY, READER_FINISHED - Signals DECODER_RUNNING, etc.
//
// Startup sequence:
//   1. play_uri() sets REQUEST_START
//   2. loop() creates both tasks
//   3. Read task opens HTTP connection, creates ring buffer, sets READER_READY
//   4. Decode task waits for READER_READY, acquires ring buffer, sets DECODER_RINGBUF_ACQUIRED
//   5. Read task waits for DECODER_RINGBUF_ACQUIRED before releasing its ring buffer guard
//   6. Both tasks run in parallel: read streams to ring buffer, decode consumes from it
//
// Shutdown sequence:
//   1. COMMAND_STOP set (by main loop or timeout)
//   2. Both tasks exit their loops and set *_STOPPED
//   3. loop() detects both stopped, destroys tasks, returns to IDLE
//
// Ring buffer lifetime:
//   - Read task creates it as shared_ptr, stores weak_ptr in HTTPRequestMediaSource
//   - Decode task locks the weak_ptr to get its own shared_ptr
//   - DECODER_RINGBUF_ACQUIRED synchronizes the handoff
//   - Ring buffer is freed when both shared_ptrs are released

// FreeRTOS task entry points (defined in reader_task.cpp and decoder_task.cpp)
void read_task(void *params);
void decode_task(void *params);

inline constexpr char TAG[] = "http_media_source";

// Task stack sizes
inline constexpr uint32_t READ_TASK_STACK_SIZE = 5 * 1024;
#if defined(USE_AUDIO_OPUS_SUPPORT)  // Opus requires more task stack
inline constexpr uint32_t DECODE_TASK_STACK_SIZE = 5 * 1024;
#else
inline constexpr uint32_t DECODE_TASK_STACK_SIZE = 3 * 1024;
#endif

// Buffer and timeout constants
inline constexpr size_t DEFAULT_TRANSFER_BUFFER_SIZE = 24 * 1024;
inline constexpr uint32_t READ_WRITE_TIMEOUT_MS = 20;
inline constexpr uint32_t CONNECTION_TIMEOUT_MS = 30000;  // 30 second timeout for no data
inline constexpr uint8_t MAX_CONNECTION_ATTEMPTS = 6;

enum EventGroupBits : uint32_t {
  // Requests to start playback (set by play_uri, handled by loop)
  REQUEST_START = (1 << 0),
  // Commands from main loop to tasks
  COMMAND_STOP = (1 << 1),
  COMMAND_PAUSE = (1 << 2),
  // Inter-task coordination
  READER_READY = (1 << 3),
  READER_FINISHED = (1 << 4),
  DECODER_RINGBUF_ACQUIRED = (1 << 5),
  // Read task lifecycle (read task -> loop)
  READER_STOPPED = (1 << 6),
  READER_ERROR = (1 << 7),
  // Decode task lifecycle (decode task -> loop, mirrors)
  DECODER_STARTING = (1 << 8),
  DECODER_RUNNING = (1 << 9),
  DECODER_PAUSED = (1 << 10),
  DECODER_STOPPING = (1 << 11),
  DECODER_STOPPED = (1 << 12),
  DECODER_ERROR = (1 << 13),
  ALL_BITS = 0x00FFFFFF,  // All valid FreeRTOS event group bits
};

}  // namespace esphome::http_request

#endif  // USE_ESP32
