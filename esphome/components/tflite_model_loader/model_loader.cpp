#include "model_loader.h"

#ifdef USE_TFLITE_MICRO_HELPER
#include <cinttypes>
#include <set>
#include "esphome/core/log.h"
#include "flatbuffers/verifier.h"
#include "tensorflow/lite/schema/schema_utils.h"

namespace esphome::tflite_model_loader {

ESPHOME_LOG_TAG(TAG, "tflite_model_loader");

void ModelLoader::release_model_() {
  this->interpreter_.reset();
  this->resolver_.reset();
  this->tflite_model_ = nullptr;
  this->model_length_ = 0;
  this->tensor_arena_ = nullptr;
  this->tensor_arena_size_ = 0;
  this->set_config(ModelConfig{});
  this->output_size_ = 0;
}

void ModelLoader::unload() {
  const bool was_loaded = this->interpreter_ != nullptr || this->tensor_arena_allocation_;
  this->release_model_();
  this->tensor_arena_allocation_.reset();
  if (was_loaded) {
    ESP_LOGI(TAG, "Model unloaded and memory freed");
  }
}

bool ModelLoader::load_model(const uint8_t *model_data, size_t model_size, const ModelConfig &config) {
  // Use the configured arena size, or fall back to the default
  if (this->tensor_arena_size_requested_ == 0) {
    this->tensor_arena_size_requested_ = DEFAULT_TENSOR_ARENA_SIZE;
    ESP_LOGW(TAG, "Using default tensor arena size: %zu bytes", DEFAULT_TENSOR_ARENA_SIZE);
  }

  // Drop the previous model first: its interpreter points into the arena being replaced.
  this->unload();

  this->tensor_arena_allocation_ =
      tflite_memory_handler::MemoryManager::allocate_tensor_arena(this->tensor_arena_size_requested_);
  if (!this->tensor_arena_allocation_) {
    ESP_LOGE(TAG, "Failed to allocate tensor arena");
    return false;
  }

  if (!this->load_model_with_arena(model_data, model_size, this->tensor_arena_allocation_.data.get(),
                                   this->tensor_arena_allocation_.actual_size, config)) {
    this->unload();
    return false;
  }
  return true;
}

bool ModelLoader::load_model_with_arena(const uint8_t *model_data, size_t model_size, uint8_t *tensor_arena,
                                        size_t tensor_arena_size, const ModelConfig &config,
                                        tflite::MicroResourceVariables *mrv) {
  if (!this->load_with_arena_(model_data, model_size, tensor_arena, tensor_arena_size, config, mrv)) {
    // The arena belongs to the caller, so only the model state is dropped here.
    this->release_model_();
    return false;
  }
  return true;
}

bool ModelLoader::load_with_arena_(const uint8_t *model_data, size_t model_size, uint8_t *tensor_arena,
                                   size_t tensor_arena_size, const ModelConfig &config,
                                   tflite::MicroResourceVariables *mrv) {
  // A previously loaded model must not keep pointing into an arena that is being replaced.
  this->release_model_();

  if (this->debug_) {
    ESP_LOGD(TAG, "Loading model with config:");
    ESP_LOGD(TAG, "  Description: %s", config.description.c_str());
    ESP_LOGD(TAG, "  Output processing: %s", config.output_processing.c_str());
    ESP_LOGD(TAG, "  Model data pointer: %p (%zu bytes)", model_data, model_size);
    ESP_LOGD(TAG, "  Tensor arena: %p (%zu bytes)", tensor_arena, tensor_arena_size);
  }

  if (model_data == nullptr) {
    ESP_LOGE(TAG, "Model data pointer is NULL!");
    return false;
  }
  if (model_size == 0) {
    ESP_LOGE(TAG, "Model data size is 0!");
    return false;
  }
  if (tensor_arena == nullptr) {
    ESP_LOGE(TAG, "Tensor arena pointer is NULL!");
    return false;
  }
  if (tensor_arena_size == 0) {
    ESP_LOGE(TAG, "Tensor arena size is 0!");
    return false;
  }

#ifdef DEBUG_TFLITE_MICRO_HELPER
  // A .tflite file is a 4-byte offset followed by the "TFL3" file identifier.
  if (model_size < 8 || model_data[4] != 'T' || model_data[5] != 'F' || model_data[6] != 'L' || model_data[7] != '3') {
    ESP_LOGE(TAG, "Model does not start with the TFL3 file identifier");
    return false;
  }
#endif

  // Validate the buffer before anything dereferences it: GetModel() trusts the offset it reads.
  flatbuffers::Verifier verifier(model_data, model_size);
  if (!tflite::VerifyModelBuffer(verifier)) {
    ESP_LOGE(TAG, "Model buffer verification failed (%zu bytes)", model_size);
    return false;
  }

  const tflite::Model *model = tflite::GetModel(model_data);
  if (model->version() != TFLITE_SCHEMA_VERSION) {
    ESP_LOGE(TAG, "Model schema version mismatch: model has %" PRIu32 ", expecting %d", model->version(),
             TFLITE_SCHEMA_VERSION);
    return false;
  }

  // These fields are optional in the schema, so they need a null check before use.
  const auto *operator_codes = model->operator_codes();
  const auto *subgraphs = model->subgraphs();
  if (operator_codes == nullptr || subgraphs == nullptr || subgraphs->size() == 0) {
    ESP_LOGE(TAG, "Model has no operator codes or subgraphs");
    return false;
  }
  const auto *subgraph = subgraphs->Get(0);
  if (subgraph == nullptr || subgraph->operators() == nullptr || subgraph->operators()->size() == 0) {
    ESP_LOGE(TAG, "Model has no operators");
    return false;
  }

  // Collect the operators once: the same set drives registration and the failure diagnostics.
  // GetBuiltinCode() is used because builtin_code() still reports PLACEHOLDER for models
  // converted with the newer schema.
  std::set<tflite::BuiltinOperator> required_ops;
  bool has_delegate = false;
  for (uint32_t i = 0; i < operator_codes->size(); i++) {
    const auto *op_code = operator_codes->Get(i);
    if (op_code == nullptr)
      continue;
    const tflite::BuiltinOperator builtin_code = tflite::GetBuiltinCode(op_code);
    required_ops.insert(builtin_code);
    has_delegate = has_delegate || builtin_code == tflite::BuiltinOperator_DELEGATE;
    if (this->debug_) {
      ESP_LOGD(TAG, "  Op code [%" PRIu32 "]: %d (%s)", i, builtin_code, tflite::EnumNameBuiltinOperator(builtin_code));
    }
  }

  if (has_delegate) {
    // TFLite Micro has no delegate support, so registration below cannot succeed.
    ESP_LOGE(TAG, "Model contains a DELEGATE operator, which TFLite Micro does not support");
    ESP_LOGE(TAG, "Re-export the model with the XNNPACK delegate disabled");
    return false;
  }

  this->tflite_model_ = model;
  this->model_length_ = model_size;
  this->tensor_arena_ = tensor_arena;
  this->tensor_arena_size_ = tensor_arena_size;

  ESP_LOGI(TAG, "Loading model (%zu bytes, arena %zu bytes)", model_size, tensor_arena_size);

  // Reset and allocate fresh resolver for this load cycle
  this->resolver_ = std::make_unique<tflite::MicroMutableOpResolver<TFLITE_MICRO_MAX_OPERATORS>>();
  if (!tflite_op_processor::OpResolverManager::register_ops<TFLITE_MICRO_MAX_OPERATORS>(*this->resolver_, required_ops,
                                                                                        TAG)) {
    ESP_LOGE(TAG, "Failed to register the model operators");
    return false;
  }

  // Conditional interpreter construction: with MRV for streaming, without for standard
  if (mrv != nullptr) {
    this->interpreter_ =
        std::make_unique<tflite::MicroInterpreter>(model, *this->resolver_, tensor_arena, tensor_arena_size, mrv);
  } else {
    this->interpreter_ =
        std::make_unique<tflite::MicroInterpreter>(model, *this->resolver_, tensor_arena, tensor_arena_size);
  }

  if (this->interpreter_->AllocateTensors() != kTfLiteOk) {
    ESP_LOGE(TAG, "Failed to allocate tensors: %zu byte arena for a %zu byte model (requested: %zu)", tensor_arena_size,
             model_size, this->tensor_arena_size_requested_);
    ESP_LOGE(TAG, "  Operators: %" PRIu32 ", operator codes: %" PRIu32, subgraph->operators()->size(),
             operator_codes->size());
#ifdef DEBUG_TFLITE_MICRO_HELPER
    ESP_LOGE(TAG, "  Arena used before failure: %zu bytes", this->interpreter_->arena_used_bytes());
#endif
    return false;
  }

  if (this->debug_) {
    const TfLiteTensor *input = this->input_tensor();
    if (input != nullptr) {
      for (int i = 0; i < input->dims->size; i++) {
        ESP_LOGD(TAG, "  Input dim %d: %d", i, input->dims->data[i]);
      }
    }
  }

  // Cache the output element count so process_output() does not recompute it per inference.
  if (const TfLiteTensor *output = this->output_tensor(); output != nullptr) {
    int size = 1;
    for (int i = 0; i < output->dims->size; i++)
      size *= output->dims->data[i];
    this->output_size_ = size;
  }

  // Committed last, so a failed load never leaves a stale config behind. This also
  // parses the output processing mode once, for the whole life of the model.
  this->set_config(config);

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
