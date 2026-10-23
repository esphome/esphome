#include "model_handler.h"

#ifdef USE_TFLITE_MICRO_HELPER

namespace esphome::tflite_model_loader {

bool ModelHandler::load_model(const uint8_t *model_data, size_t model_size, const ModelConfig &config) {
  return this->loader_.load_model(model_data, model_size, config);
}

void ModelHandler::unload() { this->loader_.unload(); }

bool ModelHandler::load_model_with_arena(const uint8_t *model_data, size_t model_size, uint8_t *tensor_arena,
                                         size_t tensor_arena_size, const ModelConfig &config,
                                         tflite::MicroResourceVariables *mrv) {
  return this->loader_.load_model_with_arena(model_data, model_size, tensor_arena, tensor_arena_size, config, mrv);
}

ProcessedOutput ModelHandler::process_output(const float *output_data) const {
  const ModelConfig &config = this->loader_.config();
  return tflite_output_processor::process_output(output_data, this->loader_.output_size(), config.output_processing,
                                                 config.scale_factor, this->debug_);
}

ProcessedOutput ModelHandler::process_output(TfLiteTensor *output_tensor) const {
  const ModelConfig &config = this->loader_.config();
  return tflite_output_processor::process_output(output_tensor, config.output_processing, config.scale_factor,
                                                 this->debug_);
}

}  // namespace esphome::tflite_model_loader

#endif  // USE_TFLITE_MICRO_HELPER
