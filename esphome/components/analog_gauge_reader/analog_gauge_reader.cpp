#include "analog_gauge_reader.h"

#include "esphome/components/camera_decoder/jpeg_grayscale.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>

#ifdef USE_ESP32
#include <esp_heap_caps.h>
#endif

namespace esphome {
namespace analog_gauge_reader {

ESPHOME_LOG_TAG(TAG, "analog_gauge_reader");

static constexpr float DECIMAL_PRECISION = 10000.0f;  // 4 decimal places (10^4)

void AnalogGaugeReader::setup() {
  this->camera_->add_listener(this);
  // The loop only has work between a request and its frame.
  this->disable_loop();

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
  this->scratch_buffer_ = std::make_unique<uint8_t[]>(max_crop_area);
  if (!this->detector_.reserve(max_crop_w, max_crop_h)) {
    this->mark_failed(LOG_STR("Could not allocate the detector buffers"));
    return;
  }
  ESP_LOGD(TAG, "Reserved scratch buffers for max area: %d pixels", static_cast<int>(max_crop_area));

  this->dial_buffers_.clear();
  this->dial_buffers_.reserve(this->dials_.size());
  for (const auto &dial : this->dials_) {
    DialBuffer buffer;
    buffer.channels =
        (dial.target_color != NO_TARGET_COLOR || dial.process_channel != PROCESS_CHANNEL_GRAYSCALE) ? 3 : 1;
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
  ESP_LOGCONFIG(TAG, "Analog Gauge Reader: Frame=%dx%d, Debug=%s, Update Interval=%" PRIu32 " ms, Stacked Digits=%s",
                this->img_width_, this->img_height_, this->debug_ ? LOG_STR_LITERAL("YES") : LOG_STR_LITERAL("NO"),
                this->get_update_interval(), this->stacked_digits_ ? LOG_STR_LITERAL("YES") : LOG_STR_LITERAL("NO"));
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
  if (this->paused_ || this->requested_)
    return;
  this->requested_ = true;
  this->last_request_time_ = App.get_loop_component_start_time();
  this->enable_loop();
  this->camera_->request_image(camera::IDLE);
  ESP_LOGV(TAG, "Requesting frame");
}

void AnalogGaugeReader::on_camera_image(const std::shared_ptr<esphome::camera::CameraImage> &image) {
  // Frames the camera captures for other listeners are not ours to process.
  if (!this->requested_ || this->paused_)
    return;
  this->pending_frame_ = image;
  this->requested_ = false;
}

void AnalogGaugeReader::loop() {
  // The camera calls its listeners from the main loop, so no locking is needed.
  std::shared_ptr<esphome::camera::CameraImage> frame = std::move(this->pending_frame_);
  if (frame) {
    this->disable_loop();
    if (frame->get_data_length() > 0) {
      this->process_image_from_buffer(frame->get_data_buffer(), frame->get_data_length());
    }
    return;
  }

  // Watchdog: a requested frame that never arrives must not block later polls.
  if (this->requested_ && App.get_loop_component_start_time() - this->last_request_time_ > FRAME_TIMEOUT_MS) {
    ESP_LOGW(TAG, "Frame timeout");
    this->requested_ = false;
    this->disable_loop();
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
      return;
    }
    const int crop_w = dial.crop_w;
    const int crop_h = dial.crop_h;
    uint8_t *gray = buffer.data.get();
    if (dial.target_color != NO_TARGET_COLOR) {
      uint8_t *scratch = this->scratch_buffer_.get();
      const uint8_t tr = (dial.target_color >> 16) & 0xFF;
      const uint8_t tg = (dial.target_color >> 8) & 0xFF;
      const uint8_t tb = dial.target_color & 0xFF;
      // A tolerance flattens off-colour pixels, leaving only the needle.
      const float tol = dial.color_tolerance;
      const bool use_tol = tol > 0.0f && tol < 442.0f;
      for (int i = 0; i < crop_w * crop_h; i++) {
        const float dr = static_cast<float>(gray[i * 3 + 0] - tr);
        const float dg = static_cast<float>(gray[i * 3 + 1] - tg);
        const float db = static_cast<float>(gray[i * 3 + 2] - tb);
        const float dist = sqrtf(dr * dr + dg * dg + db * db);
        const float val = use_tol ? (dist >= tol ? 255.0f : dist * 255.0f / tol) : dist * 255.0f / 442.0f;
        scratch[i] = static_cast<uint8_t>(std::min(val, 255.0f));
      }
      gray = scratch;
    } else if (dial.process_channel != PROCESS_CHANNEL_GRAYSCALE) {
      uint8_t *scratch = this->scratch_buffer_.get();
      const int offset = static_cast<int>(dial.process_channel) - 1;
      for (int i = 0; i < crop_w * crop_h; i++) {
        scratch[i] = gray[i * 3 + offset];
      }
      gray = scratch;
    }
    // Both maps end on one byte per pixel, so the contrast options apply to either.
    if (dial.auto_contrast) {
      apply_auto_contrast(gray, crop_w * crop_h);
    }
    apply_contrast(gray, crop_w * crop_h, dial.contrast);

    gauge_needle_detector::DetectorConfig detector_config;
    detector_config.id = dial.id;
    detector_config.needle_type = dial.needle_type;
    detector_config.algorithm = dial.algorithm;
    detector_config.min_scan_radius = dial.min_scan_radius;
    detector_config.max_scan_radius = dial.max_scan_radius;
    detector_config.deadzone_diameter = dial.deadzone_diameter;
    detector_config.debug = this->debug_;

    const uint32_t start_algo = micros();
    const gauge_needle_detector::DetectionResult result = this->detector_.detect(gray, crop_w, crop_h, detector_config);
    const uint32_t dur_algo = micros() - start_algo;
    // Zero confidence means no needle; its angle must not become a value.
    if (result.confidence <= 0.0f) {
      ESP_LOGW(TAG, "No needle found for dial %s; dropping this frame", dial.id);
      return;
    }

    const float val = angle_to_value(result.angle, dial);
    const float display_angle = wrap360(result.angle + 90.0f);
    ESP_LOGV(TAG, "Dial %s: Angle=%.1f (North), Val=%.2f, Conf=%.2f, Time=%" PRIu32 " us", dial.id, display_angle, val,
             result.confidence, dur_algo);
    this->readings_.push_back({&dial, val, display_angle, result.confidence});
  }

  // Publish the per-dial states only now, so a dropped frame leaves every sensor untouched.
  for (const auto &r : this->readings_) {
    PUBLISH_STATE(r.dial->angle_sensor, r.angle);
    PUBLISH_STATE(r.dial->confidence_sensor, r.confidence);
  }

  // Round away float noise; stacked digits keep the finest dial's resolution.
  float precision = DECIMAL_PRECISION;
  if (this->stacked_digits_) {
    const size_t n = this->readings_.size();
    if (n == 0) {
      ESP_LOGW(TAG, "No valid dial readings available for stacked digit calculation");
      return;
    }
    total_value = combine_stacked_digits(this->readings_.data(), n, this->order_.data(), this->phase_.data(),
                                         this->digits_.data());
    if (!std::isfinite(total_value)) {
      ESP_LOGE(TAG, "Stacked dial values cannot be combined; dropping this frame");
      return;
    }
    precision = 1.0f / this->readings_[this->order_[0]].dial->scale;
    for (size_t i = 0; i < n; i++) {
      const DialConfig *dial = this->readings_[i].dial;
      PUBLISH_STATE(dial->value_sensor, static_cast<float>(this->digits_[i]));
      if (this->debug_) {
        ESP_LOGD(TAG, "Stacked dial %s: final_digit=%d scale=%.7f", dial->id, this->digits_[i], dial->scale);
      }
    }
  } else {
    for (const auto &r : this->readings_) {
      total_value += r.value * r.dial->scale;
      PUBLISH_STATE(r.dial->value_sensor, r.value);
    }
  }
  total_value = roundf(total_value * precision) / precision;
  ESP_LOGV(TAG, "Result: %.6f", total_value);
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

}  // namespace analog_gauge_reader
}  // namespace esphome
