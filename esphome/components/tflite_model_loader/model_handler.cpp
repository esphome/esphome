#include "model_handler.h"
#include "esphome/core/log.h"

namespace esphome::tflite_model_loader {

ESPHOME_LOG_TAG(TAG, "ModelHandler");

bool ModelHandler::load_model(const uint8_t *model_data, size_t model_size, const ModelConfig &config) {
  this->model_length_ = model_size;
  if (!this->loader_.load_model(model_data, model_size, config)) {
    return false;
  }
  this->config_ = this->loader_.config();
  this->output_size_ = this->loader_.output_size();
  this->tensor_arena_size_requested_ = this->loader_.tensor_arena_size_requested();
  return true;
}

void ModelHandler::unload() {
  this->loader_.unload();
  this->config_ = ModelConfig{};
  this->output_size_ = 0;
  this->tensor_arena_allocation_ = MemoryManager::AllocationResult{};
  this->tensor_arena_size_requested_ = 0;
}

bool ModelHandler::load_model_with_arena(const uint8_t *model_data, size_t model_size, uint8_t *tensor_arena,
                                         size_t tensor_arena_size, const ModelConfig &config,
                                         tflite::MicroResourceVariables *mrv) {
  this->model_length_ = model_size;
  if (!this->loader_.load_model_with_arena(model_data, model_size, tensor_arena, tensor_arena_size, config, mrv)) {
    return false;
  }
  this->config_ = config;
  this->output_size_ = this->loader_.output_size();
  return true;
}

ProcessedOutput ModelHandler::process_output(const float *output_data) const {
  return tflite_output_processor::process_output(output_data, this->output_size_, this->config_, this->debug_);
}

ProcessedOutput ModelHandler::process_output(TfLiteTensor *output_tensor) const {
  return tflite_output_processor::process_output(output_tensor, this->config_, this->debug_);
}

void ModelHandler::report_memory_status() {
  size_t effective_arena_size = this->loader_.get_tensor_arena_size();
  if (effective_arena_size == 0) {
    effective_arena_size = this->tensor_arena_size_requested_;
  }
  MemoryManager::report_memory_status(this->tensor_arena_size_requested_, effective_arena_size,
                                      this->get_arena_used_bytes(), this->model_length_);
}

}  // namespace esphome::tflite_model_loader
