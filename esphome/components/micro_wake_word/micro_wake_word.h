#pragma once

/**
 * This is a workaround until we can figure out a way to get
 * the tflite-micro idf component code available in CI
 *
 * */
//
#ifndef CLANG_TIDY

#ifdef USE_ESP_IDF

#include "preprocessor_settings.h"
#include "streaming_model.h"

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/ring_buffer.h"

#include "esphome/components/microphone/microphone.h"

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

  void set_microphone(microphone::Microphone *microphone) { this->microphone_ = microphone; }

  Trigger<std::string> *get_wake_word_detected_trigger() const { return this->wake_word_detected_trigger_; }

  void add_wake_word_model(const uint8_t *model_start, float probability_cutoff, size_t sliding_window_average_size,
                           const std::string &wake_word, size_t tensor_arena_size);

#ifdef USE_MWW_VAD
  void add_vad_model(const uint8_t *model_start, float upper_threshold, float lower_threshold,
                     size_t sliding_window_size, size_t tensor_arena_size);
#endif

 protected:
  microphone::Microphone *microphone_{nullptr};
  Trigger<std::string> *wake_word_detected_trigger_ = new Trigger<std::string>();
  State state_{State::IDLE};
  HighFrequencyLoopRequester high_freq_;

  std::unique_ptr<RingBuffer> ring_buffer_;

  std::vector<WakeWordModel> wake_word_models_;

#ifdef USE_MWW_VAD
  VADModel *vad_model_;
#endif

  tflite::MicroMutableOpResolver<17> streaming_op_resolver_;
  tflite::MicroMutableOpResolver<18> preprocessor_op_resolver_;

  tflite::MicroInterpreter *preprocessor_interpreter_{nullptr};

  // When the wake word detection first starts, we ignore this many audio
  // feature slices before accepting a positive detection
  int16_t ignore_windows_{-MIN_SLICES_BEFORE_DETECTION};

  uint8_t *preprocessor_tensor_arena_{nullptr};

  // Stores audio read from the microphone before being added to the ring buffer.
  int16_t *input_buffer_{nullptr};
  // Stores audio fed into feature generator preprocessor. Also used for striding samples in each window.
  int16_t *preprocessor_audio_buffer_{nullptr};

  bool detected_{false};
  std::string detected_wake_word_{""};

  void set_state_(State state);

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

  /// @brief Loads streaming models
  /// @return True if successful, false otherwise
  bool load_models_();

  /// @brief Deletes each model's TFLite interpreters and frees tensor arena memory
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

  /** Reads in new audio data from ring buffer to create the next sample window
   *
   * Moves the last 10 ms of audio from the previous window to the start of the new window.
   * The next 20 ms of audio is copied from the ring buffer and inserted into the new window.
   * The new window's audio samples are stored in preprocessor_audio_buffer_.
   * Adapted from the TFLite micro speech example.
   * @return True if successful, false otherwise
   */
  bool stride_audio_samples_();

  /** Generates features for a window of audio samples
   *
   * Feeds the strided audio samples in preprocessor_audio_buffer_ into the preprocessor.
   * Adapted from TFLite micro speech example.
   * @param features int8_t array to store the audio features
   * @return True if successful, false otherwise.
   */
  bool generate_features_for_window_(int8_t features[PREPROCESSOR_FEATURE_SIZE]);

  /// @brief Resets the ring buffer, ignore_windows_, and sliding window probabilities
  void reset_states_();

  /// @brief Returns true if successfully registered the preprocessor's TensorFlow operations
  bool register_preprocessor_ops_(tflite::MicroMutableOpResolver<18> &op_resolver);

  /// @brief Returns true if successfully registered the streaming model's TensorFlow operations
  bool register_streaming_ops_(tflite::MicroMutableOpResolver<17> &op_resolver);
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

#endif  // CLANG_TIDY
