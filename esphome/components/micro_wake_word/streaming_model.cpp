#include "streaming_model.h"

#ifdef USE_ESP_IDF

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

static const char *const TAG = "micro_wake_word";

namespace esphome {
namespace micro_wake_word {

void WakeWordModel::log_model_config() {
  ESP_LOGCONFIG(TAG, "    - Wake Word: %s", this->wake_word_.c_str());
  ESP_LOGCONFIG(TAG, "      Probability cutoff: %.2f", this->probability_cutoff_ / 255.0f);
  ESP_LOGCONFIG(TAG, "      Sliding window size: %d", this->sliding_window_size_);
}

void VADModel::log_model_config() {
  ESP_LOGCONFIG(TAG, "    - VAD Model");
  ESP_LOGCONFIG(TAG, "      Probability cutoff: %.2f", this->probability_cutoff_ / 255.0f);
  ESP_LOGCONFIG(TAG, "      Sliding window size: %d", this->sliding_window_size_);
}

bool StreamingModel::load_model_() {
  RAMAllocator<uint8_t> arena_allocator(RAMAllocator<uint8_t>::ALLOW_FAILURE);

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
    this->interpreter_ =
        make_unique<tflite::MicroInterpreter>(tflite::GetModel(this->model_start_), this->streaming_op_resolver_,
                                              this->tensor_arena_, this->tensor_arena_size_, this->mrv_);
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

  this->loaded_ = true;
  this->reset_probabilities();
  return true;
}

void StreamingModel::unload_model() {
  this->interpreter_.reset();

  RAMAllocator<uint8_t> arena_allocator(RAMAllocator<uint8_t>::ALLOW_FAILURE);

  if (this->tensor_arena_ != nullptr) {
    arena_allocator.deallocate(this->tensor_arena_, this->tensor_arena_size_);
    this->tensor_arena_ = nullptr;
  }

  if (this->var_arena_ != nullptr) {
    arena_allocator.deallocate(this->var_arena_, STREAMING_MODEL_VARIABLE_ARENA_SIZE);
    this->var_arena_ = nullptr;
  }

  this->loaded_ = false;
}

bool StreamingModel::perform_streaming_inference(const int8_t features[PREPROCESSOR_FEATURE_SIZE]) {
  if (this->enabled_ && !this->loaded_) {
    // Model is enabled but isn't loaded
    if (!this->load_model_()) {
      return false;
    }
  }

  if (!this->enabled_ && this->loaded_) {
    // Model is disabled but still loaded
    this->unload_model();
    return true;
  }

  if (this->loaded_) {
    TfLiteTensor *input = this->interpreter_->input(0);

    uint8_t stride = this->interpreter_->input(0)->dims->data[1];
    this->current_stride_step_ = this->current_stride_step_ % stride;

    std::memmove(
        (int8_t *) (tflite::GetTensorData<int8_t>(input)) + PREPROCESSOR_FEATURE_SIZE * this->current_stride_step_,
        features, PREPROCESSOR_FEATURE_SIZE);
    ++this->current_stride_step_;

    if (this->current_stride_step_ >= stride) {
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
      this->unprocessed_probability_status_ = true;
    }
    if (this->recent_streaming_probabilities_[this->last_n_index_] < this->probability_cutoff_) {
      // Only increment ignore windows if less than the probability cutoff; this forces the model to "cool-off" from a
      // previous detection and calling ``reset_probabilities`` so it avoids duplicate detections
      this->ignore_windows_ = std::min(this->ignore_windows_ + 1, 0);
    }
  }
  return true;
}

void StreamingModel::reset_probabilities() {
  for (auto &prob : this->recent_streaming_probabilities_) {
    prob = 0;
  }
  this->ignore_windows_ = -MIN_SLICES_BEFORE_DETECTION;
}

WakeWordModel::WakeWordModel(const std::string &id, const uint8_t *model_start, uint8_t default_probability_cutoff,
                             size_t sliding_window_average_size, const std::string &wake_word, size_t tensor_arena_size,
                             bool default_enabled, bool internal_only) {
  this->id_ = id;
  this->model_start_ = model_start;
  this->default_probability_cutoff_ = default_probability_cutoff;
  this->probability_cutoff_ = default_probability_cutoff;
  this->sliding_window_size_ = sliding_window_average_size;
  this->recent_streaming_probabilities_.resize(sliding_window_average_size, 0);
  this->wake_word_ = wake_word;
  this->tensor_arena_size_ = tensor_arena_size;
  this->register_streaming_ops_(this->streaming_op_resolver_);
  this->current_stride_step_ = 0;
  this->internal_only_ = internal_only;

  this->pref_ = global_preferences->make_preference<bool>(fnv1_hash(id));
  bool enabled;
  if (this->pref_.load(&enabled)) {
    // Use the enabled state loaded from flash
    this->enabled_ = enabled;
  } else {
    // If no state saved, then use the default
    this->enabled_ = default_enabled;
  }
};

void WakeWordModel::enable() {
  this->enabled_ = true;
  if (!this->internal_only_) {
    this->pref_.save(&this->enabled_);
  }
}

void WakeWordModel::disable() {
  this->enabled_ = false;
  if (!this->internal_only_) {
    this->pref_.save(&this->enabled_);
  }
}

DetectionEvent WakeWordModel::determine_detected() {
  DetectionEvent detection_event;
  detection_event.wake_word = &this->wake_word_;
  detection_event.max_probability = 0;
  detection_event.average_probability = 0;

  if ((this->ignore_windows_ < 0) || !this->enabled_) {
    detection_event.detected = false;
    return detection_event;
  }

  uint32_t sum = 0;
  for (auto &prob : this->recent_streaming_probabilities_) {
    detection_event.max_probability = std::max(detection_event.max_probability, prob);
    sum += prob;
  }

  detection_event.average_probability = sum / this->sliding_window_size_;
  detection_event.detected = sum > this->probability_cutoff_ * this->sliding_window_size_;

  this->unprocessed_probability_status_ = false;
  return detection_event;
}

VADModel::VADModel(const uint8_t *model_start, uint8_t default_probability_cutoff, size_t sliding_window_size,
                   size_t tensor_arena_size) {
  this->model_start_ = model_start;
  this->default_probability_cutoff_ = default_probability_cutoff;
  this->probability_cutoff_ = default_probability_cutoff;
  this->sliding_window_size_ = sliding_window_size;
  this->recent_streaming_probabilities_.resize(sliding_window_size, 0);
  this->tensor_arena_size_ = tensor_arena_size;
  this->register_streaming_ops_(this->streaming_op_resolver_);
}

DetectionEvent VADModel::determine_detected() {
  DetectionEvent detection_event;
  detection_event.max_probability = 0;
  detection_event.average_probability = 0;

  if (!this->enabled_) {
    // We disabled the VAD model for some reason... so we shouldn't block wake words from being detected
    detection_event.detected = true;
    return detection_event;
  }

  uint32_t sum = 0;
  for (auto &prob : this->recent_streaming_probabilities_) {
    detection_event.max_probability = std::max(detection_event.max_probability, prob);
    sum += prob;
  }

  detection_event.average_probability = sum / this->sliding_window_size_;
  detection_event.detected = sum > (this->probability_cutoff_ * this->sliding_window_size_);

  return detection_event;
}

bool StreamingModel::register_streaming_ops_(tflite::MicroMutableOpResolver<20> &op_resolver) {
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

#endif
