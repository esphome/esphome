#include "fake_sample_buffer.h"

namespace esphome::audio_analyzer {

bool testing::FakeSampleBuffer::fail_next_create = false;

// On the device, sample_buffer.cpp builds it on a ring_buffer::RingBuffer, which needs FreeRTOS
std::unique_ptr<SampleBuffer> SampleBuffer::create(size_t size) {
  if (testing::FakeSampleBuffer::fail_next_create) {
    testing::FakeSampleBuffer::fail_next_create = false;
    return nullptr;
  }
  return std::make_unique<testing::FakeSampleBuffer>(size);
}

}  // namespace esphome::audio_analyzer
