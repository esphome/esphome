#include "camera_image.h"

#ifdef USE_CAMERA_IMAGE

#include "esphome/core/log.h"

#include <cstring>

namespace esphome::camera_image {

ESPHOME_LOG_TAG(TAG, "camera_image");

/// Give up on a frame the camera never delivered, so the next update can try again.
static constexpr uint32_t REQUEST_TIMEOUT_MS = 5000;
/// Scheduler id for the delivery timeout, so a late frame can cancel it.
static constexpr uint32_t REQUEST_TIMEOUT_ID = 0;

CameraImage::CameraImage(camera::Camera *camera, int width, int height, runtime_image::ImageFormat format,
                         image::ImageType type, image::Transparency transparency, image::Image *placeholder,
                         bool is_big_endian)
    : RuntimeImage(format, type, transparency, placeholder, is_big_endian, width, height), camera_(camera) {}

CameraImage::~CameraImage() {
  if (this->transform_buffer_ != nullptr) {
    RAMAllocator<uint8_t>().deallocate(this->transform_buffer_, this->transform_buffer_size_);
  }
}

void CameraImage::setup() {
  this->camera_->add_listener(this);

  // A crop only copies rows in place, so only a rotation needs scratch memory.
  if (this->rotation_ == 0 || this->transform_width_ <= 0 || this->transform_height_ <= 0)
    return;

  const size_t bytes_per_pixel = (this->get_bpp() + 7) / 8;
  const uint64_t bytes_64 = static_cast<uint64_t>(this->transform_width_) *
                            static_cast<uint64_t>(this->transform_height_) * static_cast<uint64_t>(bytes_per_pixel);
  if (bytes_64 > SIZE_MAX) {
    this->mark_failed(LOG_STR("Crop/rotation buffer size overflows"));
    return;
  }

  this->transform_buffer_size_ = static_cast<size_t>(bytes_64);
  this->transform_buffer_ = RAMAllocator<uint8_t>().allocate(this->transform_buffer_size_);
  if (this->transform_buffer_ == nullptr) {
    ESP_LOGE(TAG, "Failed to allocate %zu bytes for rotation", this->transform_buffer_size_);
    this->mark_failed(LOG_STR("Could not allocate the rotation buffer"));
  }
}

void CameraImage::update() {
  if (this->is_failed())
    return;

  // A request is still in flight; its timeout clears the pending state.
  if (this->waiting_)
    return;

  this->waiting_ = true;
  this->set_timeout(REQUEST_TIMEOUT_ID, REQUEST_TIMEOUT_MS, [this]() {
    if (!this->waiting_)
      return;
    this->waiting_ = false;
    ESP_LOGW(TAG, "Camera did not deliver a frame within %u ms", static_cast<unsigned>(REQUEST_TIMEOUT_MS));
    this->on_error_callback_.call();
  });
  this->camera_->request_image(camera::IDLE);
}

void CameraImage::on_camera_image(const std::shared_ptr<camera::CameraImage> &image) {
  if (this->is_failed())
    return;
  // The camera notifies every listener for every frame; only decode the frame
  // this component asked for.
  if (!this->waiting_)
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

  // A transformed frame left the buffer reporting the output size; put the
  // decoded size back so the next decode reuses this buffer instead of
  // allocating a new one.
  if (this->decode_width_ > 0) {
    this->buffer_width_ = this->decode_width_;
    this->buffer_height_ = this->decode_height_;
  }

  if (!this->begin_decode(length, this->get_format()))
    return false;

  size_t offset = 0;
  while (offset < length) {
    const int consumed = this->feed_data(const_cast<uint8_t *>(data) + offset, length - offset);
    if (consumed < 0) {
      this->abort_decode();
      return false;
    }
    if (consumed == 0)
      break;
    offset += static_cast<size_t>(consumed);
  }

  if (!this->is_decode_finished()) {
    this->abort_decode();
    return false;
  }
  // Crop and rotate before the frame is published, so a crop that does not fit
  // the decoded frame fails the update instead of showing the untransformed one.
  if (!this->apply_crop_rotation_()) {
    this->abort_decode();
    return false;
  }
  return this->end_decode();
}

bool CameraImage::apply_crop_rotation_() {
  if (this->rotation_ == 0 && !this->has_crop_)
    return true;

  if (this->buffer_ == nullptr || this->buffer_width_ <= 0 || this->buffer_height_ <= 0) {
    return false;
  }

  const int source_width = this->buffer_width_;
  const int source_height = this->buffer_height_;
  const int crop_x = this->has_crop_ ? this->crop_x_ : 0;
  const int crop_y = this->has_crop_ ? this->crop_y_ : 0;
  const int crop_width = this->has_crop_ ? this->crop_width_ : source_width;
  const int crop_height = this->has_crop_ ? this->crop_height_ : source_height;

  if (crop_x < 0 || crop_y < 0 || crop_width <= 0 || crop_height <= 0 || crop_x + crop_width > source_width ||
      crop_y + crop_height > source_height) {
    ESP_LOGE(TAG, "Crop %d,%d %dx%d is outside the %dx%d frame", crop_x, crop_y, crop_width, crop_height, source_width,
             source_height);
    return false;
  }

  const bool swap = this->rotation_ == 90 || this->rotation_ == 270;
  const int output_width = swap ? crop_height : crop_width;
  const int output_height = swap ? crop_width : crop_height;

  const size_t pixel_size = (this->get_bpp() + 7) / 8;
  const size_t source_stride = static_cast<size_t>(source_width) * pixel_size;
  const size_t output_stride = static_cast<size_t>(output_width) * pixel_size;
  const size_t output_size = output_stride * static_cast<size_t>(output_height);
  const uint8_t *crop_origin = this->buffer_ + static_cast<size_t>(crop_y) * source_stride + crop_x * pixel_size;

  if (this->rotation_ == 0) {
    // Every output row is one run of a source row, and the destination is never
    // ahead of the source, so the copy can happen in place.
    for (int y = 0; y < output_height; y++) {
      std::memmove(this->buffer_ + static_cast<size_t>(y) * output_stride,
                   crop_origin + static_cast<size_t>(y) * source_stride, output_stride);
    }
  } else {
    const size_t expected_size =
        static_cast<size_t>(this->transform_width_) * static_cast<size_t>(this->transform_height_) * pixel_size;
    if (this->transform_buffer_ == nullptr || output_size > expected_size) {
      ESP_LOGE(TAG, "No buffer allocated for a %dx%d crop/rotation", output_width, output_height);
      return false;
    }
    uint8_t *output = this->transform_buffer_;
    switch (this->rotation_) {
      case 90:
        for (int y = 0; y < output_height; y++) {
          for (int x = 0; x < output_width; x++) {
            std::memcpy(output + static_cast<size_t>(y) * output_stride + x * pixel_size,
                        crop_origin + static_cast<size_t>(crop_height - 1 - x) * source_stride + y * pixel_size,
                        pixel_size);
          }
        }
        break;
      case 180:
        for (int y = 0; y < output_height; y++) {
          for (int x = 0; x < output_width; x++) {
            std::memcpy(output + static_cast<size_t>(y) * output_stride + x * pixel_size,
                        crop_origin + static_cast<size_t>(crop_height - 1 - y) * source_stride +
                            (crop_width - 1 - x) * pixel_size,
                        pixel_size);
          }
        }
        break;
      default:  // 270
        for (int y = 0; y < output_height; y++) {
          for (int x = 0; x < output_width; x++) {
            std::memcpy(output + static_cast<size_t>(y) * output_stride + x * pixel_size,
                        crop_origin + static_cast<size_t>(x) * source_stride + (crop_width - 1 - y) * pixel_size,
                        pixel_size);
          }
        }
        break;
    }
    // Publish from the decoded buffer: it keeps its own size, so the next decode
    // reuses it and no frame allocates.
    std::memcpy(this->buffer_, output, output_size);
  }

  this->decode_width_ = source_width;
  this->decode_height_ = source_height;
  this->buffer_width_ = output_width;
  this->buffer_height_ = output_height;
  return true;
}

void CameraImage::dump_config() {
  ESP_LOGCONFIG(TAG, "Camera Image:");
  ESP_LOGCONFIG(TAG, "  Format: %s", LOG_STR_ARG(runtime_image::get_format_name(this->get_format())));
  ESP_LOGCONFIG(TAG, "  Type: %d", static_cast<int>(this->get_type()));
  if (this->has_crop_) {
    ESP_LOGCONFIG(TAG, "  Crop: %d,%d %dx%d", this->crop_x_, this->crop_y_, this->crop_width_, this->crop_height_);
  }
  if (this->rotation_ != 0) {
    ESP_LOGCONFIG(TAG, "  Rotation: %d", this->rotation_);
    ESP_LOGCONFIG(TAG, "  Rotation buffer: %zu bytes", this->transform_buffer_size_);
  }
  if (this->transform_width_ > 0) {
    ESP_LOGCONFIG(TAG, "  Output size: %dx%d", this->transform_width_, this->transform_height_);
  }
  LOG_UPDATE_INTERVAL(this);
}

}  // namespace esphome::camera_image

#endif  // USE_CAMERA_IMAGE
