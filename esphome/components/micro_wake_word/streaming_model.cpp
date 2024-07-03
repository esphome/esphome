#ifdef USE_ESP_IDF

#include "streaming_model.h"

#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

static const char *const TAG = "micro_wake_word";

namespace esphome {
namespace micro_wake_word {

void WakeWordModel::log_model_config() {
  ESP_LOGCONFIG(TAG, "    - Wake Word: %s", this->wake_word_.c_str());
  ESP_LOGCONFIG(TAG, "      Probability cutoff: %.3f", this->probability_cutoff_);
  ESP_LOGCONFIG(TAG, "      Sliding window size: %d", this->sliding_window_size_);
}

void VADModel::log_model_config() {
  ESP_LOGCONFIG(TAG, "    - VAD Model");
  ESP_LOGCONFIG(TAG, "      Probability cutoff: %.3f", this->probability_cutoff_);
  ESP_LOGCONFIG(TAG, "      Sliding window size: %d", this->sliding_window_size_);
}

bool StreamingModel::load_model(tflite::MicroMutableOpResolver<20> &op_resolver) {
  ExternalRAMAllocator<uint8_t> arena_allocator(ExternalRAMAllocator<uint8_t>::ALLOW_FAILURE);

  if (this->tensor_arena_ == nullptr) {
    this->tensor_arena_ = arena_allocator.allocate(this->tensor_arena_size_);
    if (this->tensor_arena_ == nullptr) {
      ESP_LOGE(TAG, "Could not allocate the streaming model's tensor arena.");
      return false;
    }
  }

  if (this->var_arena_ == nullptr) {
    this->var_arena_ = arena_allocator.allocate(STREAMING_MODEL_VARIABLE_ARENA_SIZE);
    if (this->var_arena_ == nullptr) {
      ESP_LOGE(TAG, "Could not allocate the streaming model's variable tensor arena.");
      return false;
    }
    this->ma_ = tflite::MicroAllocator::Create(this->var_arena_, STREAMING_MODEL_VARIABLE_ARENA_SIZE);
    this->mrv_ = tflite::MicroResourceVariables::Create(this->ma_, 20);
  }

  const tflite::Model *model = tflite::GetModel(this->model_start_);
  if (model->version() != TFLITE_SCHEMA_VERSION) {
    ESP_LOGE(TAG, "Streaming model's schema is not supported");
    return false;
  }

  if (this->interpreter_ == nullptr) {
    this->interpreter_ = make_unique<tflite::MicroInterpreter>(
        tflite::GetModel(this->model_start_), op_resolver, this->tensor_arena_, this->tensor_arena_size_, this->mrv_);
    if (this->interpreter_->AllocateTensors() != kTfLiteOk) {
      ESP_LOGE(TAG, "Failed to allocate tensors for the streaming model");
      return false;
    }

    // Verify input tensor matches expected values
    // Dimension 3 will represent the first layer stride, so skip it may vary
    TfLiteTensor *input = this->interpreter_->input(0);
    if ((input->dims->size != 3) || (input->dims->data[0] != 1) ||
        (input->dims->data[2] != PREPROCESSOR_FEATURE_SIZE)) {
      ESP_LOGE(TAG, "Streaming model tensor input dimensions has improper dimensions.");
      return false;
    }

    if (input->type != kTfLiteInt8) {
      ESP_LOGE(TAG, "Streaming model tensor input is not int8.");
      return false;
    }

    // Verify output tensor matches expected values
    TfLiteTensor *output = this->interpreter_->output(0);
    if ((output->dims->size != 2) || (output->dims->data[0] != 1) || (output->dims->data[1] != 1)) {
      ESP_LOGE(TAG, "Streaming model tensor output dimension is not 1x1.");
    }

    if (output->type != kTfLiteUInt8) {
      ESP_LOGE(TAG, "Streaming model tensor output is not uint8.");
      return false;
    }
  }

  return true;
}

void StreamingModel::unload_model() {
  this->interpreter_.reset();

  ExternalRAMAllocator<uint8_t> arena_allocator(ExternalRAMAllocator<uint8_t>::ALLOW_FAILURE);

  arena_allocator.deallocate(this->tensor_arena_, this->tensor_arena_size_);
  this->tensor_arena_ = nullptr;
  arena_allocator.deallocate(this->var_arena_, STREAMING_MODEL_VARIABLE_ARENA_SIZE);
  this->var_arena_ = nullptr;
}

bool StreamingModel::perform_streaming_inference(const int8_t features[PREPROCESSOR_FEATURE_SIZE]) {
  if (this->interpreter_ != nullptr) {
    TfLiteTensor *input = this->interpreter_->input(0);

    std::memmove(
        (int8_t *) (tflite::GetTensorData<int8_t>(input)) + PREPROCESSOR_FEATURE_SIZE * this->current_stride_step_,
        features, PREPROCESSOR_FEATURE_SIZE);
    ++this->current_stride_step_;

    uint8_t stride = this->interpreter_->input(0)->dims->data[1];

    if (this->current_stride_step_ >= stride) {
      this->current_stride_step_ = 0;

      TfLiteStatus invoke_status = this->interpreter_->Invoke();
      if (invoke_status != kTfLiteOk) {
        ESP_LOGW(TAG, "Streaming interpreter invoke failed");
        return false;
      }

      TfLiteTensor *output = this->interpreter_->output(0);

      ++this->last_n_index_;
      if (this->last_n_index_ == this->sliding_window_size_)
        this->last_n_index_ = 0;
      this->recent_streaming_probabilities_[this->last_n_index_] = output->data.uint8[0];  // probability;
    }
    return true;
  }
  ESP_LOGE(TAG, "Streaming interpreter is not initialized.");
  return false;
}

void StreamingModel::reset_probabilities() {
  for (auto &prob : this->recent_streaming_probabilities_) {
    prob = 0;
  }
}

WakeWordModel::WakeWordModel(const uint8_t *model_start, float probability_cutoff, size_t sliding_window_average_size,
                             const std::string &wake_word, size_t tensor_arena_size) {
  this->model_start_ = model_start;
  this->probability_cutoff_ = probability_cutoff;
  this->sliding_window_size_ = sliding_window_average_size;
  this->recent_streaming_probabilities_.resize(sliding_window_average_size, 0);
  this->wake_word_ = wake_word;
  this->tensor_arena_size_ = tensor_arena_size;
};

bool WakeWordModel::determine_detected() {
  int32_t sum = 0;
  for (auto &prob : this->recent_streaming_probabilities_) {
    sum += prob;
  }

  float sliding_window_average = static_cast<float>(sum) / static_cast<float>(255 * this->sliding_window_size_);

  // Detect the wake word if the sliding window average is above the cutoff
  if (sliding_window_average > this->probability_cutoff_) {
    ESP_LOGD(TAG, "The '%s' model sliding average probability is %.3f and most recent probability is %.3f",
             this->wake_word_.c_str(), sliding_window_average,
             this->recent_streaming_probabilities_[this->last_n_index_] / (255.0));
    return true;
  }
  return false;
}

VADModel::VADModel(const uint8_t *model_start, float probability_cutoff, size_t sliding_window_size,
                   size_t tensor_arena_size) {
  this->model_start_ = model_start;
  this->probability_cutoff_ = probability_cutoff;
  this->sliding_window_size_ = sliding_window_size;
  this->recent_streaming_probabilities_.resize(sliding_window_size, 0);
  this->tensor_arena_size_ = tensor_arena_size;
};

bool VADModel::determine_detected() {
  uint8_t max = 0;
  for (auto &prob : this->recent_streaming_probabilities_) {
    max = std::max(prob, max);
  }

  return max > this->probability_cutoff_;
}

}  // namespace micro_wake_word
}  // namespace esphome

#endif
