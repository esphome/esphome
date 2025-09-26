#pragma once

#include "microphone.h"

#include "esphome/components/audio/audio.h"

#include <bitset>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

namespace esphome {
namespace microphone {

static const int32_t MAX_GAIN_FACTOR = 64;

class MicrophoneSource {
  /*
   * @brief Helper class that handles converting raw microphone data to a requested format.
   * Components requesting microphone audio should register a callback through this class instead of registering a
   * callback directly with the microphone if a particular format is required.
   *
   * Raw microphone data may have a different number of bits per sample and number of channels than the requesting
   * component needs. This class handles the conversion by:
   *   - Internally adds a callback to receive the raw microphone data
   *   - The ``process_audio_`` handles the raw data
   *     - Only the channels set in the ``channels_`` bitset are passed through
   *     - Passed through samples have the bits per sample converted
   *     - A gain factor is optionally applied to increase the volume - audio may clip!
   *   - The processed audio is passed to the callback of the component requesting microphone data
   *   - It tracks an internal enabled state, so it ignores raw microphone data when the component requesting
   *     microphone data is not actively requesting audio.
   *
   * Note that this class cannot convert sample rates!
   */
 public:
  MicrophoneSource(Microphone *mic, uint8_t bits_per_sample, int32_t gain_factor, bool passive)
      : mic_(mic), bits_per_sample_(bits_per_sample), gain_factor_(gain_factor), passive_(passive) {}

  /// @brief Enables a channel to be processed through the callback.
  ///
  /// If the microphone component only has reads from one channel, it is always in channel number 0, regardless if it
  /// represents left or right. If the microphone reads from both left and right, channel number 0 and 1 represent the
  /// left and right channels respectively.
  ///
  /// @param channel 0-indexed channel number to enable
  void add_channel(uint8_t channel) { this->channels_.set(channel); }

  void add_data_callback(std::function<void(const std::vector<uint8_t> &)> &&data_callback);

  void set_gain_factor(int32_t gain_factor) { this->gain_factor_ = clamp<int32_t>(gain_factor, 1, MAX_GAIN_FACTOR); }
  int32_t get_gain_factor() { return this->gain_factor_; }

  /// @brief Gets the AudioStreamInfo of the data after processing
  /// @return audio::AudioStreamInfo with the configured bits per sample, configured channel count, and source
  ///         microphone's sample rate
  audio::AudioStreamInfo get_audio_stream_info();

  void start();
  void stop();
  bool is_passive() const { return this->passive_; }
  bool is_running() const { return (this->mic_->is_running() && (this->enabled_ || this->passive_)); }
  bool is_stopped() const { return !this->is_running(); };

 protected:
  void process_audio_(const std::vector<uint8_t> &data, std::vector<uint8_t> &filtered_data);

  std::shared_ptr<std::vector<uint8_t>> processed_samples_;

  Microphone *mic_;
  uint8_t bits_per_sample_;
  std::bitset<8> channels_;
  int32_t gain_factor_;
  bool enabled_{false};
  bool passive_;  // Only pass audio if ``mic_`` is already running
};

}  // namespace microphone
}  // namespace esphome
