#include "micro_wake_word.h"

/**
 * This is a workaround until we can figure out a way to get
 * the tflite-micro idf component code available in CI
 *
 * */
//
#ifndef CLANG_TIDY

#ifdef USE_ESP_IDF

#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include "audio_preprocessor_int8_model_data.h"

#include <tensorflow/lite/core/c/common.h>
#include <tensorflow/lite/micro/micro_interpreter.h>
#include <tensorflow/lite/micro/micro_mutable_op_resolver.h>

#include <cinttypes>
#include <cmath>

namespace esphome {
namespace micro_wake_word {

static const char *const TAG = "micro_wake_word";

static const size_t SAMPLE_RATE_HZ = 16000;  // 16 kHz
static const size_t BUFFER_LENGTH = 500;     // 0.5 seconds
static const size_t BUFFER_SIZE = SAMPLE_RATE_HZ / 1000 * BUFFER_LENGTH;
static const size_t INPUT_BUFFER_SIZE = 32 * SAMPLE_RATE_HZ / 1000;  // 32ms * 16kHz / 1000ms

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
  ESP_LOGCONFIG(TAG, "  Wake Word: %s", this->get_wake_word().c_str());
  ESP_LOGCONFIG(TAG, "  Probability cutoff: %.3f", this->probability_cutoff_);
  ESP_LOGCONFIG(TAG, "  Sliding window size: %d", this->sliding_window_average_size_);
}

void MicroWakeWord::setup() {
  ESP_LOGCONFIG(TAG, "Setting up microWakeWord...");

  if (!this->initialize_models()) {
    ESP_LOGE(TAG, "Failed to initialize models");
    this->mark_failed();
    return;
  }

  ExternalRAMAllocator<int16_t> allocator(ExternalRAMAllocator<int16_t>::ALLOW_FAILURE);
  this->input_buffer_ = allocator.allocate(INPUT_BUFFER_SIZE * sizeof(int16_t));
  if (this->input_buffer_ == nullptr) {
    ESP_LOGW(TAG, "Could not allocate input buffer");
    this->mark_failed();
    return;
  }

  this->ring_buffer_ = RingBuffer::create(BUFFER_SIZE * sizeof(int16_t));
  if (this->ring_buffer_ == nullptr) {
    ESP_LOGW(TAG, "Could not allocate ring buffer");
    this->mark_failed();
    return;
  }

  ESP_LOGCONFIG(TAG, "Micro Wake Word initialized");
}

int MicroWakeWord::read_microphone_() {
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
      this->read_microphone_();
      if (this->detect_wake_word_()) {
        ESP_LOGD(TAG, "Wake Word Detected");
        this->detected_ = true;
        this->set_state_(State::STOP_MICROPHONE);
      }
      break;
    case State::STOP_MICROPHONE:
      ESP_LOGD(TAG, "Stopping Microphone");
      this->microphone_->stop();
      this->set_state_(State::STOPPING_MICROPHONE);
      this->high_freq_.stop();
      break;
    case State::STOPPING_MICROPHONE:
      if (this->microphone_->is_stopped()) {
        this->set_state_(State::IDLE);
        if (this->detected_) {
          this->detected_ = false;
          this->wake_word_detected_trigger_->trigger(this->wake_word_);
        }
      }
      break;
  }
}

