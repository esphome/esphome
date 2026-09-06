#include "qoi_decoder.h"

#ifdef USE_RUNTIME_IMAGE_QOI

#include "esphome/components/display/display.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <cinttypes>

namespace esphome::runtime_image {

static const char *const TAG = "image_decoder.qoi";

constexpr uint8_t QOI_OP_RGB = 0b11111110;
constexpr uint8_t QOI_OP_RGBA = 0b11111111;
constexpr uint8_t QOI_OP_INDEX = 0b00000000;  // 00xxxxxx
constexpr uint8_t QOI_OP_DIFF = 0b01000000;   // 01xxxxxx
constexpr uint8_t QOI_OP_LUMA = 0b10000000;   // 10xxxxxx
constexpr uint8_t QOI_OP_RUN = 0b11000000;    // 11xxxxxx

constexpr uint8_t QOI_RGB_CHUNK_SIZE = 4;
constexpr uint8_t QOI_RGBA_CHUNK_SIZE = 5;
constexpr uint8_t QOI_LUMA_CHUNK_SIZE = 2;

constexpr uint8_t QOI_MASK_OP = 0b11000000;
constexpr uint8_t QOI_MASK_VALUE = 0b00111111;

constexpr size_t QOI_HEADER_SIZE = 14;
constexpr size_t QOI_COLOR_TABLE_SIZE = 64;

inline size_t qoi_color_table_index(const Color &color) {
  // QOI color hash function: (r * 3 + g * 5 + b * 7 + a * 11) % 64
  return (color.r * 3 + color.g * 5 + color.b * 7 + color.w * 11) &
         63;  // modulo 64 is equivalent to bitwise AND with 63 (0b00111111)
}

void QoiDecoder::reset() {
  ImageDecoder::reset();
  this->current_index_ = 0;
  this->paint_index_ = 0;
  this->width_ = 0;
  this->height_ = 0;
  this->bits_per_pixel_ = 0;
  this->last_pixel_ = Color(0, 0, 0, 255);
  if (this->color_table_) {
    std::fill_n(this->color_table_.get(), QOI_COLOR_TABLE_SIZE, Color());
  }
}

int HOT QoiDecoder::decode(uint8_t *buffer, size_t size) {
  size_t index = 0;
  if (this->current_index_ == 0) {
    if (size < QOI_HEADER_SIZE) {
      return 0;  // Need more data for file header
    }

    /** QOI Header definition, for reference:
        char magic[4];  // magic bytes "qoif"
        uint32_t width; // image width in pixels (BE)
        uint32_t height; // image height in pixels (BE)
        uint8_t channels;  // 3 = RGB, 4 = RGBA
        uint8_t colorspace;  // 0 = sRGB with linear alpha, 1 = all channels linear
    */
    // Check if the file is a QOI image
    if (buffer[0] != 'q' || buffer[1] != 'o' || buffer[2] != 'i' || buffer[3] != 'f') {
      ESP_LOGE(TAG, "Not a QOI file");
      return DECODE_ERROR_INVALID_TYPE;
    }

    this->width_ = encode_uint32(buffer[4], buffer[5], buffer[6], buffer[7]);
    this->height_ = encode_uint32(buffer[8], buffer[9], buffer[10], buffer[11]);
    if (this->width_ == 0 || this->height_ == 0) {
      ESP_LOGE(TAG, "Invalid image dimensions: (%zux%zu)", this->width_, this->height_);
      return DECODE_ERROR_INVALID_TYPE;
    }
    uint8_t channels = buffer[12];
    if (channels < 3 || channels > 4) {
      ESP_LOGE(TAG, "Unsupported number of channels: %d", channels);
      return DECODE_ERROR_UNSUPPORTED_FORMAT;
    }
    this->bits_per_pixel_ = channels * 8;
    uint8_t colorspace = buffer[13];
    if (colorspace > 1) {
      ESP_LOGE(TAG, "Unsupported colorspace value: %d", colorspace);
      return DECODE_ERROR_UNSUPPORTED_FORMAT;
    }
    ESP_LOGD(TAG, "QOI image header: width=%zu, height=%zu, channels=%d, colorspace=%d", this->width_, this->height_,
             channels, colorspace);

    if (!this->color_table_) {
      this->color_table_ = std::make_unique<Color[]>(QOI_COLOR_TABLE_SIZE);
    }

    if (!this->set_size(this->width_, this->height_)) {
      return DECODE_ERROR_OUT_OF_MEMORY;
    }

    this->current_index_ = QOI_HEADER_SIZE;
    index = QOI_HEADER_SIZE;
  }  // Current_index == 0

  Color color;
  const size_t total_pixels = this->width_ * this->height_;
  while (index < size && this->paint_index_ < total_pixels) {
    color = this->last_pixel_;
    uint8_t byte = buffer[index];
    if (byte == QOI_OP_RGB) {
      if (size < index + QOI_RGB_CHUNK_SIZE) {
        return index;  // Need more data for RGB chunk
      }
      index++;
      color.r = buffer[index++];
      color.g = buffer[index++];
      color.b = buffer[index++];
      this->draw(this->paint_index_ % this->width_, this->paint_index_ / this->width_, 1, 1, color);
      this->paint_index_++;
    } else if (byte == QOI_OP_RGBA) {
      if (size < index + QOI_RGBA_CHUNK_SIZE) {
        return index;  // Need more data for RGBA chunk
      }
      index++;
      color.r = buffer[index++];
      color.g = buffer[index++];
      color.b = buffer[index++];
      color.w = buffer[index++];
      this->draw(this->paint_index_ % this->width_, this->paint_index_ / this->width_, 1, 1, color);
      this->paint_index_++;
    } else if ((byte & QOI_MASK_OP) == QOI_OP_RUN) {
      // QOI run chunk
      size_t run_length = (byte & QOI_MASK_VALUE) + 1;  // run length is encoded in the lower 6 bits, plus one
      for (size_t i = 0; i < run_length; i++) {
        // TODO: optimize by drawing runs of pixels at once instead of one by one
        this->draw(this->paint_index_ % this->width_, this->paint_index_ / this->width_, 1, 1, color);
        this->paint_index_++;
      }
      index++;
    } else if ((byte & QOI_MASK_OP) == QOI_OP_LUMA) {
      if (size < index + QOI_LUMA_CHUNK_SIZE) {
        return index;  // Need more data for LUMA chunk
      }
      index++;
      uint8_t byte2 = buffer[index++];
      uint8_t delta_g = (byte & QOI_MASK_VALUE) - 32;
      color.r += delta_g - 8 + ((byte2 >> 4) & 0x0f);
      color.g += delta_g;
      color.b += delta_g - 8 + (byte2 & 0x0f);

      this->draw(this->paint_index_ % this->width_, this->paint_index_ / this->width_, 1, 1, color);
      this->paint_index_++;
    } else if ((byte & QOI_MASK_OP) == QOI_OP_DIFF) {
      color.r += ((byte >> 4) & 0x03) - 2;
      color.g += ((byte >> 2) & 0x03) - 2;
      color.b += (byte & 0x03) - 2;

      this->draw(this->paint_index_ % this->width_, this->paint_index_ / this->width_, 1, 1, color);
      this->paint_index_++;
      index++;
    } else if ((byte & QOI_MASK_OP) == QOI_OP_INDEX) {
      color = this->color_table_[byte];
      this->draw(this->paint_index_ % this->width_, this->paint_index_ / this->width_, 1, 1, color);
      this->paint_index_++;
      index++;
    }
    this->last_pixel_ = color;
    this->color_table_[qoi_color_table_index(color)] = color;
  }
  this->decoded_bytes_ += size;
  return size;
}

}  // namespace esphome::runtime_image

#endif  // USE_RUNTIME_IMAGE_QOI
