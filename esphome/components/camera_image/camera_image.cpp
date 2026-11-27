#include "camera_image.h"

#ifdef USE_CAMERA_IMAGE

#include "esphome/core/log.h"
#include "esphome/core/progmem.h"

#include <cstdint>
#include <cstring>

namespace esphome::camera_image {

ESPHOME_LOG_TAG(TAG, "camera_image");

/// Scheduler id for the delivery timeout, so a late frame can cancel it.
static constexpr uint32_t REQUEST_TIMEOUT_ID = 0;

namespace {

PROGMEM_STRING_TABLE(ImageTypeStrings, "BINARY", "GRAYSCALE", "RGB", "RGB565", "RGB111", "UNKNOWN");

}  // namespace

CameraImage::CameraImage(camera::Camera *camera, int width, int height, runtime_image::ImageFormat format,
                         image::ImageType type, image::Transparency transparency, image::Image *placeholder,
                         bool is_big_endian, uint32_t request_timeout, const Transform &transform)
    : RuntimeImage(format, type, transparency, placeholder, is_big_endian, width, height),
      camera_(camera),
#ifdef USE_CAMERA_IMAGE_TRANSFORM
      transform_(transform),
#endif
      request_timeout_(request_timeout),
      output_width_(transform.output_width),
      output_height_(transform.output_height) {
}

void CameraImage::setup() {
  this->camera_->add_listener(this);

#ifdef USE_CAMERA_IMAGE_TRANSFORM
  // A crop only copies rows in place, so only a rotation needs scratch memory.
  if (this->transform_.rotation != 0 && this->transform_.buffer == nullptr) {
    this->mark_failed(LOG_STR("Could not allocate the rotation buffer"));
  }
#endif
}

void CameraImage::update() {
  // A request is still in flight; its timeout clears the pending state.
  if (this->waiting_)
    return;

  this->waiting_ = true;
  this->timed_out_ = false;
  this->set_timeout(REQUEST_TIMEOUT_ID, this->request_timeout_, [this]() {
    if (!this->waiting_)
      return;
    this->waiting_ = false;
    this->timed_out_ = true;
    ESP_LOGW(TAG, "Camera did not deliver a frame within %u ms", static_cast<unsigned>(this->request_timeout_));
    this->on_error_callback_.call();
  });
  this->camera_->request_image(camera::COMPONENT_REQUESTER);
}

void CameraImage::on_camera_image(const std::shared_ptr<camera::CameraImage> &image) {
  if (this->is_failed())
    return;
  // The camera notifies every listener for every frame; only the frame this
  // component asked for may clear the pending request. A frame captured for the
  // API, web server or RTSP server would otherwise be decoded here while the
  // frame requested for this component is dropped.
  if (!this->waiting_) {
    if (this->timed_out_ && image != nullptr && image->was_requested_by(camera::COMPONENT_REQUESTER)) {
      this->timed_out_ = false;
      ESP_LOGD(TAG, "Dropped a frame that arrived after the %" PRIu32 " ms timeout; increase 'timeout'",
               this->request_timeout_);
    }
    return;
  }
  if (image != nullptr && !image->was_requested_by(camera::COMPONENT_REQUESTER))
    return;
  this->waiting_ = false;
  this->cancel_timeout(REQUEST_TIMEOUT_ID);

  if (image == nullptr || image->get_data_length() == 0) {
    ESP_LOGW(TAG, "Camera delivered an empty frame");
    this->on_error_callback_.call();
    return;
  }

  if (this->decode_frame_(image->get_data_buffer(), image->get_data_length())) {
    this->on_image_callback_.call();
  } else {
    ESP_LOGW(TAG, "Failed to decode the camera frame");
    this->on_error_callback_.call();
  }
}

bool CameraImage::decode_frame_(const uint8_t *data, size_t length) {
  if (this->is_decoding())
    this->abort_decode();

#ifdef USE_CAMERA_IMAGE_TRANSFORM
  // A transformed frame left the buffer reporting the output size; put the
  // decoded size back so the next decode reuses this buffer instead of
  // allocating a new one.
  if (this->decode_width_ > 0) {
    this->buffer_width_ = this->decode_width_;
    this->buffer_height_ = this->decode_height_;
  }
#endif

  if (!this->begin_decode(length, this->get_format()))
    return false;

  size_t offset = 0;
  while (offset < length) {
    const int consumed = this->feed_data(const_cast<uint8_t *>(data) + offset, length - offset);
    if (consumed < 0) {
      ESP_LOGE(TAG, "Decoder error %d at offset %zu of %zu bytes", consumed, offset, length);
      this->abort_decode();
      return false;
    }
    if (consumed == 0) {
      // The decoder asked for more data than the frame holds.
      ESP_LOGE(TAG, "Decoder stalled at offset %zu of %zu bytes", offset, length);
      this->abort_decode();
      return false;
    }
    offset += static_cast<size_t>(consumed);
  }

  if (!this->is_decode_finished()) {
    ESP_LOGE(TAG, "Truncated frame: decoded %zu of %zu bytes", offset, length);
    this->abort_decode();
    return false;
  }
  // Crop and rotate before the frame is published, so a crop that does not fit
  // the decoded frame fails the update instead of showing the untransformed one.
#ifdef USE_CAMERA_IMAGE_TRANSFORM
  if (!this->apply_transform_()) {
    this->abort_decode();
    return false;
  }
#endif
  // The image size was derived from the camera resolution; a sensor `window` delivers less.
  if (!this->size_mismatch_logged_ &&
      (this->buffer_width_ != this->output_width_ || this->buffer_height_ != this->output_height_)) {
    this->size_mismatch_logged_ = true;
    ESP_LOGW(TAG, "Image is %dx%d, but the configuration expects %dx%d; check the camera 'window'", this->buffer_width_,
             this->buffer_height_, this->output_width_, this->output_height_);
  }
  return this->end_decode();
}

#ifdef USE_CAMERA_IMAGE_TRANSFORM
bool CameraImage::apply_transform_() {
  const Transform &transform = this->transform_;
  if (transform.rotation == 0 && transform.crop_width == 0)
    return true;

  if (this->buffer_ == nullptr || this->buffer_width_ <= 0 || this->buffer_height_ <= 0) {
    ESP_LOGE(TAG, "No decoded buffer to crop or rotate (%dx%d); the resize allocation failed", this->buffer_width_,
             this->buffer_height_);
    return false;
  }

  const int source_width = this->buffer_width_;
  const int source_height = this->buffer_height_;
  const bool has_crop = transform.crop_width > 0;
  const int crop_x = has_crop ? transform.crop_x : 0;
  const int crop_y = has_crop ? transform.crop_y : 0;
  const int crop_width = has_crop ? transform.crop_width : source_width;
  const int crop_height = has_crop ? transform.crop_height : source_height;

  if (crop_x + crop_width > source_width || crop_y + crop_height > source_height) {
    ESP_LOGE(TAG, "Crop %d,%d %dx%d is outside the %dx%d frame", crop_x, crop_y, crop_width, crop_height, source_width,
             source_height);
    return false;
  }

  const bool swap = transform.rotation == 90 || transform.rotation == 270;
  const int output_width = swap ? crop_height : crop_width;
  const int output_height = swap ? crop_width : crop_height;

  const size_t pixel_size = (this->get_bpp() + 7) / 8;
  const size_t source_stride = static_cast<size_t>(source_width) * pixel_size;
  const size_t output_stride = static_cast<size_t>(output_width) * pixel_size;
  const size_t output_size = output_stride * static_cast<size_t>(output_height);
  const uint8_t *crop_origin = this->buffer_ + static_cast<size_t>(crop_y) * source_stride + crop_x * pixel_size;

  if (transform.rotation == 0) {
    // Every output row is one run of a source row, and the destination is never
    // ahead of the source, so the copy can happen in place.
    for (int y = 0; y < output_height; y++) {
      std::memmove(this->buffer_ + static_cast<size_t>(y) * output_stride,
                   crop_origin + static_cast<size_t>(y) * source_stride, output_stride);
    }
  } else {
    if (transform.buffer == nullptr || output_size > transform.buffer_size) {
      ESP_LOGE(TAG, "No buffer allocated for a %dx%d crop/rotation", output_width, output_height);
      return false;
    }
    if (!rotate_crop(pixel_size, transform.buffer, crop_origin, crop_width, crop_height, output_width, output_height,
                     source_stride, output_stride, transform.rotation)) {
      ESP_LOGE(TAG, "Cannot rotate a frame with %zu-byte pixels", pixel_size);
      return false;
    }
    // Publish from the decoded buffer: it keeps its own size, so the next decode
    // reuses it and no frame allocates.
    std::memcpy(this->buffer_, transform.buffer, output_size);
  }

  this->decode_width_ = source_width;
  this->decode_height_ = source_height;
  this->buffer_width_ = output_width;
  this->buffer_height_ = output_height;
  return true;
}
#endif

void CameraImage::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Camera Image:\n"
                "  Format: %s\n"
                "  Type: %s",
                LOG_STR_ARG(runtime_image::get_format_name(this->get_format())),
                LOG_STR_ARG(ImageTypeStrings::get_log_str(this->get_type())));
#ifdef USE_CAMERA_IMAGE_TRANSFORM
  if (this->transform_.crop_width > 0) {
    ESP_LOGCONFIG(TAG, "  Crop: %d,%d %dx%d", this->transform_.crop_x, this->transform_.crop_y,
                  this->transform_.crop_width, this->transform_.crop_height);
  }
  if (this->transform_.rotation != 0) {
    ESP_LOGCONFIG(TAG,
                  "  Rotation: %u\n"
                  "  Rotation buffer: %zu bytes",
                  static_cast<unsigned>(this->transform_.rotation), this->transform_.buffer_size);
  }
  ESP_LOGCONFIG(TAG, "  Output size: %dx%d", this->transform_.output_width, this->transform_.output_height);
#endif
  ESP_LOGCONFIG(TAG, "  Request timeout: %" PRIu32 " ms", this->request_timeout_);
  LOG_UPDATE_INTERVAL(this);
}

}  // namespace esphome::camera_image

#endif  // USE_CAMERA_IMAGE
