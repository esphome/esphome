#include "jpeg_turbo_decoder.h"
#ifdef USE_RUNTIME_IMAGE_JPEG_TURBO

#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <csetjmp>
#include <cstdio>

#include <jpeglib.h>
#include <jerror.h>

#ifdef USE_ESP_IDF
#include "esp_task_wdt.h"
#endif

namespace esphome::runtime_image {

static const char *const TAG = "image_decoder.jpeg_turbo";

struct DecoderErrorManager {
  struct jpeg_error_mgr pub;  // Must be the first member so it can be cast back from cinfo->err
  jmp_buf jump_buffer;
  bool truncated;
};

static void handle_message(j_common_ptr cinfo, int msg_level) {
  if (msg_level >= 0)  // Trace messages are not interesting here
    return;
  auto *err = reinterpret_cast<DecoderErrorManager *>(cinfo->err);
  if (cinfo->err->msg_code == JWRN_JPEG_EOF)
    err->truncated = true;
  if (cinfo->err->num_warnings == 0) {  // Corrupt files can warn a lot; log only the first
    char message[JMSG_LENGTH_MAX];
    (*cinfo->err->format_message)(cinfo, message);
    ESP_LOGW(TAG, "%s", message);
  }
  cinfo->err->num_warnings++;
}

// libjpeg reports fatal errors through this callback; jump back into decode()
// instead of the library's default behavior of terminating the program.
static void handle_fatal_error(j_common_ptr cinfo) {
  auto *err = reinterpret_cast<DecoderErrorManager *>(cinfo->err);
  char message[JMSG_LENGTH_MAX];
  (*cinfo->err->format_message)(cinfo, message);
  ESP_LOGE(TAG, "Decoding failed: %s", message);
  longjmp(err->jump_buffer, 1);
}

struct DecoderProgressManager {
  struct jpeg_progress_mgr pub;  // Must be the first member so it can be cast back from cinfo->progress
  bool feed_wdt;
};

// Installed as libjpeg's progress_monitor: big images take too long to decode, so feed
// the watchdog to avoid crashing if the executing task has a watchdog enabled.
// libjpeg calls this once per scanline, so the subscription check is done once up front
// rather than here: esp_task_wdt_status() takes the watchdog spinlock on every call.
static void feed_watchdog(j_common_ptr cinfo) {
  auto *progress = reinterpret_cast<DecoderProgressManager *>(cinfo->progress);
  if (progress->feed_wdt) {
    App.feed_wdt();
  }
}

int HOT JpegTurboDecoder::decode(uint8_t *buffer, size_t size) {
  // JPEG decoder requires complete data
  // If we know the expected size, wait for it
  if (this->expected_size_ > 0 && size < this->expected_size_) {
    ESP_LOGV(TAG, "Download not complete. Size: %zu/%zu", size, this->expected_size_);
    return 0;
  }

  struct jpeg_decompress_struct cinfo;
  DecoderErrorManager error_manager;
  cinfo.err = jpeg_std_error(&error_manager.pub);
  error_manager.pub.error_exit = handle_fatal_error;
  error_manager.pub.emit_message = handle_message;
  error_manager.truncated = false;
  // setjmp/longjmp is the documented libjpeg error handling mechanism; exceptions
  // are not available (ESPHome builds with -fno-exceptions).
  if (setjmp(error_manager.jump_buffer)) {  // NOLINT(cert-err52-cpp,modernize-avoid-setjmp-longjmp)
    // Control returns here when libjpeg encounters a fatal error
    int msg_code = error_manager.pub.msg_code;
    jpeg_destroy_decompress(&cinfo);
    switch (msg_code) {
      case JERR_OUT_OF_MEMORY:
        return DECODE_ERROR_OUT_OF_MEMORY;
      case JERR_NO_SOI:
        return DECODE_ERROR_INVALID_TYPE;
      case JERR_NOT_COMPILED:
        return DECODE_ERROR_UNSUPPORTED_FORMAT;
      default:
        return DECODE_ERROR_INTERNAL_DECODER_ERROR;
    }
  }

  jpeg_create_decompress(&cinfo);
  jpeg_mem_src(&cinfo, buffer, size);
  if (jpeg_read_header(&cinfo, TRUE) != JPEG_HEADER_OK) {
    jpeg_destroy_decompress(&cinfo);
    ESP_LOGE(TAG, "Could not read JPEG header");
    return DECODE_ERROR_INVALID_TYPE;
  }
  cinfo.out_color_space = JCS_RGB;
  // libjpeg calls progress_monitor from inside its long-running loops, including the
  // multi-scan absorb loop in jpeg_start_decompress() that a progressive image spends
  // nearly all of its time in. Feeding there covers the whole decode; nothing else can,
  // because that loop never returns to us until the image is fully absorbed.
  DecoderProgressManager progress{};
  progress.pub.progress_monitor = feed_watchdog;
  progress.feed_wdt = true;
#ifdef USE_ESP_IDF
  progress.feed_wdt = esp_task_wdt_status(nullptr) == ESP_OK;
#endif
  cinfo.progress = &progress.pub;
  if (!jpeg_start_decompress(&cinfo)) {
    jpeg_destroy_decompress(&cinfo);
    ESP_LOGE(TAG, "Could not start JPEG decompression");
    return DECODE_ERROR_INTERNAL_DECODER_ERROR;
  }
  ESP_LOGD(TAG, "Image size: %u x %u, progressive: %s", cinfo.output_width, cinfo.output_height,
           YESNO(jpeg_has_multiple_scans(&cinfo)));

  if (!this->set_size(cinfo.output_width, cinfo.output_height)) {
    jpeg_destroy_decompress(&cinfo);
    return DECODE_ERROR_OUT_OF_MEMORY;
  }

  // Allocate a single scanline buffer from the library's image-lifetime pool,
  // so it is released together with the decompress object.
  JSAMPARRAY row =
      (*cinfo.mem->alloc_sarray)((j_common_ptr) &cinfo, JPOOL_IMAGE, cinfo.output_width * cinfo.output_components, 1);

  while (cinfo.output_scanline < cinfo.output_height) {
    size_t y = cinfo.output_scanline;
    if (jpeg_read_scanlines(&cinfo, row, 1) != 1) {
      jpeg_destroy_decompress(&cinfo);
      ESP_LOGE(TAG, "Could not read scanline %zu of %u", y, cinfo.output_height);
      return DECODE_ERROR_INTERNAL_DECODER_ERROR;
    }
    const uint8_t *pixel = row[0];
    for (size_t x = 0; x < cinfo.output_width; x++, pixel += 3) {
      Color color(pixel[0], pixel[1], pixel[2], 0xFF);
      this->draw(x, y, 1, 1, color);
    }
  }

  jpeg_finish_decompress(&cinfo);
  const bool truncated = error_manager.truncated;
  jpeg_destroy_decompress(&cinfo);
  if (truncated) {
    ESP_LOGE(TAG, "JPEG data is incomplete");
    return DECODE_ERROR_INVALID_TYPE;
  }
  this->decoded_bytes_ = size;
  return size;
}

}  // namespace esphome::runtime_image

#endif  // USE_RUNTIME_IMAGE_JPEG_TURBO
