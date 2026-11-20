#include "sample_buffer.h"

#ifdef USE_ESP32

#include "esphome/components/ring_buffer/ring_buffer.h"

namespace esphome::audio_analyzer {

namespace {

class RingSampleBuffer final : public SampleBuffer {
 public:
  explicit RingSampleBuffer(std::unique_ptr<ring_buffer::RingBuffer> ring_buffer)
      : ring_buffer_(std::move(ring_buffer)) {}

  bool write(const void *data, size_t length) override {
    return this->ring_buffer_->write_without_replacement(data, length, 0, false) == length;
  }
  size_t read(void *data, size_t length) override { return this->ring_buffer_->read(data, length, 0); }
  size_t available() const override { return this->ring_buffer_->available(); }
  void reset() override { this->ring_buffer_->reset(); }

 protected:
  std::unique_ptr<ring_buffer::RingBuffer> ring_buffer_;
};

}  // namespace

std::unique_ptr<SampleBuffer> SampleBuffer::create(size_t size) {
  std::unique_ptr<ring_buffer::RingBuffer> ring_buffer = ring_buffer::RingBuffer::create(size);
  if (ring_buffer == nullptr) {
    return nullptr;
  }
  return std::make_unique<RingSampleBuffer>(std::move(ring_buffer));
}

}  // namespace esphome::audio_analyzer

#endif  // USE_ESP32
