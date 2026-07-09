#pragma once

#include "model_handler.h"
#include "memory_manager.h"
#include "esphome/core/log.h"

#include "esphome/core/defines.h"

#include <atomic>
#include <mutex>

#ifdef USE_TFLITE_MICRO_HELPER

namespace esphome {
namespace tflite_micro_helper {

/**
 * @struct ModelSpec
 * @brief Generic model dimensions and input spec for any TFLite consumer.
 */
struct ModelSpec {
  int input_width{0};
  int input_height{0};
  int input_channels{0};
  bool normalize{false};
  std::string input_order{"RGB"};
  int input_type{0};  // 0=UINT8, 1=FLOAT
};

/**
 * @struct ArenaStats
 * @brief Tensor arena utilization statistics.
 */
struct ArenaStats {
  size_t total_size{0};
  size_t used_bytes{0};
  size_t wasted_bytes{0};
  float efficiency{0.0f};
};

/**
 * @class TFLiteMicroHelper
 * @brief Reusable TFLite Micro component for ESPHome.
 *
 * Wraps model loading, tensor arena management, config building,
 * and inference invocation. Consumer components set config via setters
 * then call load_model() / run_inference().
 */
class TFLiteMicroHelper {
 public:
  TFLiteMicroHelper() = default;
  ~TFLiteMicroHelper() = default;

  // -- Model Config Setters -------------------------------------------
  void set_model_type(const std::string &t) { this->model_type_ = t; }
  void set_tensor_arena_size(size_t size);
  void set_model(const uint8_t *model_data, size_t model_size);

  void set_input_type(const std::string &t) { this->input_type_ = t; }
  void set_input_channels(int c) { this->input_channels_ = c; }
  void set_input_width(int w) { this->input_width_ = w; }
  void set_input_height(int h) { this->input_height_ = h; }
  void set_output_processing(const std::string &p) { this->output_processing_ = p; }
  void set_scale_factor(float f) { this->scale_factor_ = f; }
  void set_input_order(const std::string &o) { this->input_order_ = o; }
  void set_normalize(bool n) { this->normalize_ = n; }
  void set_invert(bool i) { this->invert_ = i; }

  // -- Lifecycle ------------------------------------------------------
  bool load_model();
  void unload_model();
  bool is_model_loaded() const { return this->model_loaded_.load(); }

  // -- Inference ------------------------------------------------------
  TfLiteStatus invoke() { return this->model_handler_.invoke(); }

  TfLiteTensor *input_tensor() { return this->model_handler_.input_tensor(); }
  const TfLiteTensor *input_tensor() const { return this->model_handler_.input_tensor(); }
  TfLiteTensor *output_tensor() { return this->model_handler_.output_tensor(); }

  /**
   * @brief Copy a preprocessed buffer into the input tensor and invoke.
   * @param src_data Source buffer (from image processor)
   * @param src_size Buffer size in bytes
   * @return true if inference succeeded
   */
  bool run_inference_on_buffer(const uint8_t *src_data, size_t src_size);

  /**
   * @brief Convenience: run inference and get processed output.
   * Returns ProcessedOutput.value / .confidence.
   */
  ProcessedOutput run_inference(const uint8_t *src_data, size_t src_size);

  // -- Accessors -----------------------------------------------------
  ModelSpec get_model_spec() const;

  int get_input_width() const { return this->model_handler_.get_input_width(); }
  int get_input_height() const { return this->model_handler_.get_input_height(); }
  int get_input_channels() const { return this->model_handler_.get_input_channels(); }
  const ModelConfig &get_config() const { return this->model_handler_.get_config(); }

  // -- Memory ---------------------------------------------------------
  size_t get_arena_used_bytes() const { return this->model_handler_.get_arena_used_bytes(); }
  size_t get_tensor_arena_size() const { return this->tensor_arena_size_requested_; }
  size_t get_tensor_arena_size_actual() const { return this->tensor_arena_allocation_.actual_size; }
  size_t get_model_size_bytes() const { return this->model_length_; }

  ArenaStats get_arena_stats() const;
  void update_arena_stats_cache();
  void report_memory_status();

  // -- Debug ----------------------------------------------------------
  void set_debug(bool debug) {
    this->debug_ = debug;
    this->model_handler_.set_debug(debug);
  }

#ifdef DEBUG_TFLITE_MICRO_HELPER
  void debug_test_parameters(const std::vector<std::vector<uint8_t>> &zone_data) {
    this->model_handler_.debug_test_parameters(zone_data);
  }
#endif

 private:
  // Model config (set from __init__.py)
  std::string model_type_{"default"};
  std::string input_type_{"uint8"};
  int input_channels_{3};
  int input_width_{32};
  int input_height_{20};
  std::string output_processing_{"direct_class"};
  float scale_factor_{1.0f};
  std::string input_order_{"RGB"};
  bool normalize_{false};
  bool invert_{false};
  bool debug_{false};

  // Model data (set via set_model)
  const uint8_t *model_data_{nullptr};
  size_t model_length_{0};
  size_t tensor_arena_size_requested_{100 * 1024};
  bool arena_bumped_{false};  // ESP32-S3: 1.5x bump applied once

  // Components
  ModelHandler model_handler_;
  MemoryManager::AllocationResult tensor_arena_allocation_;

  // Arena stats cache (thread-safe for dual-core)
  mutable std::mutex arena_stats_mutex_;
  ArenaStats cached_arena_stats_{};

  // State
  std::atomic<bool> model_loaded_{false};

  // Internals
  bool allocate_tensor_arena_();
  ModelConfig build_config_();
  bool validate_input_tensor_(const uint8_t *src_data, size_t src_size) const;
};

}  // namespace tflite_micro_helper
}  // namespace esphome

#endif  // USE_TFLITE_MICRO_HELPER
