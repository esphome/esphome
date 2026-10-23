#include "model_loader.h"

#ifdef USE_TFLITE_MICRO_HELPER
#include "esphome/core/log.h"
#include <set>

namespace esphome::tflite_model_loader {

ESPHOME_LOG_TAG(TAG, "tflite_model_loader");

void ModelLoader::unload() {
  this->interpreter_.reset();
  this->tensor_arena_allocation_.data.reset();
  this->tensor_arena_allocation_.actual_size = 0;
  this->tensor_arena_size_requested_ = 0;

  // Reset state
  this->config_ = ModelConfig{};
  this->output_size_ = 0;
  this->tflite_model_ = nullptr;
  this->model_length_ = 0;
  this->resolver_.reset();

  ESP_LOGI(TAG, "Model unloaded and memory freed");
}

bool ModelLoader::load_model(const uint8_t *model_data, size_t model_size, const ModelConfig &config) {
  this->model_length_ = model_size;

  // Use configured arena size, or fall back to default
  if (this->tensor_arena_size_requested_ == 0) {
    this->tensor_arena_size_requested_ = 100 * 1024;  // Default 100KB
    ESP_LOGW(TAG, "Using default tensor arena size: 100KB");
  }

  // Allocate tensor arena
  this->tensor_arena_allocation_ =
      tflite_memory_handler::MemoryManager::allocate_tensor_arena(this->tensor_arena_size_requested_);
  if (!this->tensor_arena_allocation_) {
    ESP_LOGE(TAG, "Failed to allocate tensor arena");
    return false;
  }

  // Load the model using the allocated arena
  return this->load_model_with_arena(model_data, model_size, this->tensor_arena_allocation_.data.get(),
                                     this->tensor_arena_allocation_.actual_size, config);
}

bool ModelLoader::load_model_with_arena(const uint8_t *model_data, size_t model_size, uint8_t *tensor_arena,
                                        size_t tensor_arena_size, const ModelConfig &config,
                                        tflite::MicroResourceVariables *mrv) {
  if (this->debug_) {
    ESP_LOGD(TAG, "Loading model with config:");
    ESP_LOGD(TAG, "  Description: %s", config.description.c_str());
    ESP_LOGD(TAG, "  Output processing: %s", config.output_processing.c_str());
    ESP_LOGD(TAG, "Model data validation:");
    ESP_LOGD(TAG, "  Model data pointer: %p", model_data);
    ESP_LOGD(TAG, "  Model data size: %zu bytes", model_size);
  }

  if (model_data == nullptr) {
    ESP_LOGE(TAG, "Model data pointer is NULL!");
    return false;
  }

  if (model_size == 0) {
    ESP_LOGE(TAG, "Model data size is 0!");
    return false;
  }

#ifdef DEBUG_TFLITE_MICRO_HELPER
  // Check TFLite magic number
  if (model_size >= 8) {
    ESP_LOGI(TAG, "First 8 bytes: %02X %02X %02X %02X %02X %02X %02X %02X", model_data[0], model_data[1], model_data[2],
             model_data[3], model_data[4], model_data[5], model_data[6], model_data[7]);

    // TFLite magic number should be: XX 00 00 00 54 46 4C 33
    if (model_data[1] == 0x00 && model_data[2] == 0x00 && model_data[3] == 0x00 && model_data[4] == 0x54 &&
        model_data[5] == 0x46 && model_data[6] == 0x4C && model_data[7] == 0x33) {
      ESP_LOGI(TAG, "Valid TFLite magic number found (version byte: 0x%02X)", model_data[0]);
    } else {
      ESP_LOGE(TAG, "Invalid TFLite magic number");
      // Don't return false here if we want to try anyway, but legacy returned matches this block
      return false;
    }
  }
#endif

  this->config_ = config;

  // For PROGMEM data, we need to handle it specially
  ESP_LOGI(TAG, "Loading model from PROGMEM (%zu bytes)", model_size);

  this->tflite_model_ = tflite::GetModel(model_data);

  // Note: If GetModel fails with PROGMEM data, the model may need to be
  // copied to RAM first. This is rare -- most PROGMEM data works with GetModel.
  // If you encounter issues, ensure the model data is properly aligned.

  if (this->tflite_model_ == nullptr) {
    ESP_LOGE(TAG, "Failed to parse model - invalid data");
    return false;
  }

  if (this->tflite_model_->version() != TFLITE_SCHEMA_VERSION) {
    ESP_LOGE(TAG, "Model schema version mismatch: Model has %d, Expecting %d", this->tflite_model_->version(),
             TFLITE_SCHEMA_VERSION);

    // Diagnostic dump to identify corrupted files
    if (model_data != nullptr && model_size >= 16) {
      ESP_LOGE(TAG, "Model Header Dump (First 16 bytes):");
      ESP_LOGE(TAG, "  %02X %02X %02X %02X %02X %02X %02X %02X  %02X %02X %02X %02X %02X %02X %02X %02X", model_data[0],
               model_data[1], model_data[2], model_data[3], model_data[4], model_data[5], model_data[6], model_data[7],
               model_data[8], model_data[9], model_data[10], model_data[11], model_data[12], model_data[13],
               model_data[14], model_data[15]);

      // Check magic number TFL3 (starts at offset 4)
      if (model_data[4] == 'T' && model_data[5] == 'F' && model_data[6] == 'L' && model_data[7] == '3') {
        ESP_LOGW(TAG, "Magic number 'TFL3' is PRESENT. Issue might be alignment or genuine schema mismatch.");
      } else {
        ESP_LOGE(TAG, "Magic number 'TFL3' is MISSING! The file is likely corrupted or not a valid TFLite model.");
      }
    } else {
      ESP_LOGE(TAG, "Unable to dump header: Data null or too small (%zu bytes)", model_size);
    }
    return false;
  }

  // Reset and allocate fresh resolver for this load cycle
  this->resolver_ = std::make_unique<tflite::MicroMutableOpResolver<TFLITE_MICRO_MAX_OPERATORS>>();

  if (this->debug_) {
    ESP_LOGD(TAG, "Operator codes found in model:");
    for (size_t i = 0; i < this->tflite_model_->operator_codes()->size(); ++i) {
      const auto *op_code = this->tflite_model_->operator_codes()->Get(i);
      ESP_LOGD(TAG, "  [%d]: %d (%s)", static_cast<int>(i), op_code->builtin_code(),
               tflite::EnumNameBuiltinOperator(op_code->builtin_code()));
    }
  }

  std::set<tflite::BuiltinOperator> required_ops;

  for (size_t i = 0; i < this->tflite_model_->operator_codes()->size(); ++i) {
    const auto *op_code = this->tflite_model_->operator_codes()->Get(i);
    required_ops.insert(op_code->builtin_code());
  }

  if (!tflite_op_processor::OpResolverManager::register_ops<TFLITE_MICRO_MAX_OPERATORS>(*this->resolver_, required_ops, TAG)) {
    ESP_LOGE(TAG, "Failed to register operators");
    return false;
  }

  // Conditional interpreter construction: with MRV for streaming, without for standard
  if (mrv != nullptr) {
    this->interpreter_ = std::make_unique<tflite::MicroInterpreter>(this->tflite_model_, *this->resolver_, tensor_arena,
                                                                    tensor_arena_size, mrv);
  } else {
    this->interpreter_ = std::make_unique<tflite::MicroInterpreter>(this->tflite_model_, *this->resolver_, tensor_arena,
                                                                    tensor_arena_size);
  }

  if (this->interpreter_->AllocateTensors() != kTfLiteOk) {
    ESP_LOGE(TAG, "Failed to allocate tensors");
    ESP_LOGE(TAG, "  Tensor arena: %zu bytes (requested: %zu)", tensor_arena_size, this->tensor_arena_size_requested_);
    ESP_LOGE(TAG, "  Model size: %zu bytes", this->model_length_);
    ESP_LOGE(TAG, "  Model operators: %zu", this->tflite_model_->subgraphs()->Get(0)->operators()->size());
    ESP_LOGE(TAG, "  Model operator codes: %zu", this->tflite_model_->operator_codes()->size());

    // Log all operator codes in the model for debugging
    for (size_t i = 0; i < this->tflite_model_->operator_codes()->size(); ++i) {
      const auto *op_code = this->tflite_model_->operator_codes()->Get(i);
      ESP_LOGE(TAG, "  Op code [%zu]: builtin_code=%d (%s)", i, op_code->builtin_code(),
               tflite::EnumNameBuiltinOperator(op_code->builtin_code()));
    }

    // Check for DELEGATE op which is not supported by TFLite Micro
    for (size_t i = 0; i < this->tflite_model_->operator_codes()->size(); ++i) {
      const auto *op_code = this->tflite_model_->operator_codes()->Get(i);
      if (op_code->builtin_code() == tflite::BuiltinOperator_DELEGATE) {
        ESP_LOGE(TAG, "  *** WARNING: Model contains DELEGATE operator (index %zu)! ***", i);
        ESP_LOGE(TAG, "  *** TFLite Micro does NOT support delegates. ***");
        ESP_LOGE(TAG, "  *** The model was likely exported with XNNPACK delegate enabled. ***");
        ESP_LOGE(TAG, "  *** Re-export the model with --no-xnnpack or disable delegates. ***");
      }
    }

// Log arena usage info if available
#ifdef DEBUG_TFLITE_MICRO_HELPER
    size_t arena_used = this->interpreter_->arena_used_bytes();
    ESP_LOGE(TAG, "  Arena used before failure: %zu bytes", arena_used);
#endif

    return false;
  }

  if (this->tflite_model_->subgraphs()->Get(0)->operators()->size() == 0) {
    ESP_LOGE(TAG, "Model has no operators!");
    return false;
  }

  auto *input = this->input_tensor();
  if (input) {
    ESP_LOGI(TAG, "Input tensor dimensions:");
    for (int i = 0; i < input->dims->size; i++) {
      ESP_LOGI(TAG, "  Dim %d: %d", i, input->dims->data[i]);
    }
  }

  // Calculate output size from tensor dimensions
  TfLiteTensor *output = this->output_tensor();
  if (output) {
    int size = 1;
    for (int i = 0; i < output->dims->size; i++) {
      size *= output->dims->data[i];
    }
    this->output_size_ = size;

    // Note: output_processing is always set by __init__.py (defaults to "direct_class"),
    // so the auto-detection branch (when output_processing is empty) is never reached
    // at runtime. If load_model_with_arena() is called directly, ensure output_processing
    // is set explicitly in the ModelConfig before calling.
  }

  ESP_LOGI(TAG, "Model loaded successfully");

  return true;
}

TfLiteTensor *ModelLoader::input_tensor() {
  if (!this->is_loaded())
    return nullptr;
  return this->interpreter_->input(0);
}

const TfLiteTensor *ModelLoader::input_tensor() const {
  if (!this->is_loaded())
    return nullptr;
  return this->interpreter_->input(0);
}

TfLiteTensor *ModelLoader::output_tensor() {
  if (!this->is_loaded())
    return nullptr;
  return this->interpreter_->output(0);
}

const TfLiteTensor *ModelLoader::output_tensor() const {
  if (!this->is_loaded())
    return nullptr;
  return this->interpreter_->output(0);
}

TfLiteStatus ModelLoader::invoke() {
  if (!this->is_loaded())
    return kTfLiteError;
  return this->interpreter_->Invoke();
}

size_t ModelLoader::get_arena_used_bytes() const {
  if (!this->is_loaded())
    return 0;
  return this->interpreter_->arena_used_bytes();
}

}  // namespace esphome::tflite_model_loader

#endif  // USE_TFLITE_MICRO_HELPER
