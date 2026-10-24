#include "jpeg_decoder.h"
#ifdef USE_RUNTIME_IMAGE_JPEG

#include "esphome/components/display/display_buffer.h"
#include "esphome/components/image/image.h"
#include "esphome/core/application.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#ifdef USE_ESP_IDF
#include "esp_task_wdt.h"
#endif

ESPHOME_LOG_TAG(TAG, "image_decoder.jpeg");

namespace esphome::runtime_image {

bool JpegDecoder::prepare_rgb565_scaling_(int width, int height) {
  if (this->x_boundaries_.capacity() < static_cast<size_t>(width + 1) && !this->x_boundaries_.try_init(width + 1)) {
    return false;
  }
  if (this->y_boundaries_.capacity() < static_cast<size_t>(height + 1) && !this->y_boundaries_.try_init(height + 1)) {
    return false;
  }

  this->source_width_ = width;
  this->source_height_ = height;
  this->x_boundaries_.clear();
  this->y_boundaries_.clear();
  const int target_width = this->image_->get_buffer_width();
  const int target_height = this->image_->get_buffer_height();
  // The same boundaries as the generic draw() path, so an 8 to 4 scale keeps columns 0, 2, 4 and 6 in both
  for (int x = 0; x <= width; x++) {
    this->x_boundaries_.push_back(static_cast<uint16_t>(scale_boundary(x, width, target_width)));
  }
  for (int y = 0; y <= height; y++) {
    this->y_boundaries_.push_back(static_cast<uint16_t>(scale_boundary(y, height, target_height)));
  }
  return true;
}

void JpegDecoder::draw_rgb565(int x, int y, int w, int h, const uint8_t *pixels) {
  if (pixels == nullptr) {
    return;
  }
  uint8_t *const buffer = this->image_buffer_();
  const uint32_t buffer_width = this->image_->get_buffer_width();
  // JPEGDEC gives little-endian words; a big-endian image stores the high byte first
  const uint8_t first = this->image_is_big_endian_() ? 1 : 0;
  for (int source_y = 0; source_y < h; source_y++) {
    const int absolute_y = y + source_y;
    if (absolute_y < 0 || absolute_y >= this->source_height_) {
      continue;
    }
    const uint16_t y1 = this->y_boundaries_[absolute_y];
    const uint16_t y2 = this->y_boundaries_[absolute_y + 1];
    for (int source_x = 0; source_x < w; source_x++) {
      const int absolute_x = x + source_x;
      if (absolute_x < 0 || absolute_x >= this->source_width_) {
        continue;
      }
      const uint16_t x1 = this->x_boundaries_[absolute_x];
      const uint16_t x2 = this->x_boundaries_[absolute_x + 1];
      const uint8_t *pixel = pixels + (source_y * w + source_x) * 2;
      for (uint16_t destination_y = y1; destination_y < y2; destination_y++) {
        uint8_t *destination = buffer + (destination_y * buffer_width + x1) * 2;
        for (uint16_t destination_x = x1; destination_x < x2; destination_x++) {
          destination[0] = pixel[first];
          destination[1] = pixel[1 - first];
          destination += 2;
        }
      }
    }
  }
}

/**
 * @brief Callback method that will be called by the JPEGDEC engine when a chunk
 * of the image is decoded.
 *
 * @param jpeg  The JPEGDRAW object, including the context data.
 */
static int draw_callback(JPEGDRAW *jpeg) {
  JpegDecoder *decoder = (JpegDecoder *) jpeg->pUser;

  if (decoder == nullptr) {
    ESP_LOGE(TAG, "Decoder pointer is null!");
    return 0;
  }

  // Some very big images take too long to decode, so feed the watchdog on each callback
  // to avoid crashing if the executing task has a watchdog enabled.
#ifdef USE_ESP_IDF
  if (esp_task_wdt_status(nullptr) == ESP_OK) {
#endif
    App.feed_wdt();
#ifdef USE_ESP_IDF
  }
#endif
  if (decoder->uses_direct_rgb565()) {
    decoder->draw_rgb565(jpeg->x, jpeg->y, jpeg->iWidth, jpeg->iHeight,
                         reinterpret_cast<const uint8_t *>(jpeg->pPixels));
    return 1;
  }
  size_t position = 0;
  size_t height = static_cast<size_t>(jpeg->iHeight);
  size_t width = static_cast<size_t>(jpeg->iWidth);
  for (size_t y = 0; y < height; y++) {
    for (size_t x = 0; x < width; x++) {
      auto rg = decode_value(jpeg->pPixels[position++]);
      auto ba = decode_value(jpeg->pPixels[position++]);
      Color color(rg[1], rg[0], ba[1], ba[0]);

      decoder->draw(jpeg->x + x, jpeg->y + y, 1, 1, color);
    }
  }
  return 1;
}

int HOT JpegDecoder::decode(uint8_t *buffer, size_t size) {
  // JPEG decoder requires complete data
  // If we know the expected size, wait for it
  if (this->expected_size_ > 0 && size < this->expected_size_) {
    ESP_LOGV(TAG, "Download not complete. Size: %zu/%zu", size, this->expected_size_);
    return 0;
  }

  // If size unknown, try to decode and see if it's valid
  // The JPEGDEC library will fail gracefully if data is incomplete

  if (!this->jpeg_.openRAM(buffer, size, draw_callback)) {
    ESP_LOGE(TAG, "Could not open image for decoding: %d", this->jpeg_.getLastError());
    return DECODE_ERROR_INVALID_TYPE;
  }
  const int err = this->start_();
  if (err != 0)
    return err;
  if (!this->jpeg_.decode(0, 0, this->scale_option_()))
    return this->fail_();
  this->decoded_bytes_ = size;
  this->jpeg_.close();
  return size;
}

int JpegDecoder::start_() {
  auto jpeg_type = this->jpeg_.getJPEGType();
  if (jpeg_type == JPEG_MODE_INVALID) {
    ESP_LOGE(TAG, "Unsupported JPEG image");
    return DECODE_ERROR_INVALID_TYPE;
  } else if (jpeg_type == JPEG_MODE_PROGRESSIVE) {
    ESP_LOGE(TAG, "Progressive JPEG images not supported");
    return DECODE_ERROR_UNSUPPORTED_FORMAT;
  }
  ESP_LOGD(TAG, "Image size: %d x %d, bpp: %d", this->jpeg_.getWidth(), this->jpeg_.getHeight(), this->jpeg_.getBpp());

  this->jpeg_.setUserPointer(this);
  this->direct_rgb565_ = this->image_->get_type() == image::IMAGE_TYPE_RGB565 && !this->image_->has_transparency();
  if (this->direct_rgb565_) {
    // JPEGDEC's RGB565_BIG_ENDIAN output is not byte swapped on every build (the x86 host build
    // returns little-endian words for it), so the byte order is applied in draw_rgb565 instead
    this->jpeg_.setPixelType(RGB565_LITTLE_ENDIAN);
  } else {
    this->jpeg_.setPixelType(RGB8888);
  }
  const int width = this->jpeg_.getWidth();
  const int height = this->jpeg_.getHeight();
  if (!this->set_size(width, height)) {
    return DECODE_ERROR_OUT_OF_MEMORY;
  }
  // JPEGDEC can decode at 1/2, 1/4 or 1/8 size for much less work. Use the smallest size that still has at least as
  // many pixels as the buffer, so a large image shown small is not decoded in full only to be thrown away.
  const int buffer_width = this->image_->get_buffer_width();
  const int buffer_height = this->image_->get_buffer_height();
  this->scale_ = 1;
  while (this->scale_ < JPEG_SCALE_EIGHTH && width / (this->scale_ * 2) >= buffer_width &&
         height / (this->scale_ * 2) >= buffer_height) {
    this->scale_ *= 2;
  }
  const int decoded_width = (width + this->scale_ - 1) / this->scale_;
  const int decoded_height = (height + this->scale_ - 1) / this->scale_;
  this->draw_width_ = decoded_width;
  this->draw_height_ = decoded_height;
  if (this->scale_ != 1) {
    ESP_LOGD(TAG, "Decoding at 1/%d size, %d x %d", this->scale_, decoded_width, decoded_height);
  }
  if (this->direct_rgb565_ && !this->prepare_rgb565_scaling_(decoded_width, decoded_height)) {
    return DECODE_ERROR_OUT_OF_MEMORY;
  }
  return 0;
}

int JpegDecoder::fail_() {
  const bool read_failed = this->source_ != nullptr && this->source_failed_;
  const int error = this->jpeg_.getLastError();
  this->jpeg_.close();
  this->source_ = nullptr;
  if (read_failed) {
    ESP_LOGE(TAG, "Reading the image failed");
    return DECODE_ERROR_READ;
  }
  ESP_LOGE(TAG, "Error while decoding: %d", error);
  switch (error) {
    case JPEG_ERROR_MEMORY:
      return DECODE_ERROR_OUT_OF_MEMORY;
    case JPEG_UNSUPPORTED_FEATURE:
      return DECODE_ERROR_UNSUPPORTED_FORMAT;
    case JPEG_INVALID_FILE:
    case JPEG_INVALID_PARAMETER:
      return DECODE_ERROR_INVALID_TYPE;
    case JPEG_DECODE_ERROR:
    default:
      return DECODE_ERROR_INTERNAL_DECODER_ERROR;
  }
}

void JpegDecoder::reset() {
  // A decode from a source that was given up part way still has the image open
  if (this->source_ != nullptr) {
    this->jpeg_.close();
    this->source_ = nullptr;
  }
  ImageDecoder::reset();
}

int32_t JpegDecoder::source_read(JPEGFILE *file, uint8_t *buf, int32_t len) {
  auto *decoder = static_cast<JpegDecoder *>(file->fHandle);
  const int32_t n = decoder->source_->read(buf, len);
  if (n < 0) {
    // JPEGDEC has no way to report a failed read, so remember it and give it no data
    decoder->source_failed_ = true;
    return 0;
  }
  file->iPos += n;
  return n;
}

int32_t JpegDecoder::source_seek(JPEGFILE *file, int32_t position) {
  auto *decoder = static_cast<JpegDecoder *>(file->fHandle);
  if (position < 0 || !decoder->source_->seek(position)) {
    decoder->source_failed_ = true;
    return file->iPos;
  }
  file->iPos = position;
  return position;
}

int JpegDecoder::begin_source(DecodeSource *source) {
  if (this->expected_size_ == 0 || this->expected_size_ > INT32_MAX) {
    ESP_LOGE(TAG, "Image size %zu cannot be read from a source", this->expected_size_);
    return DECODE_ERROR_INVALID_TYPE;
  }
  if (!source->seek(0)) {
    ESP_LOGE(TAG, "Could not read the image");
    return DECODE_ERROR_READ;
  }
  this->source_ = source;
  this->source_failed_ = false;
  if (!this->jpeg_.open(this, this->expected_size_, source_close, source_read, source_seek, draw_callback)) {
    // The caller may recover, for example by reading the whole image first, so it reports the failure
    ESP_LOGD(TAG, "Could not open image for decoding: %d", this->jpeg_.getLastError());
    this->source_ = nullptr;
    return this->source_failed_ ? DECODE_ERROR_READ : DECODE_ERROR_INVALID_TYPE;
  }
  const int err = this->start_();
  if (err != 0) {
    this->jpeg_.close();
    this->source_ = nullptr;
    return err;
  }
  if (!this->jpeg_.decodeBegin(0, 0, this->scale_option_()))
    return this->fail_();
  return 0;
}

int HOT JpegDecoder::decode_step() {
  if (this->source_ == nullptr)
    return DECODE_ERROR_INVALID_TYPE;
  const int result = this->jpeg_.decodeRows(1);
  // JPEGDEC may decode on with the missing data, so check the read before its result
  if (this->source_failed_)
    return this->fail_();
  if (result == JPEG_DECODE_MORE)
    return DECODE_STEP_MORE;
  if (result == JPEG_DECODE_NEED_DATA)
    return DECODE_STEP_NEED_DATA;
  if (result == 0)
    return this->fail_();
  this->decoded_bytes_ = this->expected_size_;
  this->jpeg_.close();
  this->source_ = nullptr;
  return DECODE_STEP_DONE;
}

}  // namespace esphome::runtime_image

#endif  // USE_RUNTIME_IMAGE_JPEG
