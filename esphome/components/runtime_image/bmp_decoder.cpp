#include "bmp_decoder.h"

#ifdef USE_RUNTIME_IMAGE_BMP

#include "esphome/components/display/display.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::runtime_image {

static const char *const TAG = "image_decoder.bmp";

int HOT BmpDecoder::decode(uint8_t *buffer, size_t size) {
  size_t index = 0;
  if (this->current_index_ == 0) {
    if (size <= 14) {
      return 0;  // Need more data for file header
    }
    /**
     * BMP file format:
     * 0-1: Signature (BM)
     * 2-5: File size
     * 6-9: Reserved
     * 10-13: Pixel data offset
     *
     * Integer values are stored in little-endian format.
     */

    // Check if the file is a BMP image
    if (buffer[0] != 'B' || buffer[1] != 'M') {
      ESP_LOGE(TAG, "Not a BMP file");
      return DECODE_ERROR_INVALID_TYPE;
    }

    // BMP file contains its own size in the header
    size_t file_size = encode_uint32(buffer[5], buffer[4], buffer[3], buffer[2]);
    if (this->expected_size_ == 0) {
      this->expected_size_ = file_size;  // Use file header size if not provided
    }
    this->data_offset_ = encode_uint32(buffer[13], buffer[12], buffer[11], buffer[10]);

    this->current_index_ = 14;
    index = 14;
  }
  if (this->current_index_ == 14) {
    if (size <= this->data_offset_) {
      return 0;  // Need more data for DIB header and color table
    }
    /**
     * BMP DIB header:
     * 14-17: DIB header size
     * 18-21: Image width
     * 22-25: Image height
     * 26-27: Number of color planes
     * 28-29: Bits per pixel
     * 30-33: Compression method
     * 34-37: Image data size
     * 38-41: Horizontal resolution
     * 42-45: Vertical resolution
     * 46-49: Number of colors in the color table
     */

    this->width_ = encode_uint32(buffer[21], buffer[20], buffer[19], buffer[18]);
    this->height_ = encode_uint32(buffer[25], buffer[24], buffer[23], buffer[22]);
    this->bits_per_pixel_ = encode_uint16(buffer[29], buffer[28]);
    this->compression_method_ = encode_uint32(buffer[33], buffer[32], buffer[31], buffer[30]);
    this->image_data_size_ = encode_uint32(buffer[37], buffer[36], buffer[35], buffer[34]);
    this->color_table_entries_ = encode_uint32(buffer[49], buffer[48], buffer[47], buffer[46]);

    switch (this->bits_per_pixel_) {
      case 1:
        this->width_bytes_ = (this->width_ + 7) / 8;
        this->padding_bytes_ = (4 - (this->width_bytes_ % 4)) % 4;
        break;
      case 8: {
        this->width_bytes_ = this->width_;
        if (this->color_table_entries_ == 0) {
          this->color_table_entries_ = 256;
        } else if (this->color_table_entries_ > 256) {
          ESP_LOGE(TAG, "Too many color table entries: %" PRIu32, this->color_table_entries_);
          return DECODE_ERROR_UNSUPPORTED_FORMAT;
        }
        size_t header_size = encode_uint32(buffer[17], buffer[16], buffer[15], buffer[14]);
        size_t offset = 14 + header_size;

        this->color_table_ = std::make_unique<uint32_t[]>(this->color_table_entries_);

        for (size_t i = 0; i < this->color_table_entries_; i++) {
          this->color_table_[i] = encode_uint32(buffer[offset + i * 4 + 3], buffer[offset + i * 4 + 2],
                                                buffer[offset + i * 4 + 1], buffer[offset + i * 4]);
        }

        this->padding_bytes_ = (4 - (this->width_bytes_ % 4)) % 4;

        break;
      }
      case 24:
        this->width_bytes_ = this->width_ * 3;
        if (this->width_bytes_ % 4 != 0) {
          this->padding_bytes_ = 4 - (this->width_bytes_ % 4);
          this->width_bytes_ += this->padding_bytes_;
        }
        break;
      default:
        ESP_LOGE(TAG, "Unsupported bits per pixel: %d", this->bits_per_pixel_);
        return DECODE_ERROR_UNSUPPORTED_FORMAT;
    }

    if (this->compression_method_ != 0) {
      ESP_LOGE(TAG, "Unsupported compression method: %d", this->compression_method_);
      return DECODE_ERROR_UNSUPPORTED_FORMAT;
    }

    if (!this->set_size(this->width_, this->height_)) {
      return DECODE_ERROR_OUT_OF_MEMORY;
    }
    this->current_index_ = this->data_offset_;
    index = this->data_offset_;
  }
  switch (this->bits_per_pixel_) {
    case 1: {
      size_t width = static_cast<size_t>(this->width_);
      while (index < size) {
        size_t x = this->paint_index_ % width;
        size_t y = static_cast<size_t>(this->height_ - 1) - (this->paint_index_ / width);
        size_t remaining_in_row = width - x;
        uint8_t pixels_in_byte = std::min<size_t>(remaining_in_row, 8);
        bool end_of_row = remaining_in_row <= 8;
        size_t needed = 1 + (end_of_row ? this->padding_bytes_ : 0);
        if (index + needed > size) {
          this->decoded_bytes_ += index;
          return index;
        }
        uint8_t current_byte = buffer[index];
        for (uint8_t i = 0; i < pixels_in_byte; i++) {
          Color c = (current_byte & (1 << (7 - i))) ? display::COLOR_ON : display::COLOR_OFF;
          this->draw(x + i, y, 1, 1, c);
        }
        this->paint_index_ += pixels_in_byte;
        this->current_index_++;
        index++;
        // End of pixel row: skip row padding bytes (4-byte alignment)
        if (end_of_row && this->padding_bytes_ > 0) {
          index += this->padding_bytes_;
          this->current_index_ += this->padding_bytes_;
        }
      }
      break;
    }
    case 8: {
      size_t width = static_cast<size_t>(this->width_);
      size_t last_col = width - 1;
      while (index < size) {
        size_t x = this->paint_index_ % width;
        size_t y = static_cast<size_t>(this->height_ - 1) - (this->paint_index_ / width);
        size_t needed = 1 + ((x == last_col) ? this->padding_bytes_ : 0);
        if (index + needed > size) {
          this->decoded_bytes_ += index;
          return index;
        }

        uint8_t color_index = buffer[index];
        if (color_index >= this->color_table_entries_) {
          ESP_LOGE(TAG, "Invalid color index: %u", color_index);
          return DECODE_ERROR_UNSUPPORTED_FORMAT;
        }

        uint32_t rgb = this->color_table_[color_index];
        uint8_t b = rgb & 0xff;
        uint8_t g = (rgb >> 8) & 0xff;
        uint8_t r = (rgb >> 16) & 0xff;
        this->draw(x, y, 1, 1, Color(r, g, b));
        this->paint_index_++;
        this->current_index_++;
        index++;
        if (x == last_col && this->padding_bytes_ > 0) {
          index += this->padding_bytes_;
          this->current_index_ += this->padding_bytes_;
        }
      }
      break;
    }
    case 24: {
      size_t width = static_cast<size_t>(this->width_);
      size_t last_col = width - 1;
      while (index < size) {
        size_t x = this->paint_index_ % width;
        size_t y = static_cast<size_t>(this->height_ - 1) - (this->paint_index_ / width);
        size_t needed = 3 + ((x == last_col) ? this->padding_bytes_ : 0);
        if (index + needed > size) {
          this->decoded_bytes_ += index;
          return index;
        }
        uint8_t b = buffer[index];
        uint8_t g = buffer[index + 1];
        uint8_t r = buffer[index + 2];
        this->draw(x, y, 1, 1, Color(r, g, b));
        this->paint_index_++;
        this->current_index_ += 3;
        index += 3;
        if (x == last_col && this->padding_bytes_ > 0) {
          index += this->padding_bytes_;
          this->current_index_ += this->padding_bytes_;
        }
      }
      break;
    }
    default:
      ESP_LOGE(TAG, "Unsupported bits per pixel: %d", this->bits_per_pixel_);
      return DECODE_ERROR_UNSUPPORTED_FORMAT;
  }
  this->decoded_bytes_ += size;
  return size;
};

}  // namespace esphome::runtime_image

#endif  // USE_RUNTIME_IMAGE_BMP
