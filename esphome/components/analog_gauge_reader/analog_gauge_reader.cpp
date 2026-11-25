#include "analog_gauge_reader.h"

#include "esphome/components/camera_decoder/jpeg_grayscale.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>

#include <esp_heap_caps.h>

namespace esphome::analog_gauge_reader {

ESPHOME_LOG_TAG(TAG, "analog_gauge_reader");

static constexpr float DECIMAL_PRECISION = 10000.0f;  // 4 decimal places (10^4)

void AnalogGaugeReader::setup() {
  this->camera_->add_listener(this);
  // The loop only has work between a request and its frame.
  this->disable_loop();

  if (!this->detector_.reserve(this->max_crop_w_, this->max_crop_h_)) {
    this->mark_failed(LOG_STR("Could not allocate the detector buffers"));
    return;
  }
  // The decoder keeps the decoded frame in PSRAM when it can.
  if (heap_caps_get_free_size(MALLOC_CAP_SPIRAM) == 0)
    ESP_LOGW(TAG, "No PSRAM detected; the decoder may not be able to allocate the decoded frame");
}

void AnalogGaugeReader::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Analog Gauge Reader:\n"
                "  Frame: %dx%d\n"
                "  Debug: %s",
                this->img_width_, this->img_height_, YESNO(this->debug_));
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Value", this->value_sensor_);
  for (const auto &dial : this->dials_) {
    ESP_LOGCONFIG(TAG,
                  "  Dial '%s':\n"
                  "    Algorithm: %s\n"
                  "    Scale: %.3f\n"
                  "    Crop: [%d,%d,%d,%d]\n"
                  "    Auto contrast: %s\n"
                  "    Contrast: %.2f\n"
                  "    Deadzone: %.1fpx",
                  dial.id, LOG_STR_ARG(gauge_needle_detector::algorithm_name(dial.algorithm)), dial.scale, dial.crop_x,
                  dial.crop_y, dial.crop_w, dial.crop_h, YESNO(dial.auto_contrast), dial.contrast,
                  dial.deadzone_diameter);
    LOG_SENSOR("    ", "Value", dial.value_sensor);
    LOG_SENSOR("    ", "Angle", dial.angle_sensor);
    LOG_SENSOR("    ", "Confidence", dial.confidence_sensor);
  }
  this->decoder_->dump_config();
}

void AnalogGaugeReader::update() {
  if (this->paused_)
    return;
  this->set_timeout(FRAME_TIMEOUT_ID, FRAME_TIMEOUT_MS, [this]() {
    ESP_LOGW(TAG, "Frame timeout");
    this->status_set_warning();
  });
  this->camera_->request_image(camera::IDLE);
}

void AnalogGaugeReader::on_camera_image(const std::shared_ptr<esphome::camera::CameraImage> &image) {
  // Frames the camera captures for other listeners are not ours to process.
  if (this->paused_ || !this->cancel_timeout(FRAME_TIMEOUT_ID))
    return;
  this->pending_frame_ = image;
  this->enable_loop();
}

void AnalogGaugeReader::loop() {
  // The camera calls its listeners from the main loop, so no locking is needed.
  std::shared_ptr<esphome::camera::CameraImage> frame = std::move(this->pending_frame_);
  if (frame) {
    this->disable_loop();
    if (frame->get_data_length() == 0 || frame->get_data_buffer() == nullptr) {
      ESP_LOGW(TAG, "The camera delivered an empty frame");
      this->status_set_warning();
    } else if (this->process_image_(frame->get_data_buffer(), frame->get_data_length())) {
      this->status_clear_warning();
    } else {
      this->status_set_warning();
    }
  }
}

