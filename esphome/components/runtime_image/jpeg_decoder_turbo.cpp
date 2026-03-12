#include "jpeg_decoder_turbo.h"
#ifdef USE_RUNTIME_IMAGE_JPEG_TURBO

#include "esphome/core/application.h"
#include "esphome/core/log.h"

static const char *const TAG = "image_decoder.jpeg_turbo";

namespace esphome::runtime_image {

static void jpeg_error_exit(j_common_ptr cinfo) {
  auto *err = reinterpret_cast<JpegErrorMgr *>(cinfo->err);
  (*(cinfo->err->format_message))(cinfo, err->message);
  longjmp(err->setjmp_buffer, 1);
}

void JpegDecoder::cleanup_() {
  if (this->cinfo_) {
    jpeg_destroy_decompress(this->cinfo_);
    delete this->cinfo_;
    this->cinfo_ = nullptr;
  }
  delete this->jerr_;
  this->jerr_ = nullptr;
  free(this->row_buffer_);
  this->row_buffer_ = nullptr;
}

int JpegDecoder::prepare(size_t expected_size) {
  ImageDecoder::prepare(expected_size);
  return 0;
}

int HOT JpegDecoder::decode(uint8_t *buffer, size_t size) {
  if (this->phase_ == FINISHED) {
    return size;
  }

  if (this->phase_ == WAITING) {
    if (this->expected_size_ > 0 && size < this->expected_size_) {
      ESP_LOGV(TAG, "Download not complete. Size: %zu/%zu", size, this->expected_size_);
      return 0;
    }

    this->cinfo_ = new jpeg_decompress_struct();
    this->jerr_ = new JpegErrorMgr();

    this->cinfo_->err = jpeg_std_error(&this->jerr_->pub);
    this->jerr_->pub.error_exit = jpeg_error_exit;

    if (setjmp(this->jerr_->setjmp_buffer)) {
      ESP_LOGE(TAG, "JPEG decode error during setup: %s", this->jerr_->message);
      this->cleanup_();
      return DECODE_ERROR_UNSUPPORTED_FORMAT;
    }

    jpeg_create_decompress(this->cinfo_);
    jpeg_mem_src(this->cinfo_, buffer, size);

    if (jpeg_read_header(this->cinfo_, TRUE) != JPEG_HEADER_OK) {
      ESP_LOGE(TAG, "Could not read JPEG header");
      this->cleanup_();
      return DECODE_ERROR_INVALID_TYPE;
    }

    int src_w = this->cinfo_->image_width;
    int src_h = this->cinfo_->image_height;
    ESP_LOGD(TAG, "JPEG header: %dx%d, components=%d", src_w, src_h, this->cinfo_->num_components);

    this->cinfo_->out_color_space = JCS_RGB;
    this->cinfo_->dct_method = JDCT_IFAST;

    // Use IDCT downscaling when fixed target dimensions are configured.
    // libjpeg-turbo can decode at 1/8, 1/4, 1/2 or full size during the
    // IDCT step, which is much cheaper than decoding at full resolution
    // and then scaling in software.
    int target_w = this->image_->get_fixed_width();
    int target_h = this->image_->get_fixed_height();
    if (target_w > 0 && target_h > 0) {
      constexpr unsigned int denoms[] = {8, 4, 2, 1};
      for (unsigned int denom : denoms) {
        this->cinfo_->scale_num = 1;
        this->cinfo_->scale_denom = denom;
        jpeg_calc_output_dimensions(this->cinfo_);
        int idct_w = static_cast<int>(this->cinfo_->output_width);
        int idct_h = static_cast<int>(this->cinfo_->output_height);
        if (idct_w >= target_w && idct_h >= target_h)
          break;
      }
    } else {
      jpeg_calc_output_dimensions(this->cinfo_);
    }

    this->out_w_ = this->cinfo_->output_width;
    int out_h = this->cinfo_->output_height;
    if (this->out_w_ != src_w || out_h != src_h) {
      ESP_LOGD(TAG, "Using IDCT downscale: %dx%d -> %dx%d", src_w, src_h, this->out_w_, out_h);
    }

    if (!this->set_size(this->out_w_, out_h)) {
      this->cleanup_();
      return DECODE_ERROR_OUT_OF_MEMORY;
    }

    jpeg_start_decompress(this->cinfo_);

    size_t row_stride = static_cast<size_t>(this->out_w_) * 3;
    this->row_buffer_ = static_cast<uint8_t *>(malloc(row_stride));
    if (this->row_buffer_ == nullptr) {
      this->cleanup_();
      return DECODE_ERROR_OUT_OF_MEMORY;
    }

    this->current_scanline_ = 0;
    this->phase_ = DECOMPRESSING;
  }

  // DECOMPRESSING phase: process a chunk of scanlines per loop iteration
  if (setjmp(this->jerr_->setjmp_buffer)) {
    ESP_LOGE(TAG, "JPEG decode error: %s", this->jerr_->message);
    this->cleanup_();
    return DECODE_ERROR_UNSUPPORTED_FORMAT;
  }

  int lines_this_chunk = 0;
  while (this->cinfo_->output_scanline < this->cinfo_->output_height && lines_this_chunk < SCANLINES_PER_CHUNK) {
    uint8_t *row_ptr = this->row_buffer_;
    jpeg_read_scanlines(this->cinfo_, &row_ptr, 1);

    if ((this->current_scanline_ & 63) == 0) {
      App.feed_wdt();
    }

    for (int x = 0; x < this->out_w_; x++) {
      Color color(this->row_buffer_[x * 3 + 0], this->row_buffer_[x * 3 + 1], this->row_buffer_[x * 3 + 2]);
      this->draw(x, this->current_scanline_, 1, 1, color);
    }

    this->current_scanline_++;
    lines_this_chunk++;
  }

  if (this->cinfo_->output_scanline >= this->cinfo_->output_height) {
    jpeg_finish_decompress(this->cinfo_);
    this->cleanup_();
    this->phase_ = FINISHED;
    this->decoded_bytes_ = size;
    return size;
  }

  return 0;
}

}  // namespace esphome::runtime_image

#endif  // USE_RUNTIME_IMAGE_JPEG_TURBO
