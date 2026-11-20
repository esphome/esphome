#include "analog_gauge_reader.h"

#include "esphome/components/camera_decoder/jpeg_grayscale.h"
#include "esphome/core/application.h"
#include "esphome/core/entity_base.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <numeric>  // std::iota
#include <vector>

#ifdef USE_ESP32
#include <esp_heap_caps.h>
#endif

namespace esphome {
namespace analog_gauge_reader {

ESPHOME_LOG_TAG(TAG, "analog_gauge_reader");

static constexpr float DECIMAL_PRECISION = 10000.0f;  // 4 decimal places (10^4)

void AnalogGaugeReader::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Analog Gauge Reader...");
  this->camera_->add_listener(this);

  // Sort calibration mappings for correct interpolation
  for (auto &dial : this->dials_) {
    if (!dial.calibration_mapping.empty()) {
      std::sort(dial.calibration_mapping.begin(), dial.calibration_mapping.end(),
                [](const std::pair<float, float> &a, const std::pair<float, float> &b) { return a.first < b.first; });
      ESP_LOGD(TAG, "Sorted calibration mapping for dial %s (%d points)", dial.id,
               static_cast<int>(dial.calibration_mapping.size()));
      if (dial.min_angle > dial.max_angle) {
        ESP_LOGW(TAG,
                 "Dial %s has BOTH calibration_mapping and a wrap-around angle range (min_angle > max_angle). "
                 "calibration_mapping takes precedence; the wrap-around branch is skipped for this dial.",
                 dial.id);
      }
    }
  }

  size_t max_crop_area = 0;
  int max_crop_w = 0;
  int max_crop_h = 0;
  for (const auto &dial : this->dials_) {
    max_crop_area = std::max(max_crop_area, static_cast<size_t>(dial.crop_w) * static_cast<size_t>(dial.crop_h));
    max_crop_w = std::max(max_crop_w, dial.crop_w);
    max_crop_h = std::max(max_crop_h, dial.crop_h);
  }
  if (max_crop_area > 0) {
    this->scratch_buffer_.reserve(max_crop_area);
    this->scratch_buffer_2_.reserve(max_crop_area);
    this->detector_.reserve(max_crop_w, max_crop_h);
    ESP_LOGD(TAG, "Reserved scratch buffers for max area: %d pixels", static_cast<int>(max_crop_area));
  }

  this->dial_buffers_.clear();
  this->dial_buffers_.reserve(this->dials_.size());
  for (const auto &dial : this->dials_) {
    DialBuffer buffer;
    buffer.channels = (dial.use_color || dial.process_channel != PROCESS_CHANNEL_GRAYSCALE) ? 3 : 1;
    buffer.size = static_cast<size_t>(dial.crop_w) * static_cast<size_t>(dial.crop_h) * buffer.channels;
    buffer.data = std::make_unique<uint8_t[]>(buffer.size);
    this->dial_buffers_.push_back(std::move(buffer));
  }

  const size_t dial_count = this->dials_.size();
  this->readings_.reserve(dial_count);
  this->digits_.resize(dial_count);
  this->phase_.resize(dial_count);
  this->order_.resize(dial_count);

