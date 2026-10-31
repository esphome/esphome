#pragma once

#if defined(USE_ESP32) && defined(USE_ESP_IDF) && defined(USE_I2S_AEC_MICROPHONE)

#ifdef USE_I2S_AEC_MICROPHONE_DSP_DECIMATOR
#include "esphome/components/audio/decimator.h"
#endif
#include "esphome/components/i2s_audio/i2s_audio.h"
#include "esphome/components/microphone/microphone.h"
#include <resampler.h>  // esp-audio-libs
#include "esphome/core/component.h"

#include <atomic>
#include <array>
#include <memory>
#include <esp_aec.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <vector>

namespace esphome::i2s_aec_microphone {

class I2SAECMicrophone final : public Component, public microphone::Microphone {
 public:
  void set_source(microphone::Microphone *source) { this->source_ = source; }
  void set_bus(i2s_audio::I2SAudioComponent *bus) { this->bus_ = bus; }
  void set_reference_sample_rate(uint32_t sample_rate) { this->reference_sample_rate_ = sample_rate; }
#ifdef USE_I2S_AEC_MICROPHONE_DSP_DECIMATOR
  void set_fast_reference_decimation(bool enabled) { this->fast_reference_decimation_ = enabled; }
#endif
  void set_task_core(int core) { this->task_core_ = core; }
  void set_filter_length(int length) { this->filter_length_ = length; }
  void set_reference_gain_hundredths(int gain) { this->reference_gain_hundredths_ = gain; }

  float get_setup_priority() const override { return setup_priority::DATA + 1.0f; }
  void setup() override;
  void loop() override;
  void dump_config() override;
  void start() override;
  void stop() override;
  uint32_t mic_drops() const { return this->mic_drops_.load(std::memory_order_relaxed); }
  uint32_t tx_drops() const { return this->tx_drops_.load(std::memory_order_relaxed); }

  static constexpr uint32_t RATE = 16000;
  static constexpr size_t FRAME = 512;
  static constexpr size_t RING = 4096;
  static constexpr size_t TX_QUEUE = 8;
  static constexpr size_t MAX_TX_FRAMES = 480;
  static constexpr size_t FAST_REFERENCE_RATIO = 3;

  struct TxBlock {
    uint32_t first;
    uint16_t count;
    int16_t mono[MAX_TX_FRAMES];
  };

 protected:
  static void on_tx_dma(const uint8_t *data, size_t bytes, void *context);
  static void task_entry(void *context);
  void on_microphone_(const std::vector<uint8_t> &data);
  void run_();
  void drain_tx_();
  void setup_reference_converter_();
  size_t convert_reference_(const TxBlock &block, const uint8_t *&samples);
  bool calibrate_reference_(uint32_t mic_write);

  microphone::Microphone *source_{nullptr};
  i2s_audio::I2SAudioComponent *bus_{nullptr};
  std::atomic<aec_handle_t *> aec_{nullptr};
  int task_core_{tskNO_AFFINITY};
  int filter_length_{2};
  int reference_gain_hundredths_{100};
  int16_t *mic_ring_{nullptr};
  int16_t *ref_ring_{nullptr};
  std::vector<uint8_t> output_;
  std::unique_ptr<esp_audio_libs::resampler::Resampler> reference_resampler_;
  std::vector<uint8_t> reference_output_;
#ifdef USE_I2S_AEC_MICROPHONE_DSP_DECIMATOR
  audio::DspDecimator reference_decimator_;
  std::array<int16_t, MAX_TX_FRAMES / FAST_REFERENCE_RATIO> decimated_reference_{};
  bool fast_reference_decimation_{false};
#endif
  uint32_t reference_sample_rate_{48000};
  std::atomic<uint8_t> listeners_{0};
  std::atomic<uint32_t> generation_{0};
  std::atomic<uint32_t> mic_written_{0};
  std::atomic<uint32_t> tx_total_{0};
  std::atomic<uint32_t> tx_produced_{0};
  std::atomic<uint32_t> tx_consumed_{0};
  std::atomic<uint32_t> ref_ready_{0};
  std::atomic<uint32_t> tx_last_us_{0};
  std::atomic<uint32_t> mic_ref_base_{0};
  std::atomic<int32_t> observed_base_{0};
  std::atomic<bool> synchronized_{false};
  std::atomic<uint32_t> tx_drops_{0};
  std::atomic<uint32_t> mic_drops_{0};
  bool aec_attempted_{false};
  TaskHandle_t task_{nullptr};
  uint32_t mic_read_{0};
  uint32_t last_calibration_mic_{0};
  uint32_t silent_frames_{0};
  bool tx_was_active_{false};
  int64_t observed_base_filter_q5_{0};
#ifdef USE_I2S_AEC_MICROPHONE_DIAGNOSTICS
  int32_t probe_lag_initial_{0};
  bool probe_lag_initialized_{false};
  uint32_t probe_lag_revision_{0};
#endif

 private:
  static TxBlock tx_queue[TX_QUEUE];
  static StackType_t task_stack[4096];
  static StaticTask_t task_buffer;
  static int16_t mic_frame[FRAME];
  static int16_t ref_frame[FRAME];
  static int16_t out_frame[FRAME];
};

}  // namespace esphome::i2s_aec_microphone

#endif
