#include "sound_level.h"

#ifdef USE_ESP32

#include "esphome/core/log.h"

#include <cmath>
#include <cstdint>

namespace esphome::sound_level {

static const char *const TAG = "sound_level";

static const uint32_t MAX_FILL_DURATION_MS = 30;
static const uint32_t RING_BUFFER_DURATION_MS = 120;

// Square INT16_MIN since INT16_MIN^2 > INT16_MAX^2
static const double MAX_SAMPLE_SQUARED_DENOMINATOR = INT16_MIN * INT16_MIN;

void SoundLevelComponent::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Sound Level Component:\n"
                "  Measurement Duration: %" PRIu32 " ms",
                measurement_duration_ms_);
  LOG_SENSOR("  ", "Peak:", this->peak_sensor_);

  LOG_SENSOR("  ", "RMS:", this->rms_sensor_);
}

void SoundLevelComponent::setup() {
  this->microphone_source_->add_data_callback([this](const std::vector<uint8_t> &data) {
    std::shared_ptr<ring_buffer::RingBuffer> temp_ring_buffer = this->ring_buffer_.lock();
    if (temp_ring_buffer != nullptr) {
      temp_ring_buffer->write((void *) data.data(), data.size());
    }
  });

  if (!this->microphone_source_->is_passive()) {
    // Automatically start the microphone if not in passive mode
    this->microphone_source_->start();
  }
}

void SoundLevelComponent::loop() {
  if ((this->peak_sensor_ == nullptr) && (this->rms_sensor_ == nullptr)) {
    // No sensors configured, nothing to do
    return;
  }

  if (this->microphone_source_->is_running() && !this->status_has_error()) {
    // Allocate buffers
    if (this->start_()) {
      this->status_clear_warning();
    }
  } else {
    if (!this->status_has_warning()) {
      this->status_set_warning(LOG_STR("Microphone isn't running, can't compute statistics"));

      // Deallocate buffers, if necessary
      this->stop_();

      // Reset sensor outputs
      if (this->peak_sensor_ != nullptr) {
        this->peak_sensor_->publish_state(NAN);
      }
      if (this->rms_sensor_ != nullptr) {
        this->rms_sensor_->publish_state(NAN);
      }

      // Reset accumulators
      this->squared_peak_ = 0;
      this->squared_samples_sum_ = 0;
      this->sample_count_ = 0;
    }

    return;
  }

  if (this->status_has_error()) {
    return;
  }

  // Expose a chunk of the ring buffer's internal storage - don't block to avoid slowing the main loop.
  // pre_shift is ignored by RingBufferAudioSource (no intermediate transfer buffer to compact).
  this->audio_source_->fill(0, false);

  if (this->audio_source_->available() == 0) {
    // No new audio available for processing
    return;
  }

  const uint32_t samples_in_window =
      this->microphone_source_->get_audio_stream_info().ms_to_samples(this->measurement_duration_ms_);
  const uint32_t samples_available_to_process =
      this->microphone_source_->get_audio_stream_info().bytes_to_samples(this->audio_source_->available());
  const uint32_t samples_to_process = std::min(samples_in_window - this->sample_count_, samples_available_to_process);

  // MicrophoneSource always provides int16 samples due to Python codegen settings
  const int16_t *audio_data = reinterpret_cast<const int16_t *>(this->audio_source_->data());

  // Process all the new audio samples
  for (uint32_t i = 0; i < samples_to_process; ++i) {
    // Squaring int16 samples won't overflow an int32
    int32_t squared_sample = static_cast<int32_t>(audio_data[i]) * static_cast<int32_t>(audio_data[i]);

    if (this->peak_sensor_ != nullptr) {
      this->squared_peak_ = std::max(this->squared_peak_, squared_sample);
    }

    if (this->rms_sensor_ != nullptr) {
      // Squared sum is an uint64 type - at max levels, an uint32 type would overflow after ~8 samples
      this->squared_samples_sum_ += squared_sample;
    }

    ++this->sample_count_;
  }

  // Remove the processed samples from ``audio_source_``
  this->audio_source_->consume(this->microphone_source_->get_audio_stream_info().samples_to_bytes(samples_to_process));

  if (this->sample_count_ == samples_in_window) {
    // Processed enough samples for the measurement window, compute and publish the sensor values
    if (this->peak_sensor_ != nullptr) {
      const float peak_db = 10.0f * log10(static_cast<float>(this->squared_peak_) / MAX_SAMPLE_SQUARED_DENOMINATOR);
      this->peak_sensor_->publish_state(peak_db);

      this->squared_peak_ = 0;  // reset accumulator
    }

    if (this->rms_sensor_ != nullptr) {
      // Calculations are done with doubles instead of floats - floats lose precision for even modest window durations
      const double rms_db = 10.0 * log10((this->squared_samples_sum_ / MAX_SAMPLE_SQUARED_DENOMINATOR) /
                                         static_cast<double>(samples_in_window));
      this->rms_sensor_->publish_state(rms_db);

      this->squared_samples_sum_ = 0;  // reset accumulator
    }

    this->sample_count_ = 0;  // reset counter
  }
}

void SoundLevelComponent::start() {
  if (this->microphone_source_->is_passive()) {
    ESP_LOGW(TAG, "Can't start the microphone in passive mode");
    return;
  }
  this->microphone_source_->start();
}

void SoundLevelComponent::stop() {
  if (this->microphone_source_->is_passive()) {
    ESP_LOGW(TAG, "Can't stop microphone in passive mode");
    return;
  }
  this->microphone_source_->stop();
}

bool SoundLevelComponent::start_() {
  if (this->audio_source_ != nullptr) {
    return true;
  }

  const auto &stream_info = this->microphone_source_->get_audio_stream_info();
  const size_t bytes_per_frame = stream_info.frames_to_bytes(1);

  // Allocate a ring buffer for the microphone callback to write into. Round the size down to a multiple
  // of bytes_per_frame so the wrap boundary stays frame-aligned and avoids unnecessary single-frame splices.
  this->ring_buffer_.reset();  // Reset pointer to any previous ring buffer allocation
  const size_t ring_buffer_size =
      (stream_info.ms_to_bytes(RING_BUFFER_DURATION_MS) / bytes_per_frame) * bytes_per_frame;
  std::shared_ptr<ring_buffer::RingBuffer> temp_ring_buffer = ring_buffer::RingBuffer::create(ring_buffer_size);
  if (temp_ring_buffer == nullptr) {
    this->status_momentary_error("ring_buffer", 15000);
    return false;
  }

  // Zero-copy source that reads directly from the ring buffer's internal storage. Frame-aligned reads
  // ensure multi-channel frames are never split across the ring buffer's wrap boundary.
  this->audio_source_ = audio::RingBufferAudioSource::create(
      temp_ring_buffer, stream_info.ms_to_bytes(MAX_FILL_DURATION_MS), static_cast<uint8_t>(bytes_per_frame));
  if (this->audio_source_ == nullptr) {
    this->status_momentary_error("audio_source", 15000);
    return false;
  }
  this->ring_buffer_ = temp_ring_buffer;

  this->status_clear_error();
  return true;
}

void SoundLevelComponent::stop_() { this->audio_source_.reset(); }

}  // namespace esphome::sound_level

#endif