  if (heap_caps_get_free_size(MALLOC_CAP_SPIRAM) == 0) {
    ESP_LOGW(TAG, "No PSRAM detected; the decoder may not be able to allocate the decoded frame");
  }
}

void AnalogGaugeReader::dump_config() {
  ESP_LOGCONFIG(TAG, "Analog Gauge Reader:");
  ESP_LOGCONFIG(TAG, "  Frame: %dx%d", this->img_width_, this->img_height_);
  ESP_LOGCONFIG(TAG, "  Debug: %s", this->debug_ ? LOG_STR_LITERAL("YES") : LOG_STR_LITERAL("NO"));
  ESP_LOGCONFIG(TAG, "  Update Interval: %" PRIu32 " ms", this->get_update_interval());
  ESP_LOGCONFIG(TAG, "  Stacked Digits: %s", this->stacked_digits_ ? LOG_STR_LITERAL("YES") : LOG_STR_LITERAL("NO"));
  for (const auto &dial : this->dials_) {
    ESP_LOGCONFIG(TAG,
                  "  Dial '%s': Algorithm=%s, Scale=%.3f, Crop=[%d,%d,%d,%d], AutoContrast=%s, Contrast=%.2f, "
                  "Deadzone=%.1fpx",
                  dial.id, LOG_STR_ARG(gauge_needle_detector::algorithm_name(dial.algorithm)), dial.scale, dial.crop_x,
                  dial.crop_y, dial.crop_w, dial.crop_h,
                  dial.auto_contrast ? LOG_STR_LITERAL("ON") : LOG_STR_LITERAL("OFF"), dial.contrast,
                  dial.deadzone_diameter);
  }
  this->decoder_->dump_config();
}

void AnalogGaugeReader::update() {
  if (this->debug_) {
    ESP_LOGD(TAG, "Update triggered (Interval cycle)");
  }

  if (this->paused_) {
    return;
  }
  FrameState expected = FrameState::IDLE;
  if (this->frame_state_.compare_exchange_strong(expected, FrameState::REQUESTED)) {
    this->last_request_time_ = App.get_loop_component_start_time();
    this->camera_->request_image(camera::IDLE);
    ESP_LOGD(TAG, "Requesting frame");
  }
}

void AnalogGaugeReader::on_camera_image(const std::shared_ptr<esphome::camera::CameraImage> &image) {
  if (this->paused_)
    return;
  FrameState expected = FrameState::REQUESTED;
  if (this->frame_state_.compare_exchange_strong(expected, FrameState::AVAILABLE)) {
    this->pending_frame_ = image;
  }
}

void AnalogGaugeReader::loop() {
  // The camera calls its listeners from the main loop, so no locking is needed.
  std::shared_ptr<esphome::camera::CameraImage> frame = std::move(this->pending_frame_);
  if (frame) {
    this->frame_state_.store(FrameState::PROCESSING);
    if (frame->get_data_length() > 0) {
      this->process_image_from_buffer(frame->get_data_buffer(), frame->get_data_length());
    }
    frame.reset();
    this->frame_state_.store(FrameState::IDLE);
  }

  // Watchdog: a requested frame that never arrives must not block later polls.
  const uint32_t now = App.get_loop_component_start_time();
  if (this->frame_state_.load() == FrameState::REQUESTED && now - this->last_request_time_ > 5000) {
    ESP_LOGW(TAG, "Frame timeout");
    FrameState expected = FrameState::REQUESTED;
    if (this->frame_state_.compare_exchange_strong(expected, FrameState::IDLE)) {
      this->pending_frame_ = nullptr;
    }
  }
}

static void apply_auto_contrast(uint8_t *data, int size) {
  uint8_t min_val = 255;
  uint8_t max_val = 0;
  for (int i = 0; i < size; i++) {
    if (data[i] < min_val)
      min_val = data[i];
    if (data[i] > max_val)
      max_val = data[i];
  }
  if (max_val > min_val) {
    float scale = 255.0f / static_cast<float>(max_val - min_val);
    for (int i = 0; i < size; i++) {
      data[i] = static_cast<uint8_t>(static_cast<float>(data[i] - min_val) * scale);
    }
  }
}

static void apply_contrast(uint8_t *data, int size, float contrast) {
  if (std::abs(contrast - 1.0f) < 0.01f)
    return;
  for (int i = 0; i < size; i++) {
    float val = static_cast<float>(data[i]);
    val = (val - 128.0f) * contrast + 128.0f;
    if (val < 0)
      val = 0;
    if (val > 255)
      val = 255;
    data[i] = static_cast<uint8_t>(val);
  }
}

// Main-task only.
void AnalogGaugeReader::process_image_from_buffer(const uint8_t *data, size_t len) {
  if (this->dials_.empty())
    return;
  if (data == nullptr || len == 0)
    return;

  camera::CameraImageSpec spec{};
  camera::DecoderError error = this->decoder_->decode(data, len, &spec);
  if (error != camera::DECODER_ERROR_SUCCESS) {
    ESP_LOGW(TAG, "JPEG decode failed: %s", camera::to_string(error));
    return;
  }
  camera::Buffer *decoded = this->decoder_->get_output_buffer();
  if (decoded == nullptr || decoded->get_data_buffer() == nullptr) {
    ESP_LOGW(TAG, "Decoder returned no output buffer");
    return;
  }

  const uint8_t *pixels = decoded->get_data_buffer();
  const int processing_w = spec.width;
  const int processing_h = spec.height;
  ESP_LOGV(TAG, "Decoded frame: %dx%d %s", processing_w, processing_h,
           spec.format == camera::PIXEL_FORMAT_GRAYSCALE ? LOG_STR_LITERAL("grayscale") : LOG_STR_LITERAL("colour"));

  // A camera_window may shrink the frame; a larger one would break the configured crops.
  if (processing_w > this->img_width_ || processing_h > this->img_height_) {
    ESP_LOGW(TAG,
             "Decoded dimensions larger than config: %dx%d vs %dx%d. Skipping analog processing to avoid conflict.",
             processing_w, processing_h, this->img_width_, this->img_height_);
    return;
  }

  float total_value = 0.0f;
  this->readings_.clear();

  for (size_t index = 0; index < this->dials_.size(); index++) {
    const auto &dial = this->dials_[index];
    DialBuffer &buffer = this->dial_buffers_[index];
    if (!this->crop_dial_(pixels, spec, dial, buffer)) {
      ESP_LOGE(TAG, "Could not crop dial %s from the %dx%d frame", dial.id, processing_w, processing_h);
      continue;
    }
    uint8_t *raw = buffer.data.get();
    size_t raw_len = buffer.size;
    const int crop_w = dial.crop_w;
    const int crop_h = dial.crop_h;
    const uint8_t *input_for_algo = raw;
    if (this->debug_) {
      ESP_LOGD(TAG, "Processing Dial: %s (Algorithm: %s)", dial.id,
               LOG_STR_ARG(gauge_needle_detector::algorithm_name(dial.algorithm)));
    }
    if (dial.process_channel != PROCESS_CHANNEL_GRAYSCALE && !dial.use_color) {
      if (this->scratch_buffer_.size() != static_cast<size_t>(crop_w * crop_h)) {
        this->scratch_buffer_.resize(crop_w * crop_h);
      }

      const int offset = static_cast<int>(dial.process_channel) - 1;
      if (buffer.channels == 3) {
        for (int i = 0; i < crop_w * crop_h; i++) {
          this->scratch_buffer_[i] = raw[i * 3 + offset];
        }
        input_for_algo = this->scratch_buffer_.data();
        if (dial.auto_contrast) {
          apply_auto_contrast(this->scratch_buffer_.data(), crop_w * crop_h);
        }
        if (std::abs(dial.contrast - 1.0f) > 0.01f) {
          apply_contrast(this->scratch_buffer_.data(), crop_w * crop_h, dial.contrast);
        }
      } else {
        ESP_LOGW(TAG, "Dial %s configured for channel %d but input is Grayscale", dial.id,
                 static_cast<int>(dial.process_channel));
      }
    } else if (dial.use_color) {
      if (this->scratch_buffer_.size() != static_cast<size_t>(crop_w * crop_h)) {
        this->scratch_buffer_.resize(crop_w * crop_h);
      }
      uint8_t tr = (dial.target_color >> 16) & 0xFF;
      uint8_t tg = (dial.target_color >> 8) & 0xFF;
      uint8_t tb = dial.target_color & 0xFF;

      // A tolerance flattens off-colour pixels, leaving only the needle.
      float tol = dial.color_tolerance;
      bool use_tol = tol > 0.0f && tol < 442.0f;

      for (int i = 0; i < crop_w * crop_h; i++) {
        uint8_t r = raw[i * 3 + 0];
        uint8_t g = raw[i * 3 + 1];
        uint8_t b = raw[i * 3 + 2];
        float dr = static_cast<float>(r - tr);
        float dg = static_cast<float>(g - tg);
        float db = static_cast<float>(b - tb);
        float dist = sqrtf(dr * dr + dg * dg + db * db);
        float val;
        if (use_tol) {
          val = (dist >= tol) ? 255.0f : (dist * 255.0f / tol);
        } else {
          val = (dist * 255.0f / 442.0f);
        }
        if (val > 255)
          val = 255;
        this->scratch_buffer_[i] = static_cast<uint8_t>(val);
      }
      input_for_algo = this->scratch_buffer_.data();
    } else if (dial.process_channel == PROCESS_CHANNEL_GRAYSCALE) {
      if (dial.auto_contrast) {
        apply_auto_contrast(raw, static_cast<int>(raw_len));
      }
      if (std::abs(dial.contrast - 1.0f) > 0.01f) {
        apply_contrast(raw, static_cast<int>(raw_len), dial.contrast);
      }
    }

    gauge_needle_detector::DetectorConfig detector_config;
    detector_config.id = dial.id;
    detector_config.needle_type = dial.needle_type;
    detector_config.algorithm = dial.algorithm;
    detector_config.min_scan_radius = dial.min_scan_radius;
    detector_config.max_scan_radius = dial.max_scan_radius;
    detector_config.deadzone_diameter = dial.deadzone_diameter;
    detector_config.debug = this->debug_;

    const uint32_t start_algo = micros();
    const gauge_needle_detector::DetectionResult result =
        this->detector_.detect(input_for_algo, crop_w, crop_h, detector_config);
    const uint32_t dur_algo = micros() - start_algo;

    float angle = result.angle;
    float confidence = result.confidence;
    if (this->debug_) {
      this->debug_angle_calculation(angle, dial);
    }

    float val = this->angle_to_value(angle, dial);
    const float display_angle = fmodf(angle + 90.0f, 360.0f);
    ESP_LOGD(TAG, "Dial %s: Angle=%.1f (North), Val=%.2f, Conf=%.2f, Time=%" PRIu32 " us", dial.id, display_angle, val,
             confidence, dur_algo);

    PUBLISH_STATE(dial.angle_sensor, display_angle);
    PUBLISH_STATE(dial.confidence_sensor, confidence);
    this->readings_.push_back({&dial, val});
  }

  if (this->stacked_digits_) {
    const int n = static_cast<int>(this->readings_.size());
    if (n == 0) {
      ESP_LOGW(TAG, "No valid dial readings available for stacked digit calculation");
      return;
    }
    auto wrap10 = [](float x) -> float {
      x = fmodf(x, 10.0f);
      if (x < 0.0f)
        x += 10.0f;
      return x;
    };
    auto round_digit10 = [&](float x) -> int {
      x = wrap10(x);
      int d = static_cast<int>(floorf(x + 0.5f));
      if (d >= 10)
        d -= 10;
      return d;
    };
    auto &digits = this->digits_;

    // Resolved dial phase in [0, 10), which is not always the raw reading.
    auto &phase = this->phase_;
    // Smallest scale = least significant dial.
    auto &order = this->order_;

    std::iota(order.begin(), order.begin() + n, 0);
    for (int i = 0; i < n; ++i) {
      float s = this->readings_[i].dial->scale;
      if (s <= 0.0f || !std::isfinite(s)) {
        ESP_LOGE(TAG, "Dial '%s' has invalid scale (%.3f); cannot compute stacked digits", this->readings_[i].dial->id,
                 s);
        return;
      }
    }

    std::stable_sort(order.begin(), order.begin() + n,
                     [&](int a, int b) { return this->readings_[a].dial->scale < this->readings_[b].dial->scale; });

    for (int p = 0; p < n; ++p) {
      const int i = order[p];
      const float raw = wrap10(this->readings_[i].value);
      float lower_fraction = 0.0f;
      if (p > 0) {
        const int lower_i = order[p - 1];
        // Use the RESOLVED lower phase, not the raw lower reading.
        lower_fraction = phase[lower_i] / 10.0f;
      }
      const float corrected = wrap10(raw - lower_fraction);
      const int d = round_digit10(corrected);
      digits[i] = d;
      if (p == 0) {
        // Finest dial has no lower dial, so keep its actual continuous phase.
        phase[i] = raw;
      } else {
        phase[i] = wrap10(static_cast<float>(d) + lower_fraction);
      }
      if (this->debug_) {
        ESP_LOGD(TAG, "Stacked dial %s: raw=%.3f lower_frac=%.3f corrected=%.3f local_digit=%d phase=%.3f",
                 this->readings_[i].dial->id, raw, lower_fraction, corrected, d, phase[i]);
      }
    }

    // Sum from the continuous phase, or 9.9 -> 0 loses the carry.
    const int finest_i = order[0];
    const float finest_scale = this->readings_[finest_i].dial->scale;
    if (finest_scale <= 0.0f) {
      ESP_LOGE(TAG, "Finest dial scale must be greater than 0");
      return;
    }

    float continuous_total = phase[finest_i] * finest_scale;
    for (int p = 1; p < n; ++p) {
      const int i = order[p];
      continuous_total += static_cast<float>(digits[i]) * this->readings_[i].dial->scale;
    }

    // Round without llroundf(), which not every ESP-IDF target provides.
    const float steps = continuous_total / finest_scale;
    if (!std::isfinite(steps) || std::abs(steps) > 1e15f) {
      ESP_LOGE(TAG, "Continuous total steps are non-finite or too large; cannot compute rounded steps");
      return;
    }
    const long long rounded_steps = static_cast<long long>(steps + (steps >= 0.0f ? 0.5f : -0.5f));
    total_value = static_cast<float>(rounded_steps) * finest_scale;

    // Derive the digits from the rounded total so the carry is consistent.
    for (int i = 0; i < n; ++i) {
      const DialConfig *dial = this->readings_[i].dial;
      const float ratio_f = dial->scale / finest_scale;
      if (!std::isfinite(ratio_f) || std::abs(ratio_f) > 1e15f) {
        ESP_LOGW(TAG, "Scale ratio for dial '%s' is non-finite or too large; skipping", dial->id);
        continue;
      }
      long long place = static_cast<long long>(ratio_f + (ratio_f >= 0.0f ? 0.5f : -0.5f));
      if (place <= 0)
        place = 1;
      int published_digit = static_cast<int>((rounded_steps / place) % 10);
      if (published_digit < 0)
        published_digit += 10;
      PUBLISH_STATE(dial->value_sensor, static_cast<float>(published_digit));
      if (this->debug_) {
        ESP_LOGD(TAG, "Stacked dial %s: final_digit=%d scale=%.7f", dial->id, published_digit, dial->scale);
      }
    }
  } else {
    for (const auto &r : this->readings_) {
      total_value += r.value * r.dial->scale;
      PUBLISH_STATE(r.dial->value_sensor, r.value);
    }
  }
  // Round away float noise; in stacked mode the finest scale is the precision.
  float precision = DECIMAL_PRECISION;
  if (this->stacked_digits_ && !this->dials_.empty()) {
    float min_scale = this->dials_.front().scale;
    for (const auto &d : this->dials_) {
      if (d.scale > 0.0f && d.scale < min_scale)
        min_scale = d.scale;
    }
    if (min_scale > 0.0f)
      precision = 1.0f / min_scale;
  }
  total_value = roundf(total_value * precision) / precision;
  ESP_LOGI(TAG, "Result: %.6f", total_value);
  PUBLISH_STATE(this->value_sensor_, total_value);
}

bool AnalogGaugeReader::crop_dial_(const uint8_t *src, const camera::CameraImageSpec &spec, const DialConfig &dial,
                                   DialBuffer &out) {
  const int crop_w = dial.crop_w;
  const int crop_h = dial.crop_h;
  if (src == nullptr || out.data == nullptr || crop_w <= 0 || crop_h <= 0)
    return false;
  if (dial.crop_x < 0 || dial.crop_y < 0 || dial.crop_x + crop_w > spec.width || dial.crop_y + crop_h > spec.height)
    return false;

  // The decoder delivers RGB888 or GRAYSCALE; anything else is a config mismatch.
  const bool source_is_gray = spec.format == camera::PIXEL_FORMAT_GRAYSCALE;
  if (!source_is_gray && spec.format != camera::PIXEL_FORMAT_RGB888)
    return false;
  const size_t source_bpp = source_is_gray ? 1 : camera_decoder::RGB888_BYTES_PER_PIXEL;
  const size_t source_stride = source_bpp * spec.width;
  const uint8_t *source_row =
      src + static_cast<size_t>(dial.crop_y) * source_stride + static_cast<size_t>(dial.crop_x) * source_bpp;

  if (out.channels == 3) {
    // A dial that reads colour needs the decoder's RGB888 output.
    if (source_is_gray)
      return false;
    for (int y = 0; y < crop_h; y++) {
      std::memcpy(out.data.get() + static_cast<size_t>(y) * crop_w * 3,
                  source_row + static_cast<size_t>(y) * source_stride, static_cast<size_t>(crop_w) * 3);
    }
    return true;
  }
  for (int y = 0; y < crop_h; y++) {
    const uint8_t *row = source_row + static_cast<size_t>(y) * source_stride;
    uint8_t *dst = out.data.get() + static_cast<size_t>(y) * crop_w;
    if (source_is_gray) {
      std::memcpy(dst, row, crop_w);
      continue;
    }
    for (int x = 0; x < crop_w; x++) {
      const uint8_t *pixel = row + static_cast<size_t>(x) * 3;
      dst[x] = static_cast<uint8_t>((pixel[0] * camera_decoder::LUMA_RED + pixel[1] * camera_decoder::LUMA_GREEN +
                                     pixel[2] * camera_decoder::LUMA_BLUE) >>
                                    8);
    }
  }
  return true;
}

float AnalogGaugeReader::angle_to_value(float image_angle, const DialConfig &dial) {
  // Image angles start at East, dial angles at North, so rotate by 90 degrees.
  float dial_angle = fmodf(image_angle + 90.0f, 360.0f);

  // Mirror the angle when the value increases counter-clockwise.
  if (!dial.clockwise) {
    dial_angle = fmodf(360.0f - dial_angle, 360.0f);
  }
  dial_angle = fmodf(dial_angle - dial.angle_offset + 360.0f, 360.0f);

  if (dial.min_angle > dial.max_angle) {
    if (dial_angle < dial.min_angle)
      dial_angle += 360.0f;
    const float range = (dial.max_angle + 360.0f) - dial.min_angle;
    const float rel_angle = dial_angle - dial.min_angle;
    const float fraction = rel_angle / range;
    return dial.min_value + fraction * (dial.max_value - dial.min_value);
  }

  if (!dial.calibration_mapping.empty()) {
    float input_x = dial_angle;
    if (input_x <= dial.calibration_mapping.front().first)
      return dial.calibration_mapping.front().second;
    if (input_x >= dial.calibration_mapping.back().first)
      return dial.calibration_mapping.back().second;
    for (size_t i = 0; i < dial.calibration_mapping.size() - 1; i++) {
      float x1 = dial.calibration_mapping[i].first;
      float y1 = dial.calibration_mapping[i].second;
      float x2 = dial.calibration_mapping[i + 1].first;
      float y2 = dial.calibration_mapping[i + 1].second;
      if (input_x >= x1 && input_x <= x2) {
        if (std::abs(x2 - x1) < 0.001f)
          return y1;
        float t = (input_x - x1) / (x2 - x1);
        return y1 + t * (y2 - y1);
      }
    }
    return dial.calibration_mapping.back().second;
  }

  float fraction = (dial_angle - dial.min_angle) / (dial.max_angle - dial.min_angle);
  fraction = std::max(0.0f, std::min(1.0f, fraction));
  return dial.min_value + fraction * (dial.max_value - dial.min_value);
}

void AnalogGaugeReader::debug_angle_calculation(float image_angle, const DialConfig &dial) {
  ESP_LOGD(TAG, "=== DEBUG ANGLE CONVERSION for %s ===", dial.id);
  ESP_LOGD(TAG, "Input image angle: %.1f deg (0 deg=East, clockwise)", image_angle);
  float dial_angle = fmodf(image_angle + 90.0f, 360.0f);
  ESP_LOGD(TAG, "Dial angle (after +90 to North): %.1f deg", dial_angle);
  dial_angle = fmodf(dial_angle - dial.angle_offset + 360.0f, 360.0f);
  ESP_LOGD(TAG, "After angle_offset (%.1f): %.1f deg", dial.angle_offset, dial_angle);
  float range_angle = dial.max_angle - dial.min_angle;
  if (dial.min_angle > dial.max_angle) {
    range_angle = (dial.max_angle + 360.0f) - dial.min_angle;
  }
  float effective_dial_angle = dial_angle;
  if (dial.min_angle > dial.max_angle && effective_dial_angle < dial.min_angle) {
    effective_dial_angle += 360.0f;
  }
  float rel_angle = effective_dial_angle - dial.min_angle;
  float fraction = 0.0f;
  if (range_angle > 0) {
    fraction = rel_angle / range_angle;
    fraction = std::max(0.0f, std::min(1.0f, fraction));
  }
  float value = dial.min_value + fraction * (dial.max_value - dial.min_value);
  ESP_LOGD(TAG, "Range: %.1f deg to %.1f deg (span=%.1f deg)", dial.min_angle, dial.max_angle, range_angle);
  ESP_LOGD(TAG, "Rel angle: %.1f deg, Fraction: %.3f", rel_angle, fraction);
  ESP_LOGD(TAG, "Value: %.3f (%.1f to %.1f)", value, dial.min_value, dial.max_value);
  ESP_LOGD(TAG, "=====================================");
}

}  // namespace analog_gauge_reader
}  // namespace esphome