// Main-task only.
bool AnalogGaugeReader::process_image_(const uint8_t *data, size_t len) {
  camera::CameraImageSpec spec{};
  camera::DecoderError error = this->decoder_->decode(data, len, &spec);
  if (error != camera::DECODER_ERROR_SUCCESS) {
    ESP_LOGW(TAG, "JPEG decode failed: %s", camera::to_string(error));
    return false;
  }
  camera::Buffer *decoded = this->decoder_->get_output_buffer();
  if (decoded == nullptr || decoded->get_data_buffer() == nullptr) {
    ESP_LOGW(TAG, "Decoder returned no output buffer");
    return false;
  }

  // A camera_window may shrink the frame; a larger one would break the configured crops.
  if (spec.width > this->img_width_ || spec.height > this->img_height_) {
    ESP_LOGW(TAG,
             "Decoded dimensions larger than config: %dx%d vs %dx%d. Skipping analog processing to avoid conflict.",
             spec.width, spec.height, this->img_width_, this->img_height_);
    return false;
  }

  this->readings_.clear();
  for (const auto &dial : this->dials_) {
    if (!this->crop_dial_(decoded->get_data_buffer(), spec, dial)) {
      ESP_LOGE(TAG, "Could not crop dial %s from the %dx%d frame", dial.id, spec.width, spec.height);
      return false;
    }
    const int pixels = dial.crop_w * dial.crop_h;
    uint8_t *gray = dial.crop;
#ifdef USE_ANALOG_GAUGE_READER_COLOR
    if (dial.target_color != NO_TARGET_COLOR) {
      const uint8_t tr = (dial.target_color >> 16) & 0xFF;
      const uint8_t tg = (dial.target_color >> 8) & 0xFF;
      const uint8_t tb = dial.target_color & 0xFF;
      // A tolerance flattens off-colour pixels, leaving only the needle.
      const float tol = dial.color_tolerance;
      const float range = tol > 0.0f && tol < MAX_COLOR_DISTANCE ? tol : MAX_COLOR_DISTANCE;
      for (int i = 0; i < pixels; i++) {
        const float dr = static_cast<float>(gray[i * 3 + 0] - tr);
        const float dg = static_cast<float>(gray[i * 3 + 1] - tg);
        const float db = static_cast<float>(gray[i * 3 + 2] - tb);
        this->scratch_buffer_[i] =
            static_cast<uint8_t>(std::min(sqrtf(dr * dr + dg * dg + db * db) * 255.0f / range, 255.0f));
      }
      gray = this->scratch_buffer_;
    } else if (dial.process_channel != PROCESS_CHANNEL_GRAYSCALE) {
      const int offset = static_cast<int>(dial.process_channel) - 1;
      for (int i = 0; i < pixels; i++)
        this->scratch_buffer_[i] = gray[i * 3 + offset];
      gray = this->scratch_buffer_;
    }
#endif
    // Every path ends on one byte per pixel, so the contrast options apply to all.
    if (dial.auto_contrast)
      apply_auto_contrast(gray, pixels);
    apply_contrast(gray, pixels, dial.contrast);

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
        this->detector_.detect(gray, dial.crop_w, dial.crop_h, detector_config);
    const uint32_t dur_algo = micros() - start_algo;
    // Zero confidence means no needle; its angle must not become a value.
    if (result.confidence <= 0.0f) {
      ESP_LOGW(TAG, "No needle found for dial %s (or the detector failed, see its log); dropping this frame", dial.id);
      return false;
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
  float total_value = 0.0f;
  float precision = DECIMAL_PRECISION;
#ifdef USE_ANALOG_GAUGE_READER_STACKED_DIGITS
  total_value = combine_stacked_digits(this->readings_.begin(), this->readings_.size(), this->order_.data(),
                                       this->phase_.data(), this->digits_.data());
  if (!std::isfinite(total_value)) {
    ESP_LOGE(TAG, "Stacked dial values cannot be combined; dropping this frame");
    return false;
  }
  precision = 1.0f / this->readings_[this->order_[0]].dial->scale;
  for (size_t i = 0; i < this->readings_.size(); i++)
    PUBLISH_STATE(this->readings_[i].dial->value_sensor, static_cast<float>(this->digits_[i]));
#else
  for (const auto &r : this->readings_) {
    total_value += r.value * r.dial->scale;
    PUBLISH_STATE(r.dial->value_sensor, r.value);
  }
#endif
  total_value = roundf(total_value * precision) / precision;
  ESP_LOGV(TAG, "Result: %.6f", total_value);
  PUBLISH_STATE(this->value_sensor_, total_value);
  return true;
}

bool AnalogGaugeReader::crop_dial_(const uint8_t *src, const camera::CameraImageSpec &spec, const DialConfig &dial) {
  const int crop_w = dial.crop_w;
  const int crop_h = dial.crop_h;
  if (dial.crop_x + crop_w > spec.width || dial.crop_y + crop_h > spec.height)
    return false;

  // The decoder delivers RGB888 or GRAYSCALE; anything else is a config mismatch.
  const bool source_is_gray = spec.format == camera::PIXEL_FORMAT_GRAYSCALE;
  if (!source_is_gray && spec.format != camera::PIXEL_FORMAT_RGB888)
    return false;
  const size_t source_bpp = source_is_gray ? 1 : camera_decoder::RGB888_BYTES_PER_PIXEL;
  const size_t source_stride = source_bpp * spec.width;
  const uint8_t *source_row =
      src + static_cast<size_t>(dial.crop_y) * source_stride + static_cast<size_t>(dial.crop_x) * source_bpp;

#ifdef USE_ANALOG_GAUGE_READER_COLOR
  if (dial.channels == 3) {
    if (source_is_gray)
      return false;
    for (int y = 0; y < crop_h; y++) {
      std::memcpy(dial.crop + static_cast<size_t>(y) * crop_w * 3, source_row + static_cast<size_t>(y) * source_stride,
                  static_cast<size_t>(crop_w) * 3);
    }
    return true;
  }
#endif
  for (int y = 0; y < crop_h; y++) {
    const uint8_t *row = source_row + static_cast<size_t>(y) * source_stride;
    uint8_t *dst = dial.crop + static_cast<size_t>(y) * crop_w;
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

}  // namespace esphome::analog_gauge_reader
