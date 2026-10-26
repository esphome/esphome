#include "runtime_image.h"
#include "image_decoder.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include <algorithm>
#include <utility>
#include <cstdint>
#include <cstring>
#include <limits>

#ifdef USE_RUNTIME_IMAGE_BMP
#include "bmp_decoder.h"
#endif
#ifdef USE_RUNTIME_IMAGE_JPEG
#ifdef USE_RUNTIME_IMAGE_JPEG_TURBO
#include "jpeg_turbo_decoder.h"
#else  // USE_RUNTIME_IMAGE_JPEG_TURBO
#include "jpeg_decoder.h"
#endif  // USE_RUNTIME_IMAGE_JPEG_TURBO
#endif  // USE_RUNTIME_IMAGE_JPEG
#ifdef USE_RUNTIME_IMAGE_PNG
#include "png_decoder.h"
#endif
#ifdef USE_RUNTIME_IMAGE_QOI
#include "qoi_decoder.h"
#endif

namespace esphome::runtime_image {

ESPHOME_LOG_TAG(TAG, "runtime_image");

// Widest supported format is 4 bytes/pixel, so 32767 * 32767 * 4 still fits a 32-bit size_t
static constexpr int MAX_IMAGE_DIMENSION = 32767;
static constexpr int MAX_IMAGE_BPP = 32;
static_assert((static_cast<uint64_t>(MAX_IMAGE_BPP) * MAX_IMAGE_DIMENSION + 7) / 8 * MAX_IMAGE_DIMENSION <=
                  std::numeric_limits<size_t>::max(),
              "MAX_IMAGE_DIMENSION must keep the worst-case buffer size within size_t");

inline bool is_color_on(const Color &color) {
  // This produces the most accurate monochrome conversion, but is slightly slower.
  //  return (0.2125 * color.r + 0.7154 * color.g + 0.0721 * color.b) > 127;

  // Approximation using fast integer computations; produces acceptable results
  // Equivalent to 0.25 * R + 0.5 * G + 0.25 * B
  return ((color.r >> 2) + (color.g >> 1) + (color.b >> 2)) & 0x80;
}

RuntimeImage::RuntimeImage(ImageFormat format, image::ImageType type, image::Transparency transparency,
                           image::Image *placeholder, bool is_big_endian, int fixed_width, int fixed_height)
    : Image(nullptr, 0, 0, type, transparency, is_big_endian),
      format_(format),
      fixed_width_(fixed_width),
      fixed_height_(fixed_height),
      placeholder_(placeholder) {}

RuntimeImage::~RuntimeImage() {
#ifdef USE_LVGL_IMAGE
  for (lv_obj_t *obj : this->lvgl_widgets_)
    lv_obj_remove_event_cb_with_user_data(obj, lvgl_widget_deleted, this);
  this->lvgl_widgets_.clear();
#endif
  this->release();
}

int RuntimeImage::resize(int width, int height) {
  // Use fixed dimensions if specified (0 means auto-resize)
  int target_width = this->fixed_width_ ? this->fixed_width_ : width;
  int target_height = this->fixed_height_ ? this->fixed_height_ : height;

  // When both fixed dimensions are set, scale uniformly to preserve aspect ratio
  if (this->fixed_width_ && this->fixed_height_ && width > 0 && height > 0) {
    float scale =
        std::min(static_cast<float>(this->fixed_width_) / width, static_cast<float>(this->fixed_height_) / height);
    target_width = static_cast<int>(width * scale);
    target_height = static_cast<int>(height * scale);
  }

  size_t result = this->resize_buffer_(target_width, target_height);
  if (result > 0 && this->progressive_display_) {
    // Update display dimensions for progressive display
    this->width_ = this->buffer_width_;
    this->height_ = this->buffer_height_;
    this->data_start_ = this->buffer_;
    this->refresh_lvgl_();
  }
  return result;
}

void RuntimeImage::draw_pixel(int x, int y, const Color &color) {
  if (!this->buffer_) {
    ESP_LOGE(TAG, "Buffer not allocated!");
    return;
  }
  if (x < 0 || y < 0 || x >= this->buffer_width_ || y >= this->buffer_height_) {
    ESP_LOGE(TAG, "Tried to paint a pixel (%d,%d) outside the image!", x, y);
    return;
  }

  switch (this->type_) {
    case image::IMAGE_TYPE_BINARY: {
      const uint32_t width_8 = ((this->buffer_width_ + 7u) / 8u) * 8u;
      uint32_t pos = x + y * width_8;
      auto bitno = 0x80 >> (pos % 8u);
      pos /= 8u;
      auto on = is_color_on(color);
      if (this->has_transparency() && color.w < 0x80)
        on = false;
      assign_bits(this->buffer_[pos], bitno, on);
      break;
    }
    case image::IMAGE_TYPE_RGB111: {
      uint8_t bits = (color.r >> 7) << 2 | (color.g >> 7) << 1 | (color.b >> 7);
      if (this->bpp_ == 4)
        bits = bits << 1 | (color.w >> 7);
      const uint32_t stride = (this->buffer_width_ * this->bpp_ + 7u) / 8u;
      const uint32_t bitpos = y * stride * 8 + x * this->bpp_;
      uint8_t *byte = this->buffer_ + bitpos / 8;
      // Place the pixel in a 16-bit window so it may straddle two bytes.
      const uint8_t shift = 16 - this->bpp_ - bitpos % 8;
      const uint16_t mask = ((1u << this->bpp_) - 1) << shift;
      const uint16_t value = bits << shift;
      byte[0] = (byte[0] & ~(mask >> 8)) | (value >> 8);
      if (mask & 0xFF)
        byte[1] = (byte[1] & ~(mask & 0xFF)) | (value & 0xFF);
      break;
    }
    case image::IMAGE_TYPE_GRAYSCALE: {
      uint32_t pos = this->get_position_(x, y);
      // 0.2125, 0.7154 and 0.0721 in 1/256ths, summing to 256 so white stays 255. Integer maths: the S3 has no
      // double FPU
      auto gray = static_cast<uint8_t>((54 * color.r + 183 * color.g + 19 * color.b) >> 8);
      if (this->transparency_ == image::TRANSPARENCY_CHROMA_KEY) {
        if (gray == 1) {
          gray = 0;
        }
        if (color.w < 0x80) {
          gray = 1;
        }
      } else if (this->transparency_ == image::TRANSPARENCY_ALPHA_CHANNEL) {
        if (color.w != 0xFF)
          gray = color.w;
      }
      this->buffer_[pos] = gray;
      break;
    }
    case image::IMAGE_TYPE_RGB565: {
      const size_t pos = (x + y * this->buffer_width_) * 2;
      Color mapped_color = color;
      this->map_chroma_key(mapped_color);
      uint16_t rgb565 = display::ColorUtil::color_to_565(mapped_color);
      if (this->big_endian_) {
        this->buffer_[pos + 0] = static_cast<uint8_t>((rgb565 >> 8) & 0xFF);
        this->buffer_[pos + 1] = static_cast<uint8_t>(rgb565 & 0xFF);
      } else {
        this->buffer_[pos + 0] = static_cast<uint8_t>(rgb565 & 0xFF);
        this->buffer_[pos + 1] = static_cast<uint8_t>((rgb565 >> 8) & 0xFF);
      }
      if (this->transparency_ == image::TRANSPARENCY_ALPHA_CHANNEL) {
        const size_t alpha_pos = pos / 2 + this->buffer_width_ * this->buffer_height_ * 2;
        this->buffer_[alpha_pos] = color.w;
      }
      break;
    }
    case image::IMAGE_TYPE_RGB: {
      uint32_t pos = this->get_position_(x, y);
      Color mapped_color = color;
      this->map_chroma_key(mapped_color);
      this->buffer_[pos + 0] = mapped_color.b;
      this->buffer_[pos + 1] = mapped_color.g;
      this->buffer_[pos + 2] = mapped_color.r;
      if (this->transparency_ == image::TRANSPARENCY_ALPHA_CHANNEL) {
        this->buffer_[pos + 3] = color.w;
      }
      break;
    }
  }
}

void RuntimeImage::map_chroma_key(Color &color) {
  if (this->transparency_ == image::TRANSPARENCY_CHROMA_KEY) {
    if (color.g == 1 && color.r == 0 && color.b == 0) {
      color.g = 0;
    }
    if (color.w < 0x80) {
      color.r = 0;
      color.g = this->type_ == image::IMAGE_TYPE_RGB565 ? 4 : 1;
      color.b = 0;
    }
  }
}

#ifdef USE_LVGL
lv_image_dsc_t *RuntimeImage::get_lv_image_dsc() {
  if (this->data_start_ == nullptr && this->placeholder_ != nullptr) {
    this->dsc_ = *this->placeholder_->get_lv_image_dsc();
    return &this->dsc_;
  }
  return Image::get_lv_image_dsc();
}
#endif

#ifdef USE_LVGL_IMAGE
void RuntimeImage::add_lvgl_widget(lv_obj_t *obj) {
  if (std::find(this->lvgl_widgets_.begin(), this->lvgl_widgets_.end(), obj) != this->lvgl_widgets_.end())
    return;
  this->lvgl_widgets_.push_back(obj);
  lv_obj_add_event_cb(obj, lvgl_widget_deleted, LV_EVENT_DELETE, this);
}

void RuntimeImage::lvgl_widget_deleted(lv_event_t *event) {
  auto *image = static_cast<RuntimeImage *>(lv_event_get_user_data(event));
  auto &widgets = image->lvgl_widgets_;
  widgets.erase(std::remove(widgets.begin(), widgets.end(), lv_event_get_target_obj(event)), widgets.end());
}
#endif

void RuntimeImage::refresh_lvgl_() {
#ifdef USE_LVGL
  // Not a virtual call: this also runs from the destructor
  lv_image_dsc_t *dsc = this->RuntimeImage::get_lv_image_dsc();
#ifdef USE_LVGL_IMAGE
  auto &widgets = this->lvgl_widgets_;
  for (size_t i = 0; i < widgets.size();) {
    lv_obj_t *obj = widgets[i];
    if (lv_image_get_src(obj) == dsc) {
      // Setting the same source makes the widget read the size again, and redraws it
      ::lv_image_set_src(obj, dsc);
      i++;
    } else {
      lv_obj_remove_event_cb_with_user_data(obj, lvgl_widget_deleted, this);
      widgets.erase(widgets.begin() + i);
    }
  }
#endif
#endif
}

void RuntimeImage::draw(int x, int y, display::Display *display, Color color_on, Color color_off) {
  if (this->data_start_) {
    // If we have a complete image, use the base class draw method
    Image::draw(x, y, display, color_on, color_off);
  } else if (this->placeholder_) {
    // Show placeholder while the runtime image is not available
    this->placeholder_->draw(x, y, display, color_on, color_off);
  }
  // If no image is loaded and no placeholder, nothing to draw
}

bool RuntimeImage::begin_decode(size_t expected_size, ImageFormat format) {
  if (this->is_decoding()) {
    ESP_LOGW(TAG, "Decoding already in progress");
    return false;
  }

  if (format == AUTO && this->format_ != AUTO) {
    // Fall back to the configured format before the reuse check below
    format = this->format_;
  }

  // An idle decoder for a different format cannot be reused
  if (this->decoder_ != nullptr && this->decoder_->get_format() != format) {
    ESP_LOGD(TAG, "Decoder format mismatch: current: %s, new: %s",
             LOG_STR_ARG(get_format_name(this->decoder_->get_format())), LOG_STR_ARG(get_format_name(format)));
    this->decoder_ = nullptr;
  }

  if (!this->decoder_) {
    this->decoder_ = this->create_decoder_(format);
    if (!this->decoder_) {
      ESP_LOGE(TAG, "Failed to create decoder for format %s", LOG_STR_ARG(get_format_name(format)));
      return false;
    }
  }

  this->decoded_bytes_ = 0;

  int result = this->decoder_->prepare(expected_size);
  if (result < 0) {
    ESP_LOGE(TAG, "Failed to prepare decoder: %d", result);
    this->decoder_ = nullptr;  // If prepare fails, a full reset is needed
    return false;
  }
  return true;
}

int RuntimeImage::feed_data(uint8_t *data, size_t len) {
  if (!this->is_decoding()) {
    ESP_LOGE(TAG, "No decoder initialized");
    return -1;
  }

  int consumed = this->decoder_->decode(data, len);
  if (consumed > 0) {
    this->decoded_bytes_ += consumed;
  }

  return consumed;
}

int RuntimeImage::begin_source(DecodeSource *source) {
  if (!this->decoder_supports_source()) {
    ESP_LOGE(TAG, "Decoder cannot read a source");
    return DECODE_ERROR_UNSUPPORTED_FORMAT;
  }
  return this->decoder_->begin_source(source);
}

int RuntimeImage::decode_step() {
  if (!this->is_decoding()) {
    ESP_LOGE(TAG, "No decoder initialized");
    return DECODE_ERROR_INVALID_TYPE;
  }
  const int result = this->decoder_->decode_step();
  if (result == DECODE_STEP_DONE)
    this->decoded_bytes_ = this->decoder_->get_decoded_bytes();
  return result;
}

bool RuntimeImage::end_decode() {
  if (!this->is_decoding()) {
    return false;
  }

  // Finalize the image for display
  if (this->is_double_buffered_()) {
    if (this->buffer_ != nullptr) {
      // Show the new image, and decode into the old one's buffer next time
      std::swap(this->front_buffer_, this->buffer_);
      std::swap(this->width_, this->buffer_width_);
      std::swap(this->height_, this->buffer_height_);
      this->data_start_ = this->front_buffer_;
      this->refresh_lvgl_();
    }
  } else if (!this->progressive_display_) {
    // Only now make the image visible
    this->width_ = this->buffer_width_;
    this->height_ = this->buffer_height_;
    this->data_start_ = this->buffer_;
    this->refresh_lvgl_();
  }

  // End the session; the decoder object stays warm so the next decode can
  // reuse it (and its buffers) without churning the heap.
  this->decoder_->reset();

  ESP_LOGD(TAG, "Decoding complete: %dx%d, %zu bytes", this->width_, this->height_, this->decoded_bytes_);
  return true;
}

bool RuntimeImage::is_decode_finished() const { return this->is_decoding() && this->decoder_->is_finished(); }

bool RuntimeImage::decoder_knows_end() const { return this->is_decoding() && this->decoder_->knows_end(); }

void RuntimeImage::release() {
  this->release_buffer_();
  if (this->front_buffer_) {
    RAMAllocator<uint8_t> allocator;
    allocator.deallocate(this->front_buffer_, this->get_buffer_size(this->width_, this->height_));
    this->front_buffer_ = nullptr;
    this->data_start_ = nullptr;
    this->width_ = 0;
    this->height_ = 0;
    this->refresh_lvgl_();
  }
  // End any active decode session; decoders free the format-specific working buffers
  // they can (PNG), while the decoder object itself is kept warm for the next decode.
  if (this->decoder_) {
    this->decoder_->reset();
  }
}

void RuntimeImage::abort_decode() {
  if (!this->is_double_buffered_()) {
    this->release();
    return;
  }
  if (this->decoder_) {
    this->decoder_->reset();
  }
}

void RuntimeImage::release_buffer_() {
  if (this->buffer_) {
    if (this->external_buffer_) {
      // The caller owns this memory and goes on using it after the image lets go of it.
      ESP_LOGV(TAG, "Letting go of the external %dx%d buffer", this->buffer_width_, this->buffer_height_);
      this->external_buffer_ = false;
    } else {
      ESP_LOGV(TAG, "Releasing buffer of size %zu", this->get_buffer_size(this->buffer_width_, this->buffer_height_));
      RAMAllocator<uint8_t> allocator;
      allocator.deallocate(this->buffer_, this->get_buffer_size(this->buffer_width_, this->buffer_height_));
    }
    const bool shown = this->data_start_ == this->buffer_;
    this->buffer_ = nullptr;
    this->buffer_width_ = 0;
    this->buffer_height_ = 0;
    // With double buffering the buffer decoded into is not the one on show
    if (shown) {
      this->data_start_ = nullptr;
      this->width_ = 0;
      this->height_ = 0;
      this->refresh_lvgl_();
    }
  }
}

bool RuntimeImage::set_external_buffer(uint8_t *buffer, int width, int height) {
  if (this->is_double_buffered_()) {
    ESP_LOGE(TAG, "An external buffer cannot be double buffered");
    return false;
  }
  this->release_buffer_();
  if (buffer == nullptr || this->get_buffer_size(width, height) == 0) {
    // Keep the released state rather than remembering a buffer that cannot be decoded into: an
    // external buffer that is never handed back would otherwise block every later allocation.
    ESP_LOGE(TAG, "Refusing an invalid external buffer for %dx%d", width, height);
    return false;
  }
  this->buffer_ = buffer;
  this->external_buffer_ = true;
  this->buffer_width_ = width;
  this->buffer_height_ = height;
  return true;
}

size_t RuntimeImage::resize_buffer_(int width, int height) {
  size_t new_size = this->get_buffer_size(width, height);

  // A buffer only ever exists with dimensions the image can decode at, so a match here means
  // new_size is non-zero. Checking it before the invalid dimension case below lets the external
  // buffer be let go of for every decode it cannot serve, not just for valid other dimensions.
  if (this->buffer_ && this->buffer_width_ == width && this->buffer_height_ == height) {
    // Buffer already allocated with correct size
    return new_size;
  }

  if (this->external_buffer_) {
    ESP_LOGE(TAG, "Image decoded to %dx%d, but the external buffer is %dx%d", width, height, this->buffer_width_,
             this->buffer_height_);
    // Let the buffer go rather than free memory that belongs to the caller. Dropping it also stops
    // a decoder that ignores this failure from publishing a picture it never painted.
    this->release_buffer_();
    return 0;
  }

  if (new_size == 0) {
    ESP_LOGE(TAG, "Refusing to allocate buffer for invalid image dimensions %dx%d", width, height);
    return 0;
  }

  // Release old buffer if dimensions changed
  if (this->buffer_) {
    this->release_buffer_();
  }

  ESP_LOGD(TAG, "Allocating buffer: %dx%d, %zu bytes", width, height, new_size);
  RAMAllocator<uint8_t> allocator;
  this->buffer_ = allocator.allocate(new_size);

  if (!this->buffer_) {
    ESP_LOGE(TAG, "Failed to allocate %zu bytes. Largest free block: %zu", new_size,
             allocator.get_max_free_block_size());
    return 0;
  }

  // Clear buffer
  memset(this->buffer_, 0, new_size);

  this->buffer_width_ = width;
  this->buffer_height_ = height;

  return new_size;
}

size_t RuntimeImage::get_buffer_size(int width, int height) const {
  // Dimensions come from a remote image header; reject absurd values so the size math cannot overflow
  if (width <= 0 || height <= 0 || width > MAX_IMAGE_DIMENSION || height > MAX_IMAGE_DIMENSION) {
    return 0;
  }
  if (this->get_type() == image::IMAGE_TYPE_RGB565 && this->transparency_ == image::TRANSPARENCY_ALPHA_CHANNEL) {
    // Add extra alpha channel for RGB565 with alpha
    return static_cast<size_t>(width) * height * 3;
  }
  return (static_cast<size_t>(this->get_bpp()) * width + 7u) / 8u * height;
}

int RuntimeImage::get_position_(int x, int y) const { return (x + y * this->buffer_width_) * this->get_bpp() / 8; }

std::unique_ptr<ImageDecoder> RuntimeImage::create_decoder_(ImageFormat format) {
  ESP_LOGV(TAG, "Creating decoder for format %s", LOG_STR_ARG(get_format_name(format)));
  switch (format) {
#ifdef USE_RUNTIME_IMAGE_BMP
    case BMP:
      return make_unique<BmpDecoder>(this);
#endif
#ifdef USE_RUNTIME_IMAGE_JPEG
    case JPEG:
#ifdef USE_RUNTIME_IMAGE_JPEG_TURBO
      return make_unique<JpegTurboDecoder>(this);
#else   // USE_RUNTIME_IMAGE_JPEG_TURBO
      return make_unique<JpegDecoder>(this);
#endif  // USE_RUNTIME_IMAGE_JPEG_TURBO
#endif  // USE_RUNTIME_IMAGE_JPEG
#ifdef USE_RUNTIME_IMAGE_PNG
    case PNG:
      return make_unique<PngDecoder>(this);
#endif
#ifdef USE_RUNTIME_IMAGE_QOI
    case QOI:
      return make_unique<QoiDecoder>(this);
#endif
    case AUTO:
      ESP_LOGE(TAG, "Image format could not be determined; set `format:` explicitly in the configuration");
      return nullptr;
    default:
      ESP_LOGE(TAG, "Unsupported image format: %s", LOG_STR_ARG(get_format_name(format)));
      return nullptr;
  }
}

}  // namespace esphome::runtime_image
