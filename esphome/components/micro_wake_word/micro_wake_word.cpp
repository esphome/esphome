#include "micro_wake_word.h"
#include "streaming_model.h"

#ifdef USE_ESP_IDF

#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <frontend.h>
#include <frontend_util.h>

#include <tensorflow/lite/core/c/common.h>
#include <tensorflow/lite/micro/micro_interpreter.h>
#include <tensorflow/lite/micro/micro_mutable_op_resolver.h>

#include <cmath>

namespace esphome {
namespace micro_wake_word {

static const char *const TAG = "micro_wake_word";

static const size_t SAMPLE_RATE_HZ = 16000;  // 16 kHz
static const size_t BUFFER_LENGTH = 64;      // 0.064 seconds
static const size_t BUFFER_SIZE = SAMPLE_RATE_HZ / 1000 * BUFFER_LENGTH;
static const size_t INPUT_BUFFER_SIZE = 16 * SAMPLE_RATE_HZ / 1000;  // 16ms * 16kHz / 1000ms

float MicroWakeWord::get_setup_priority() const { return setup_priority::AFTER_CONNECTION; }

static const LogString *micro_wake_word_state_to_string(State state) {
  switch (state) {
    case State::IDLE:
      return LOG_STR("IDLE");
    case State::START_MICROPHONE:
      return LOG_STR("START_MICROPHONE");
    case State::STARTING_MICROPHONE:
      return LOG_STR("STARTING_MICROPHONE");
    case State::DETECTING_WAKE_WORD:
      return LOG_STR("DETECTING_WAKE_WORD");
    case State::STOP_MICROPHONE:
      return LOG_STR("STOP_MICROPHONE");
    case State::STOPPING_MICROPHONE:
      return LOG_STR("STOPPING_MICROPHONE");
    default:
      return LOG_STR("UNKNOWN");
  }
}

void MicroWakeWord::dump_config() {
  ESP_LOGCONFIG(TAG, "microWakeWord:");
  ESP_LOGCONFIG(TAG, "  models:");
  for (auto &model : this->wake_word_models_) {
    model.log_model_config();
  }
#ifdef USE_MICRO_WAKE_WORD_VAD
  this->vad_model_->log_model_config();
#endif
}

void MicroWakeWord::setup() {
  ESP_LOGCONFIG(TAG, "Setting up microWakeWord...");

  if (!this->register_streaming_ops_(this->streaming_op_resolver_)) {
    this->mark_failed();
    return;
  }

  ESP_LOGCONFIG(TAG, "Micro Wake Word initialized");

  this->frontend_config_.window.size_ms = FEATURE_DURATION_MS;
  this->frontend_config_.window.step_size_ms = this->features_step_size_;
  this->frontend_config_.filterbank.num_channels = PREPROCESSOR_FEATURE_SIZE;
  this->frontend_config_.filterbank.lower_band_limit = 125.0;
  this->frontend_config_.filterbank.upper_band_limit = 7500.0;
  this->frontend_config_.noise_reduction.smoothing_bits = 10;
  this->frontend_config_.noise_reduction.even_smoothing = 0.025;
  this->frontend_config_.noise_reduction.odd_smoothing = 0.06;
  this->frontend_config_.noise_reduction.min_signal_remaining = 0.05;
  this->frontend_config_.pcan_gain_control.enable_pcan = 1;
  this->frontend_config_.pcan_gain_control.strength = 0.95;
  this->frontend_config_.pcan_gain_control.offset = 80.0;
  this->frontend_config_.pcan_gain_control.gain_bits = 21;
  this->frontend_config_.log_scale.enable_log = 1;
  this->frontend_config_.log_scale.scale_shift = 6;
}

void MicroWakeWord::add_wake_word_model(const uint8_t *model_start, float probability_cutoff,
                                        size_t sliding_window_average_size, const std::string &wake_word,
                                        size_t tensor_arena_size) {
  this->wake_word_models_.emplace_back(model_start, probability_cutoff, sliding_window_average_size, wake_word,
                                       tensor_arena_size);
}

#ifdef USE_MICRO_WAKE_WORD_VAD
void MicroWakeWord::add_vad_model(const uint8_t *model_start, float probability_cutoff, size_t sliding_window_size,
                                  size_t tensor_arena_size) {
  this->vad_model_ = make_unique<VADModel>(model_start, probability_cutoff, sliding_window_size, tensor_arena_size);
}
#endif

void MicroWakeWord::loop() {
  switch (this->state_) {
    case State::IDLE:
      break;
    case State::START_MICROPHONE:
      ESP_LOGD(TAG, "Starting Microphone");
      this->microphone_->start();
      this->set_state_(State::STARTING_MICROPHONE);
      this->high_freq_.start();
      break;
    case State::STARTING_MICROPHONE:
      if (this->microphone_->is_running()) {
        this->set_state_(State::DETECTING_WAKE_WORD);
      }
      break;
    case State::DETECTING_WAKE_WORD:
      while (!this->has_enough_samples_()) {
        this->read_microphone_();
      }
      this->update_model_probabilities_();
      if (this->detect_wake_words_()) {
        ESP_LOGD(TAG, "Wake Word '%s' Detected", (this->detected_wake_word_).c_str());
        this->detected_ = true;
        this->set_state_(State::STOP_MICROPHONE);
      }
      break;
    case State::STOP_MICROPHONE:
      ESP_LOGD(TAG, "Stopping Microphone");
      this->microphone_->stop();
      this->set_state_(State::STOPPING_MICROPHONE);
      this->high_freq_.stop();
      this->unload_models_();
      this->deallocate_buffers_();
      break;
    case State::STOPPING_MICROPHONE:
      if (this->microphone_->is_stopped()) {
        this->set_state_(State::IDLE);
        if (this->detected_) {
          this->wake_word_detected_trigger_->trigger(this->detected_wake_word_);
          this->detected_ = false;
          this->detected_wake_word_ = "";
        }
      }
      break;
  }
}

void MicroWakeWord::start() {
  if (!this->is_ready()) {
    ESP_LOGW(TAG, "Wake word detection can't start as the component hasn't been setup yet");
    return;
  }

  if (this->is_failed()) {
    ESP_LOGW(TAG, "Wake word component is marked as failed. Please check setup logs");
    return;
  }

  if (!this->load_models_() || !this->allocate_buffers_()) {
    ESP_LOGE(TAG, "Failed to load the wake word model(s) or allocate buffers");
    this->status_set_error();
  } else {
    this->status_clear_error();
  }

  if (this->status_has_error()) {
    ESP_LOGW(TAG, "Wake word component has an error. Please check logs");
    return;
  }

  if (this->state_ != State::IDLE) {
    ESP_LOGW(TAG, "Wake word is already running");
    return;
  }

  this->reset_states_();
  this->set_state_(State::START_MICROPHONE);
}

void MicroWakeWord::stop() {
  if (this->state_ == State::IDLE) {
    ESP_LOGW(TAG, "Wake word is already stopped");
    return;
  }
  if (this->state_ == State::STOPPING_MICROPHONE) {
    ESP_LOGW(TAG, "Wake word is already stopping");
    return;
  }
  this->set_state_(State::STOP_MICROPHONE);
}

void MicroWakeWord::set_state_(State state) {
  ESP_LOGD(TAG, "State changed from %s to %s", LOG_STR_ARG(micro_wake_word_state_to_string(this->state_)),
           LOG_STR_ARG(micro_wake_word_state_to_string(state)));
  this->state_ = state;
}

size_t MicroWakeWord::read_microphone_() {
  size_t bytes_read = this->microphone_->read(this->input_buffer_, INPUT_BUFFER_SIZE * sizeof(int16_t));
  if (bytes_read == 0) {
    return 0;
  }

  size_t bytes_free = this->ring_buffer_->free();

  if (bytes_free < bytes_read) {
    ESP_LOGW(TAG,
             "Not enough free bytes in ring buffer to store incoming audio data (free bytes=%d, incoming bytes=%d). "
             "Resetting the ring buffer. Wake word detection accuracy will be reduced.",
             bytes_free, bytes_read);

    this->ring_buffer_->reset();
  }

  return this->ring_buffer_->write((void *) this->input_buffer_, bytes_read);
}

bool MicroWakeWord::allocate_buffers_() {
  ExternalRAMAllocator<int16_t> audio_samples_allocator(ExternalRAMAllocator<int16_t>::ALLOW_FAILURE);

  if (this->input_buffer_ == nullptr) {
    this->input_buffer_ = audio_samples_allocator.allocate(INPUT_BUFFER_SIZE * sizeof(int16_t));
    if (this->input_buffer_ == nullptr) {
      ESP_LOGE(TAG, "Could not allocate input buffer");
      return false;
    }
  }

  if (this->preprocessor_audio_buffer_ == nullptr) {
    this->preprocessor_audio_buffer_ = audio_samples_allocator.allocate(this->new_samples_to_get_());
    if (this->preprocessor_audio_buffer_ == nullptr) {
      ESP_LOGE(TAG, "Could not allocate the audio preprocessor's buffer.");
      return false;
    }
  }

  if (this->ring_buffer_ == nullptr) {
    this->ring_buffer_ = RingBuffer::create(BUFFER_SIZE * sizeof(int16_t));
    if (this->ring_buffer_ == nullptr) {
      ESP_LOGE(TAG, "Could not allocate ring buffer");
      return false;
    }
  }

  return true;
}

void MicroWakeWord::deallocate_buffers_() {
  ExternalRAMAllocator<int16_t> audio_samples_allocator(ExternalRAMAllocator<int16_t>::ALLOW_FAILURE);
  audio_samples_allocator.deallocate(this->input_buffer_, INPUT_BUFFER_SIZE * sizeof(int16_t));
  this->input_buffer_ = nullptr;
  audio_samples_allocator.deallocate(this->preprocessor_audio_buffer_, this->new_samples_to_get_());
  this->preprocessor_audio_buffer_ = nullptr;
}

bool MicroWakeWord::load_models_() {
  // Setup preprocesor feature generator
  if (!FrontendPopulateState(&this->frontend_config_, &this->frontend_state_, AUDIO_SAMPLE_FREQUENCY)) {
    ESP_LOGD(TAG, "Failed to populate frontend state");
    FrontendFreeStateContents(&this->frontend_state_);
    return false;
  }

  // Setup streaming models
  for (auto &model : this->wake_word_models_) {
    if (!model.load_model(this->streaming_op_resolver_)) {
      ESP_LOGE(TAG, "Failed to initialize a wake word model.");
      return false;
    }
  }
#ifdef USE_MICRO_WAKE_WORD_VAD
  if (!this->vad_model_->load_model(this->streaming_op_resolver_)) {
    ESP_LOGE(TAG, "Failed to initialize VAD model.");
    return false;
  }
#endif

  return true;
}

void MicroWakeWord::unload_models_() {
  FrontendFreeStateContents(&this->frontend_state_);

  for (auto &model : this->wake_word_models_) {
    model.unload_model();
  }
#ifdef USE_MICRO_WAKE_WORD_VAD
  this->vad_model_->unload_model();
#endif
}

void MicroWakeWord::update_model_probabilities_() {
  int8_t audio_features[PREPROCESSOR_FEATURE_SIZE];

  if (!this->generate_features_for_window_(audio_features)) {
    return;
  }

  // Increase the counter since the last positive detection
  this->ignore_windows_ = std::min(this->ignore_windows_ + 1, 0);

  for (auto &model : this->wake_word_models_) {
    // Perform inference
    model.perform_streaming_inference(audio_features);
  }
#ifdef USE_MICRO_WAKE_WORD_VAD
  this->vad_model_->perform_streaming_inference(audio_features);
#endif
}

bool MicroWakeWord::detect_wake_words_() {
  // Verify we have processed samples since the last positive detection
  if (this->ignore_windows_ < 0) {
    return false;
  }

#ifdef USE_MICRO_WAKE_WORD_VAD
  bool vad_state = this->vad_model_->determine_detected();
#endif

  for (auto &model : this->wake_word_models_) {
    if (model.determine_detected()) {
#ifdef USE_MICRO_WAKE_WORD_VAD
      if (vad_state) {
#endif
        this->detected_wake_word_ = model.get_wake_word();
        return true;
#ifdef USE_MICRO_WAKE_WORD_VAD
      } else {
        ESP_LOGD(TAG, "Wake word model predicts %s, but VAD model doesn't.", model.get_wake_word().c_str());
      }
#endif
    }
  }

  return false;
}

bool MicroWakeWord::has_enough_samples_() {
  return this->ring_buffer_->available() >=
         (this->features_step_size_ * (AUDIO_SAMPLE_FREQUENCY / 1000)) * sizeof(int16_t);
}

bool MicroWakeWord::generate_features_for_window_(int8_t features[PREPROCESSOR_FEATURE_SIZE]) {
  // Ensure we have enough new audio samples in the ring buffer for a full window
  if (!this->has_enough_samples_()) {
    return false;
  }

  size_t bytes_read = this->ring_buffer_->read((void *) (this->preprocessor_audio_buffer_),
                                               this->new_samples_to_get_() * sizeof(int16_t), pdMS_TO_TICKS(200));

  if (bytes_read == 0) {
    ESP_LOGE(TAG, "Could not read data from Ring Buffer");
  } else if (bytes_read < this->new_samples_to_get_() * sizeof(int16_t)) {
    ESP_LOGD(TAG, "Partial Read of Data by Model");
    ESP_LOGD(TAG, "Could only read %d bytes when required %d bytes ", bytes_read,
             (int) (this->new_samples_to_get_() * sizeof(int16_t)));
    return false;
  }

  size_t num_samples_read;
  struct FrontendOutput frontend_output = FrontendProcessSamples(
      &this->frontend_state_, this->preprocessor_audio_buffer_, this->new_samples_to_get_(), &num_samples_read);

  for (size_t i = 0; i < frontend_output.size; ++i) {
    // These scaling values are set to match the TFLite audio frontend int8 output.
    // The feature pipeline outputs 16-bit signed integers in roughly a 0 to 670
    // range. In training, these are then arbitrarily divided by 25.6 to get
    // float values in the rough range of 0.0 to 26.0. This scaling is performed
    // for historical reasons, to match up with the output of other feature
    // generators.
    // The process is then further complicated when we quantize the model. This
    // means we have to scale the 0.0 to 26.0 real values to the -128 to 127
    // signed integer numbers.
    // All this means that to get matching values from our integer feature
    // output into the tensor input, we have to perform:
    // input = (((feature / 25.6) / 26.0) * 256) - 128
    // To simplify this and perform it in 32-bit integer math, we rearrange to:
    // input = (feature * 256) / (25.6 * 26.0) - 128
    constexpr int32_t value_scale = 256;
    constexpr int32_t value_div = 666;  // 666 = 25.6 * 26.0 after rounding
    int32_t value = ((frontend_output.values[i] * value_scale) + (value_div / 2)) / value_div;
    value -= 128;
    if (value < -128) {
      value = -128;
    }
    if (value > 127) {
      value = 127;
    }
    features[i] = value;
  }

  return true;
}

void MicroWakeWord::reset_states_() {
  ESP_LOGD(TAG, "Resetting buffers and probabilities");
  this->ring_buffer_->reset();
  this->ignore_windows_ = -MIN_SLICES_BEFORE_DETECTION;
  for (auto &model : this->wake_word_models_) {
    model.reset_probabilities();
  }
#ifdef USE_MICRO_WAKE_WORD_VAD
  this->vad_model_->reset_probabilities();
#endif
}

bool MicroWakeWord::register_streaming_ops_(tflite::MicroMutableOpResolver<20> &op_resolver) {
  if (op_resolver.AddCallOnce() != kTfLiteOk)
    return false;
  if (op_resolver.AddVarHandle() != kTfLiteOk)
    return false;
  if (op_resolver.AddReshape() != kTfLiteOk)
    return false;
  if (op_resolver.AddReadVariable() != kTfLiteOk)
    return false;
  if (op_resolver.AddStridedSlice() != kTfLiteOk)
    return false;
  if (op_resolver.AddConcatenation() != kTfLiteOk)
    return false;
  if (op_resolver.AddAssignVariable() != kTfLiteOk)
    return false;
  if (op_resolver.AddConv2D() != kTfLiteOk)
    return false;
  if (op_resolver.AddMul() != kTfLiteOk)
    return false;
  if (op_resolver.AddAdd() != kTfLiteOk)
    return false;
  if (op_resolver.AddMean() != kTfLiteOk)
    return false;
  if (op_resolver.AddFullyConnected() != kTfLiteOk)
    return false;
  if (op_resolver.AddLogistic() != kTfLiteOk)
    return false;
  if (op_resolver.AddQuantize() != kTfLiteOk)
    return false;
  if (op_resolver.AddDepthwiseConv2D() != kTfLiteOk)
    return false;
  if (op_resolver.AddAveragePool2D() != kTfLiteOk)
    return false;
  if (op_resolver.AddMaxPool2D() != kTfLiteOk)
    return false;
  if (op_resolver.AddPad() != kTfLiteOk)
    return false;
  if (op_resolver.AddPack() != kTfLiteOk)
    return false;
  if (op_resolver.AddSplitV() != kTfLiteOk)
    return false;

  return true;
}

}  // namespace micro_wake_word
}  // namespace esphome

#endif  // USE_ESP_IDF
