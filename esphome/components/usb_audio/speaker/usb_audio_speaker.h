#pragma once

#include "esphome/core/defines.h"

#ifdef USE_USB_AUDIO

#include "esphome/components/usb_audio/usb_audio.h"

#include "esphome/components/audio/audio.h"
#include "esphome/components/speaker/speaker.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#include <atomic>

namespace esphome::usb_audio {

class USBAudioSpeaker : public speaker::Speaker,
                        public Component,
                        public Parented<USBAudioComponent>,
                        public USBAudioStream {
 public:
  USBAudioSpeaker(const audio::AudioStreamInfo &stream_info, uint32_t buffer_size, uint32_t write_timeout_ms)
      : USBAudioStream(buffer_size), usb_stream_info_(stream_info), write_timeout_ms_(write_timeout_ms) {
    this->audio_stream_info_ = stream_info;
  }

  void setup() override;
  void dump_config() override;
  void loop() override;

  // Safe from any task; loop() acts on the request.
  void start() override;
  void stop() override;
  void finish() override;

  size_t play(const uint8_t *data, size_t length, TickType_t ticks_to_wait) override;
  size_t play(const uint8_t *data, size_t length) override;
  bool has_buffered_data() const override;

  void set_volume(float volume) override;
  void set_mute_state(bool mute_state) override;

  audio::AudioStreamInfo get_usb_stream_info() override { return this->usb_stream_info_; }
  void on_stream_state(StreamEvent event) override;
  bool stream_idle() const override { return this->writes_in_flight_ == 0; }
  void on_transfer_taken() override;

 protected:
  static constexpr uint8_t COMMAND_START = 1 << 0;
  static constexpr uint8_t COMMAND_STOP = 1 << 1;
  static constexpr uint8_t COMMAND_FINISH = 1 << 2;
  void request_(uint8_t set, uint8_t clear);
  void deliver_reports_();
  void begin_finishing_();
  void stop_();
  size_t play_internal_(const uint8_t *data, size_t length, uint32_t budget_ms);
  /// Write silence until a whole USB transfer is padded, so the driver sends a final partial transfer.
  void pad_with_silence_();
  /// Write to the driver, counting the bytes in ring_bytes_ (and in ring_padding_ for silence padding).
  esp_err_t write_(const uint8_t *data, size_t length, uint32_t timeout_ms, bool padding);
  /// Bytes that can be written without filling the driver's ring buffer past buffer_size_.
  size_t ring_room_() const;
  bool has_buffered_data_(int64_t now_us) const;

  // The configured format the USB stream runs with. Callers such as the media pipeline set audio_stream_info_ for
  // each item, so play() checks it against this instead of reopening the stream with another format.
  const audio::AudioStreamInfo usb_stream_info_;
  uint32_t write_timeout_ms_;
  // Frame-aligned size of each driver write, set in setup().
  uint32_t write_chunk_{0};
  // The driver's packet sizing, mirrored so on_transfer_taken() knows how much each transfer takes: whole frames per
  // 1 ms packet, plus a frame whenever the remainder of sample_rate / 1000 adds up. Set in setup().
  uint32_t frame_bytes_{0};
  uint32_t packet_bytes_{0};
  uint32_t packet_frac_{0};
  // Bytes in the largest USB transfer, how long one transfer plays and how long all the driver keeps in flight play,
  // and the batch of frames reported at a time; set in setup().
  uint32_t transfer_bytes_{0};
  uint32_t transfer_us_{0};
  uint32_t in_flight_us_{0};
  uint32_t report_frames_{0};
  // Silence still to write while finishing.
  uint32_t pad_remaining_{0};
  // Bytes in the driver's ring buffer: added before each write, since the driver can take a transfer inside it, and
  // taken off for each transfer it takes. ring_padding_ is the silence padding at its end.
  std::atomic<uint32_t> ring_bytes_{0};
  std::atomic<uint32_t> ring_padding_{0};
  // Low 32 bits of esp_timer_get_time() when the driver last took a transfer.
  std::atomic<uint32_t> last_taken_us_{0};
  // on_transfer_taken() runs on the USB driver's task, which must not block, and inside writes, so its state is
  // guarded by a spinlock and the reports it collects are delivered from loop(). Reset while the stream is stopped.
  portMUX_TYPE taken_lock_ = portMUX_INITIALIZER_UNLOCKED;
  uint32_t packet_frac_accum_{0};
  uint32_t unreported_frames_{0};
  uint32_t report_frames_pending_{0};
  int64_t report_finish_us_{0};
  // When the last transfer taken finishes playing; zero until the first one after a start.
  int64_t last_finish_us_{0};
  std::atomic<uint8_t> commands_{0};
  // play() calls and silence padding that may be using the device handle; the hub closes a lost device only at zero.
  std::atomic<uint8_t> writes_in_flight_{0};
  // Set while play() rejects audio in a format other than usb_stream_info_, so the error is logged once.
  std::atomic<bool> format_mismatch_{false};
  bool finishing_{false};
  // finish() arrived while the stream was still starting.
  bool finish_after_start_{false};
};

}  // namespace esphome::usb_audio

#endif
