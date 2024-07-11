#pragma once

#ifdef USE_ESP_IDF

#include "preprocessor_settings.h"
#include "streaming_model.h"

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/ring_buffer.h"

#include "esphome/components/microphone/microphone.h"

#include <frontend_util.h>

#include <tensorflow/lite/core/c/common.h>
#include <tensorflow/lite/micro/micro_interpreter.h>
#include <tensorflow/lite/micro/micro_mutable_op_resolver.h>

namespace esphome {
namespace micro_wake_word {

enum State {
  IDLE,
  START_MICROPHONE,
  STARTING_MICROPHONE,
  DETECTING_WAKE_WORD,
  STOP_MICROPHONE,
  STOPPING_MICROPHONE,
};

// The number of audio slices to process before accepting a positive detection
static const uint8_t MIN_SLICES_BEFORE_DETECTION = 74;

class MicroWakeWord : public Component {
 public:
  void setup() override;
  void loop() override;
  float get_setup_priority() const override;
  void dump_config() override;

  void start();
  void stop();

  bool is_running() const { return this->state_ != State::IDLE; }

  void set_features_step_size(uint8_t step_size) { this->features_step_size_ = step_size; }

  void set_microphone(microphone::Microphone *microphone) { this->microphone_ = microphone; }

  Trigger<std::string> *get_wake_word_detected_trigger() const { return this->wake_word_detected_trigger_; }

  void add_wake_word_model(const uint8_t *model_start, float probability_cutoff, size_t sliding_window_average_size,
                           const std::string &wake_word, size_t tensor_arena_size);

#ifdef USE_MICRO_WAKE_WORD_VAD
  void add_vad_model(const uint8_t *model_start, float probability_cutoff, size_t sliding_window_size,
                     size_t tensor_arena_size);
#endif

 protected:
  microphone::Microphone *microphone_{nullptr};
  Trigger<std::string> *wake_word_detected_trigger_ = new Trigger<std::string>();
  State state_{State::IDLE};
  HighFrequencyLoopRequester high_freq_;

  std::unique_ptr<RingBuffer> ring_buffer_;

  std::vector<WakeWordModel> wake_word_models_;

#ifdef USE_MICRO_WAKE_WORD_VAD
  std::unique_ptr<VADModel> vad_model_;
#endif

  tflite::MicroMutableOpResolver<20> streaming_op_resolver_;

  // Audio frontend handles generating spectrogram features
  struct FrontendConfig frontend_config_;
  struct FrontendState frontend_state_;

  // When the wake word detection first starts, we ignore this many audio
  // feature slices before accepting a positive detection
  int16_t ignore_windows_{-MIN_SLICES_BEFORE_DETECTION};

  uint8_t features_step_size_;

  // Stores audio read from the microphone before being added to the ring buffer.
  int16_t *input_buffer_{nullptr};
  // Stores audio to be fed into the audio frontend for generating features.
  int16_t *preprocessor_audio_buffer_{nullptr};

  bool detected_{false};
  std::string detected_wake_word_{""};

  void set_state_(State state);

  /// @brief Tests if there are enough samples in the ring buffer to generate new features.
  /// @return True if enough samples, false otherwise.
  bool has_enough_samples_();

  /** Reads audio from microphone into the ring buffer
   *
   * Audio data (16000 kHz with int16 samples) is read into the input_buffer_.
   * Verifies the ring buffer has enough space for all audio data. If not, it logs
   * a warning and resets the ring buffer entirely.
   * @return Number of bytes written to the ring buffer
   */
  size_t read_microphone_();

  /// @brief Allocates memory for input_buffer_, preprocessor_audio_buffer_, and ring_buffer_
  /// @return True if successful, false otherwise
  bool allocate_buffers_();

  /// @brief Frees memory allocated for input_buffer_ and preprocessor_audio_buffer_
  void deallocate_buffers_();

  /// @brief Loads streaming models and prepares the feature generation frontend
  /// @return True if successful, false otherwise
  bool load_models_();

  /// @brief Deletes each model's TFLite interpreters and frees tensor arena memory. Frees memory used by the feature
  /// generation frontend.
  void unload_models_();

  /** Performs inference with each configured model
   *
   * If enough audio samples are available, it will generate one slice of new features.
   * It then loops through and performs inference with each of the loaded models.
   */
  void update_model_probabilities_();

  /** Checks every model's recent probabilities to determine if the wake word has been predicted
   *
   * Verifies the models have processed enough new samples for accurate predictions.
   * Sets detected_wake_word_ to the wake word, if one is detected.
   * @return True if a wake word is predicted, false otherwise
   */
  bool detect_wake_words_();

  /** Generates features for a window of audio samples
   *
   * Reads samples from the ring buffer and feeds them into the preprocessor frontend.
   * Adapted from TFLite microspeech frontend.
   * @param features int8_t array to store the audio features
   * @return True if successful, false otherwise.
   */
  bool generate_features_for_window_(int8_t features[PREPROCESSOR_FEATURE_SIZE]);

  /// @brief Resets the ring buffer, ignore_windows_, and sliding window probabilities
  void reset_states_();

  /// @brief Returns true if successfully registered the streaming model's TensorFlow operations
  bool register_streaming_ops_(tflite::MicroMutableOpResolver<20> &op_resolver);

  inline uint16_t new_samples_to_get_() { return (this->features_step_size_ * (AUDIO_SAMPLE_FREQUENCY / 1000)); }
};

template<typename... Ts> class StartAction : public Action<Ts...>, public Parented<MicroWakeWord> {
 public:
  void play(Ts... x) override { this->parent_->start(); }
};

template<typename... Ts> class StopAction : public Action<Ts...>, public Parented<MicroWakeWord> {
 public:
  void play(Ts... x) override { this->parent_->stop(); }
};

template<typename... Ts> class IsRunningCondition : public Condition<Ts...>, public Parented<MicroWakeWord> {
 public:
  bool check(Ts... x) override { return this->parent_->is_running(); }
};

}  // namespace micro_wake_word
}  // namespace esphome

#endif  // USE_ESP_IDF