void MicroWakeWord::start() {
  if (this->is_failed()) {
    ESP_LOGW(TAG, "Wake word component is marked as failed. Please check setup logs");
    return;
  }
  if (this->state_ != State::IDLE) {
    ESP_LOGW(TAG, "Wake word is already running");
    return;
  }
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

bool MicroWakeWord::initialize_models() {
  ExternalRAMAllocator<uint8_t> arena_allocator(ExternalRAMAllocator<uint8_t>::ALLOW_FAILURE);
  ExternalRAMAllocator<int8_t> features_allocator(ExternalRAMAllocator<int8_t>::ALLOW_FAILURE);
  ExternalRAMAllocator<int16_t> audio_samples_allocator(ExternalRAMAllocator<int16_t>::ALLOW_FAILURE);

  this->streaming_tensor_arena_ = arena_allocator.allocate(STREAMING_MODEL_ARENA_SIZE);
  if (this->streaming_tensor_arena_ == nullptr) {
    ESP_LOGE(TAG, "Could not allocate the streaming model's tensor arena.");
    return false;
  }

  this->streaming_var_arena_ = arena_allocator.allocate(STREAMING_MODEL_VARIABLE_ARENA_SIZE);
  if (this->streaming_var_arena_ == nullptr) {
    ESP_LOGE(TAG, "Could not allocate the streaming model variable's tensor arena.");
    return false;
  }

  this->preprocessor_tensor_arena_ = arena_allocator.allocate(PREPROCESSOR_ARENA_SIZE);
  if (this->preprocessor_tensor_arena_ == nullptr) {
    ESP_LOGE(TAG, "Could not allocate the audio preprocessor model's tensor arena.");
    return false;
  }

  this->new_features_data_ = features_allocator.allocate(PREPROCESSOR_FEATURE_SIZE);
  if (this->new_features_data_ == nullptr) {
    ESP_LOGE(TAG, "Could not allocate the audio features buffer.");
    return false;
  }

  this->preprocessor_audio_buffer_ = audio_samples_allocator.allocate(SAMPLE_DURATION_COUNT);
  if (this->preprocessor_audio_buffer_ == nullptr) {
    ESP_LOGE(TAG, "Could not allocate the audio preprocessor's buffer.");
    return false;
  }

  this->preprocessor_model_ = tflite::GetModel(G_AUDIO_PREPROCESSOR_INT8_TFLITE);
  if (this->preprocessor_model_->version() != TFLITE_SCHEMA_VERSION) {
    ESP_LOGE(TAG, "Wake word's audio preprocessor model's schema is not supported");
    return false;
  }

  this->streaming_model_ = tflite::GetModel(this->model_start_);
  if (this->streaming_model_->version() != TFLITE_SCHEMA_VERSION) {
    ESP_LOGE(TAG, "Wake word's streaming model's schema is not supported");
    return false;
  }

  static tflite::MicroMutableOpResolver<18> preprocessor_op_resolver;
  static tflite::MicroMutableOpResolver<17> streaming_op_resolver;

  if (!this->register_preprocessor_ops_(preprocessor_op_resolver))
    return false;
  if (!this->register_streaming_ops_(streaming_op_resolver))
    return false;

  tflite::MicroAllocator *ma =
      tflite::MicroAllocator::Create(this->streaming_var_arena_, STREAMING_MODEL_VARIABLE_ARENA_SIZE);
  this->mrv_ = tflite::MicroResourceVariables::Create(ma, 15);

  static tflite::MicroInterpreter static_preprocessor_interpreter(
      this->preprocessor_model_, preprocessor_op_resolver, this->preprocessor_tensor_arena_, PREPROCESSOR_ARENA_SIZE);

  static tflite::MicroInterpreter static_streaming_interpreter(this->streaming_model_, streaming_op_resolver,
                                                               this->streaming_tensor_arena_,
                                                               STREAMING_MODEL_ARENA_SIZE, this->mrv_);

  this->preprocessor_interperter_ = &static_preprocessor_interpreter;
  this->streaming_interpreter_ = &static_streaming_interpreter;

  // Allocate tensors for each models.
  if (this->preprocessor_interperter_->AllocateTensors() != kTfLiteOk) {
    ESP_LOGE(TAG, "Failed to allocate tensors for the audio preprocessor");
    return false;
  }
  if (this->streaming_interpreter_->AllocateTensors() != kTfLiteOk) {
    ESP_LOGE(TAG, "Failed to allocate tensors for the streaming model");
    return false;
  }

  // Verify input tensor matches expected values
  TfLiteTensor *input = this->streaming_interpreter_->input(0);
  if ((input->dims->size != 3) || (input->dims->data[0] != 1) || (input->dims->data[0] != 1) ||
      (input->dims->data[1] != 1) || (input->dims->data[2] != PREPROCESSOR_FEATURE_SIZE)) {
    ESP_LOGE(TAG, "Wake word detection model tensor input dimensions is not 1x1x%u", input->dims->data[2]);
    return false;
  }

  if (input->type != kTfLiteInt8) {
    ESP_LOGE(TAG, "Wake word detection model tensor input is not int8.");
    return false;
  }

  // Verify output tensor matches expected values
  TfLiteTensor *output = this->streaming_interpreter_->output(0);
  if ((output->dims->size != 2) || (output->dims->data[0] != 1) || (output->dims->data[1] != 1)) {
    ESP_LOGE(TAG, "Wake word detection model tensor output dimensions is not 1x1.");
  }

  if (output->type != kTfLiteUInt8) {
    ESP_LOGE(TAG, "Wake word detection model tensor input is not uint8.");
    return false;
  }

  this->recent_streaming_probabilities_.resize(this->sliding_window_average_size_, 0.0);

  return true;
}

bool MicroWakeWord::update_features_() {
  // Retrieve strided audio samples
  int16_t *audio_samples = nullptr;
  if (!this->stride_audio_samples_(&audio_samples)) {
    return false;
  }

  // Compute the features for the newest audio samples
  if (!this->generate_single_feature_(audio_samples, SAMPLE_DURATION_COUNT, this->new_features_data_)) {
    return false;
  }

  return true;
}

float MicroWakeWord::perform_streaming_inference_() {
  TfLiteTensor *input = this->streaming_interpreter_->input(0);

  size_t bytes_to_copy = input->bytes;

  memcpy((void *) (tflite::GetTensorData<int8_t>(input)), (const void *) (this->new_features_data_), bytes_to_copy);

  uint32_t prior_invoke = millis();

  TfLiteStatus invoke_status = this->streaming_interpreter_->Invoke();
  if (invoke_status != kTfLiteOk) {
    ESP_LOGW(TAG, "Streaming Interpreter Invoke failed");
    return false;
  }

  ESP_LOGV(TAG, "Streaming Inference Latency=%" PRIu32 " ms", (millis() - prior_invoke));

  TfLiteTensor *output = this->streaming_interpreter_->output(0);

  return static_cast<float>(output->data.uint8[0]) / 255.0;
}

bool MicroWakeWord::detect_wake_word_() {
  // Preprocess the newest audio samples into features
  if (!this->update_features_()) {
    return false;
  }

  // Perform inference
  float streaming_prob = this->perform_streaming_inference_();

  // Add the most recent probability to the sliding window
  this->recent_streaming_probabilities_[this->last_n_index_] = streaming_prob;
  ++this->last_n_index_;
  if (this->last_n_index_ == this->sliding_window_average_size_)
    this->last_n_index_ = 0;

  float sum = 0.0;
  for (auto &prob : this->recent_streaming_probabilities_) {
    sum += prob;
  }

  float sliding_window_average = sum / static_cast<float>(this->sliding_window_average_size_);

  // Ensure we have enough samples since the last positive detection
  this->ignore_windows_ = std::min(this->ignore_windows_ + 1, 0);
  if (this->ignore_windows_ < 0) {
    return false;
  }

  // Detect the wake word if the sliding window average is above the cutoff
  if (sliding_window_average > this->probability_cutoff_) {
    this->ignore_windows_ = -MIN_SLICES_BEFORE_DETECTION;
    for (auto &prob : this->recent_streaming_probabilities_) {
      prob = 0;
    }

    ESP_LOGD(TAG, "Wake word sliding average probability is %.3f and most recent probability is %.3f",
             sliding_window_average, streaming_prob);
    return true;
  }

  return false;
}

void MicroWakeWord::set_sliding_window_average_size(size_t size) {
  this->sliding_window_average_size_ = size;
  this->recent_streaming_probabilities_.resize(this->sliding_window_average_size_, 0.0);
}

bool MicroWakeWord::slice_available_() {
  size_t available = this->ring_buffer_->available();

  return available > (NEW_SAMPLES_TO_GET * sizeof(int16_t));
}

bool MicroWakeWord::stride_audio_samples_(int16_t **audio_samples) {
  if (!this->slice_available_()) {
    return false;
  }

  // Copy the last 320 bytes (160 samples over 10 ms) from the audio buffer to the start of the audio buffer
  memcpy((void *) (this->preprocessor_audio_buffer_), (void *) (this->preprocessor_audio_buffer_ + NEW_SAMPLES_TO_GET),
         HISTORY_SAMPLES_TO_KEEP * sizeof(int16_t));

  // Copy 640 bytes (320 samples over 20 ms) from the ring buffer into the audio buffer offset 320 bytes (160 samples
  // over 10 ms)
  size_t bytes_read = this->ring_buffer_->read((void *) (this->preprocessor_audio_buffer_ + HISTORY_SAMPLES_TO_KEEP),
                                               NEW_SAMPLES_TO_GET * sizeof(int16_t), pdMS_TO_TICKS(200));

  if (bytes_read == 0) {
    ESP_LOGE(TAG, "Could not read data from Ring Buffer");
  } else if (bytes_read < NEW_SAMPLES_TO_GET * sizeof(int16_t)) {
    ESP_LOGD(TAG, "Partial Read of Data by Model");
    ESP_LOGD(TAG, "Could only read %d bytes when required %d bytes ", bytes_read,
             (int) (NEW_SAMPLES_TO_GET * sizeof(int16_t)));
    return false;
  }

  *audio_samples = this->preprocessor_audio_buffer_;
  return true;
}

bool MicroWakeWord::generate_single_feature_(const int16_t *audio_data, const int audio_data_size,
                                             int8_t feature_output[PREPROCESSOR_FEATURE_SIZE]) {
  TfLiteTensor *input = this->preprocessor_interperter_->input(0);
  TfLiteTensor *output = this->preprocessor_interperter_->output(0);
  std::copy_n(audio_data, audio_data_size, tflite::GetTensorData<int16_t>(input));

  if (this->preprocessor_interperter_->Invoke() != kTfLiteOk) {
    ESP_LOGE(TAG, "Failed to preprocess audio for local wake word.");
    return false;
  }
  std::memcpy(feature_output, tflite::GetTensorData<int8_t>(output), PREPROCESSOR_FEATURE_SIZE * sizeof(int8_t));

  return true;
}

bool MicroWakeWord::register_preprocessor_ops_(tflite::MicroMutableOpResolver<18> &op_resolver) {
  if (op_resolver.AddReshape() != kTfLiteOk)
    return false;
  if (op_resolver.AddCast() != kTfLiteOk)
    return false;
  if (op_resolver.AddStridedSlice() != kTfLiteOk)
    return false;
  if (op_resolver.AddConcatenation() != kTfLiteOk)
    return false;
  if (op_resolver.AddMul() != kTfLiteOk)
    return false;
  if (op_resolver.AddAdd() != kTfLiteOk)
    return false;
  if (op_resolver.AddDiv() != kTfLiteOk)
    return false;
  if (op_resolver.AddMinimum() != kTfLiteOk)
    return false;
  if (op_resolver.AddMaximum() != kTfLiteOk)
    return false;
  if (op_resolver.AddWindow() != kTfLiteOk)
    return false;
  if (op_resolver.AddFftAutoScale() != kTfLiteOk)
    return false;
  if (op_resolver.AddRfft() != kTfLiteOk)
    return false;
  if (op_resolver.AddEnergy() != kTfLiteOk)
    return false;
  if (op_resolver.AddFilterBank() != kTfLiteOk)
    return false;
  if (op_resolver.AddFilterBankSquareRoot() != kTfLiteOk)
    return false;
  if (op_resolver.AddFilterBankSpectralSubtraction() != kTfLiteOk)
    return false;
  if (op_resolver.AddPCAN() != kTfLiteOk)
    return false;
  if (op_resolver.AddFilterBankLog() != kTfLiteOk)
    return false;

  return true;
}

bool MicroWakeWord::register_streaming_ops_(tflite::MicroMutableOpResolver<17> &op_resolver) {
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

  return true;
}

}  // namespace micro_wake_word
}  // namespace esphome

#endif  // USE_ESP_IDF

#endif  // CLANG_TIDY
