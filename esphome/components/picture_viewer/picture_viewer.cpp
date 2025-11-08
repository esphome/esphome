#include "picture_viewer.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

#include <algorithm>
#include <cstring>

#ifdef ESP32
#include "esp_heap_caps.h"
#endif

namespace esphome {
namespace picture_viewer {

static const char *const TAG = "picture_viewer";

// =====================================================
// Component Lifecycle
// =====================================================

PictureViewer::~PictureViewer() {
  // Free current image data
  if (this->current_image_data_ != nullptr) {
    free(this->current_image_data_);
    this->current_image_data_ = nullptr;
  }

  // Free next image data
  if (this->next_image_data_ != nullptr) {
    free(this->next_image_data_);
    this->next_image_data_ = nullptr;
  }

  // Note: Don't delete transcoder's decoder - transcoder owns it

#ifdef USE_JPEGDEC
  if (this->jpeg_decoder_ != nullptr) {
    delete this->jpeg_decoder_;
    this->jpeg_decoder_ = nullptr;
  }
#endif
}

void PictureViewer::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Picture Viewer...");

#ifdef USE_TRANSCODER
  // Verify transcoder is available
  if (this->transcoder_ == nullptr) {
    ESP_LOGE(TAG, "Transcoder component not set");
    this->mark_failed();
    return;
  }

  // Verify JPEG decoder is available in transcoder
  if (!this->transcoder_->is_jpeg_decoder_available()) {
    ESP_LOGE(TAG, "JPEG decoder not available in transcoder");
    this->mark_failed();
    return;
  }

  ESP_LOGI(TAG, "Using transcoder for JPEG decoding");
#else
#ifdef USE_JPEGDEC
  // Initialize JPEGDec library (fallback when transcoder not available)
  this->jpeg_decoder_ = new JPEGDEC();
  ESP_LOGI(TAG, "JPEGDec library initialized");
#endif
#endif

  // Register callback with file_manager if available
#ifdef USE_STORAGE_HOST
  if (this->file_manager_ != nullptr) {
    this->file_manager_->add_on_directory_changed_callback(
        [this](const storage_host::DirectoryChangeInfo &info) { this->on_directory_changed_(info); });
    ESP_LOGI(TAG, "Registered directory change callback with file_manager");
  }
#endif

  // Scan directory for images
  if (!this->watch_directory_.empty()) {
    this->refresh_images();
  }

  ESP_LOGCONFIG(TAG, "Picture Viewer setup complete");
}

void PictureViewer::loop() {
  // Handle slideshow
  if (this->slideshow_mode_ == SlideshowMode::PLAYING) {
    uint32_t now = millis();
    if (now - this->last_slideshow_time_ >= this->slideshow_interval_ms_) {
      this->next_image();
      this->last_slideshow_time_ = now;
    }
  }
}

void PictureViewer::dump_config() {
  ESP_LOGCONFIG(TAG, "Picture Viewer:");
  ESP_LOGCONFIG(TAG, "  Watch Directory: %s", this->watch_directory_.c_str());
  ESP_LOGCONFIG(TAG, "  Image Count: %zu", this->images_.size());
  ESP_LOGCONFIG(TAG, "  Slideshow Interval: %u ms", this->slideshow_interval_ms_);
  ESP_LOGCONFIG(TAG, "  Thumbnails: %s", this->enable_thumbnails_ ? "enabled" : "disabled");
  if (this->enable_thumbnails_) {
    ESP_LOGCONFIG(TAG, "  Thumbnail Size: %dx%d", this->thumbnail_width_, this->thumbnail_height_);
  }
#ifdef USE_ESP_JPEG_DECODER
  ESP_LOGCONFIG(TAG, "  Decoder: esp_jpeg (ESP32-S2/S3)");
#elif defined(USE_HARDWARE_JPEG_DECODER)
  ESP_LOGCONFIG(TAG, "  Decoder: Hardware JPEG (ESP32-P4)");
#elif defined(USE_JPEGDEC)
  ESP_LOGCONFIG(TAG, "  Decoder: JPEGDec (software)");
#endif
}

// =====================================================
// Picture Control API
// =====================================================

bool PictureViewer::show_image(size_t index) {
  if (index >= this->images_.size()) {
    ESP_LOGW(TAG, "Image index out of range: %zu (total: %zu)", index, this->images_.size());
    return false;
  }

  const auto &entry = this->images_[index];
  ESP_LOGD(TAG, "Showing image %zu: %s", index, entry.filename.c_str());

  // Load and decode image
  std::vector<uint8_t> rgb565_data;
  int width, height;

  // Get target dimensions
  this->update_canvas_dimensions_();
  int target_width = this->fullscreen_ ? this->canvas_width_ : 0;
  int target_height = this->fullscreen_ ? this->canvas_height_ : 0;

  if (!this->load_jpeg_(entry.path, rgb565_data, width, height, target_width, target_height)) {
    ESP_LOGE(TAG, "Failed to load image: %s", entry.path.c_str());
    return false;
  }

  // Free old current image
  if (this->current_image_data_ != nullptr) {
    free(this->current_image_data_);
    this->current_image_data_ = nullptr;
  }

  // Allocate new buffer in PSRAM/heap
  this->current_image_size_ = rgb565_data.size();
  this->current_image_data_ = this->allocate_image_buffer_(this->current_image_size_);
  if (this->current_image_data_ == nullptr) {
    ESP_LOGE(TAG, "Failed to allocate image buffer: %zu bytes", this->current_image_size_);
    return false;
  }

  // Copy decoded data to buffer
  std::memcpy(this->current_image_data_, rgb565_data.data(), this->current_image_size_);
  this->current_image_width_ = width;
  this->current_image_height_ = height;
  this->current_index_ = index;

  // Update canvas
  this->update_canvas_();

  ESP_LOGI(TAG, "Displayed image: %s (%dx%d)", entry.filename.c_str(), width, height);
  return true;
}

bool PictureViewer::show_image(const std::string &path) {
  // Find image by path
  for (size_t i = 0; i < this->images_.size(); i++) {
    if (this->images_[i].path == path || this->images_[i].filename == path) {
      return this->show_image(i);
    }
  }

  ESP_LOGW(TAG, "Image not found: %s", path.c_str());
  return false;
}

bool PictureViewer::next_image() {
  if (this->images_.empty()) {
    ESP_LOGW(TAG, "No images available");
    return false;
  }

  int next_index = (this->current_index_ + 1) % static_cast<int>(this->images_.size());
  return this->show_image(next_index);
}

bool PictureViewer::previous_image() {
  if (this->images_.empty()) {
    ESP_LOGW(TAG, "No images available");
    return false;
  }

  int prev_index = this->current_index_ - 1;
  if (prev_index < 0) {
    prev_index = static_cast<int>(this->images_.size()) - 1;
  }
  return this->show_image(prev_index);
}

void PictureViewer::start_slideshow() {
  if (this->images_.empty()) {
    ESP_LOGW(TAG, "No images available for slideshow");
    return;
  }

  ESP_LOGI(TAG, "Starting slideshow");
  this->slideshow_mode_ = SlideshowMode::PLAYING;
  this->last_slideshow_time_ = millis();

  // Show first image if none is currently displayed
  if (this->current_index_ < 0) {
    this->show_image(0);
  }
}

void PictureViewer::stop_slideshow() {
  ESP_LOGI(TAG, "Stopping slideshow");
  this->slideshow_mode_ = SlideshowMode::STOPPED;
}

void PictureViewer::pause_slideshow() {
  ESP_LOGI(TAG, "Pausing slideshow");
  this->slideshow_mode_ = SlideshowMode::PAUSED;
}

void PictureViewer::toggle_slideshow() {
  if (this->slideshow_mode_ == SlideshowMode::PLAYING) {
    this->pause_slideshow();
  } else {
    this->start_slideshow();
  }
}

void PictureViewer::refresh_images() {
  ESP_LOGI(TAG, "Refreshing image list from: %s", this->watch_directory_.c_str());

  // Clear existing images
  this->images_.clear();
  this->current_index_ = -1;

  // Get files from file_manager and scan
#ifdef USE_STORAGE_HOST
  if (this->file_manager_ != nullptr) {
    const auto &directory_state = this->file_manager_->get_directory_state();
    // Convert map values to vector
    std::vector<storage_host::FileInfo> files;
    files.reserve(directory_state.size());
    for (const auto &[path, file] : directory_state) {
      files.push_back(file);
    }
    this->scan_directory_(files);
  } else {
    ESP_LOGW(TAG, "No file_manager available");
  }
#else
  ESP_LOGW(TAG, "Storage host support not available");
#endif

  ESP_LOGI(TAG, "Found %zu images", this->images_.size());
}

void PictureViewer::set_fullscreen(bool fullscreen) {
  if (this->fullscreen_ == fullscreen) {
    return;
  }

  ESP_LOGI(TAG, "Setting fullscreen: %s", fullscreen ? "true" : "false");
  this->fullscreen_ = fullscreen;

  // Reload current image with new dimensions
  if (this->current_index_ >= 0) {
    this->show_image(this->current_index_);
  }
}

// =====================================================
// Internal Methods
// =====================================================

void PictureViewer::scan_directory_(const std::vector<storage_host::FileInfo> &files) {
#ifdef USE_STORAGE_HOST
  ESP_LOGD(TAG, "Scanning %zu files for JPEGs in watch_directory: '%s'", files.size(), this->watch_directory_.c_str());

  size_t jpeg_count = 0;
  size_t matched_count = 0;

  for (const auto &file : files) {
    ESP_LOGV(TAG, "Examining file: path='%s', filename='%s'", file.path.c_str(), file.filename.c_str());

    // Filter by directory first (if watch_directory is set)
    if (!this->watch_directory_.empty()) {
      // Check if file path starts with watch_directory
      if (file.path.find(this->watch_directory_) != 0) {
        ESP_LOGV(TAG, "  Skipping - not in watch_directory");
        continue;
      }
    }

    // Filter JPEG files
    std::string lower_filename = file.filename;
    std::transform(lower_filename.begin(), lower_filename.end(), lower_filename.begin(), ::tolower);

    if (lower_filename.ends_with(".jpg") || lower_filename.ends_with(".jpeg")) {
      jpeg_count++;
      ESP_LOGD(TAG, "  Found JPEG: %s (size: %llu bytes)", file.filename.c_str(),
               (unsigned long long)file.size);
      ImageEntry entry;
      entry.path = file.path;
      entry.filename = file.filename;
      entry.size = file.size;
      this->images_.push_back(entry);
      matched_count++;
    }
  }

  ESP_LOGD(TAG, "Scan complete: %zu JPEGs found, %zu matched filters", jpeg_count, matched_count);

  // Sort by filename
  std::sort(this->images_.begin(), this->images_.end(),
            [](const ImageEntry &a, const ImageEntry &b) { return a.filename < b.filename; });
#endif
}

void PictureViewer::on_directory_changed_(const storage_host::DirectoryChangeInfo &info) {
#ifdef USE_STORAGE_HOST
  ESP_LOGI(TAG, "Directory changed callback: %s (%zu files)", info.path.c_str(), info.files.size());

  // Clear existing images
  this->images_.clear();
  this->current_index_ = -1;

  // Scan the new file list
  this->scan_directory_(info.files);

  ESP_LOGI(TAG, "After directory change: %zu images found", this->images_.size());
#endif
}

bool PictureViewer::load_jpeg_(const std::string &path, std::vector<uint8_t> &rgb565_data, int &width, int &height,
                                int target_width, int target_height) {
  // Read JPEG file
  std::vector<uint8_t> jpeg_data;
  if (!this->read_file_(path, jpeg_data)) {
    return false;
  }

  ESP_LOGD(TAG, "Loaded JPEG file: %s (%zu bytes)", path.c_str(), jpeg_data.size());

  // Decode JPEG based on platform
#ifdef USE_ESP_JPEG_DECODER
  return this->decode_jpeg_esp_(jpeg_data, rgb565_data, width, height, target_width, target_height);
#elif defined(USE_HARDWARE_JPEG_DECODER)
  return this->decode_jpeg_hardware_(jpeg_data, rgb565_data, width, height, target_width, target_height);
#elif defined(USE_JPEGDEC)
  return this->decode_jpeg_jpegdec_(jpeg_data, rgb565_data, width, height, target_width, target_height);
#else
#error "No JPEG decoder available"
#endif
}

#ifdef USE_ESP_JPEG_DECODER
bool PictureViewer::decode_jpeg_esp_(const std::vector<uint8_t> &jpeg_data, std::vector<uint8_t> &rgb565_data,
                                      int &width, int &height, int target_width, int target_height) {
  esp_jpeg_image_cfg_t jpeg_cfg = {
      .indata = jpeg_data.data(),
      .indata_size = static_cast<int>(jpeg_data.size()),
      .outbuf = nullptr,
      .outbuf_size = 0,
      .out_format = JPEG_IMAGE_FORMAT_RGB565,
      .out_scale = JPEG_IMAGE_SCALE_0,
      .flags = {
          .swap_color_bytes = 0,
      }};

  esp_jpeg_image_output_t outimg = {};
  esp_err_t ret = esp_jpeg_decode(&jpeg_cfg, &outimg);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "JPEG decode failed: %d", ret);
    return false;
  }

  width = outimg.width;
  height = outimg.height;

  // Copy decoded data
  size_t size = outimg.width * outimg.height * 2;  // RGB565 = 2 bytes per pixel
  rgb565_data.resize(size);
  std::memcpy(rgb565_data.data(), outimg.outbuf, size);

  // Free decoder output buffer
  free(outimg.outbuf);

  ESP_LOGD(TAG, "Decoded JPEG using esp_jpeg: %dx%d", width, height);
  return true;
}
#endif

#ifdef USE_HARDWARE_JPEG_DECODER
bool PictureViewer::decode_jpeg_hardware_(const std::vector<uint8_t> &jpeg_data, std::vector<uint8_t> &rgb565_data,
                                           int &width, int &height, int target_width, int target_height) {
  if (this->transcoder_ == nullptr || !this->transcoder_->is_jpeg_decoder_available()) {
    ESP_LOGE(TAG, "Hardware JPEG decoder not available in transcoder");
    return false;
  }

  // Get decoder handle from transcoder
  jpeg_decoder_handle_t hw_decoder = this->transcoder_->get_jpeg_decoder();

  // Get image info
  jpeg_decode_picture_info_t pic_info = {};
  esp_err_t ret = jpeg_decoder_get_info(jpeg_data.data(), static_cast<uint32_t>(jpeg_data.size()), &pic_info);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to get JPEG info: %s", esp_err_to_name(ret));
    return false;
  }

  ESP_LOGD(TAG, "JPEG dimensions: %dx%d", pic_info.width, pic_info.height);

  width = pic_info.width;
  height = pic_info.height;

  // Calculate buffer sizes
  uint32_t input_size = jpeg_data.size();
  uint32_t output_size = width * height * 2;  // RGB565 = 2 bytes per pixel

  // Allocate 16-byte aligned buffers (hardware requirement)
  uint8_t *aligned_input = (uint8_t *) heap_caps_aligned_alloc(16, input_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!aligned_input) {
    ESP_LOGE(TAG, "Failed to allocate aligned input buffer (%u bytes)", input_size);
    return false;
  }

  uint8_t *aligned_output = (uint8_t *) heap_caps_aligned_alloc(16, output_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!aligned_output) {
    ESP_LOGE(TAG, "Failed to allocate aligned output buffer (%u bytes)", output_size);
    free(aligned_input);
    return false;
  }

  // Copy input data to aligned buffer
  memcpy(aligned_input, jpeg_data.data(), input_size);

  // Configure decoder - use zero-init pattern to avoid struct corruption
  jpeg_decode_cfg_t decode_cfg = {};
  decode_cfg.output_format = JPEG_DECODE_OUT_FORMAT_RGB565;
  decode_cfg.rgb_order = JPEG_DEC_RGB_ELEMENT_ORDER_RGB;

  // Debug: Log all parameters before decode
  ESP_LOGI(TAG, "[DECODE DEBUG] Decoder handle: %p", hw_decoder);
  ESP_LOGI(TAG, "[DECODE DEBUG] Input buffer: %p (size: %u, aligned: %s)", aligned_input, input_size,
           ((uintptr_t)aligned_input % 16 == 0) ? "YES" : "NO");
  ESP_LOGI(TAG, "[DECODE DEBUG] Output buffer: %p (size: %u, aligned: %s)", aligned_output, output_size,
           ((uintptr_t)aligned_output % 16 == 0) ? "YES" : "NO");
  ESP_LOGI(TAG, "[DECODE DEBUG] Config: output_format=%d, rgb_order=%d", decode_cfg.output_format,
           decode_cfg.rgb_order);
  ESP_LOGI(TAG, "[DECODE DEBUG] Image dimensions: %dx%d", width, height);

  // Decode
  ret = jpeg_decoder_process(hw_decoder, &decode_cfg, aligned_input, input_size, aligned_output, output_size,
                             &output_size);

  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Hardware JPEG decode failed: %s (error code: %d)", esp_err_to_name(ret), ret);
    ESP_LOGE(TAG, "[DECODE DEBUG] Failed with decoder=%p, in=%p, in_size=%u, out=%p, out_size=%u", hw_decoder,
             aligned_input, input_size, aligned_output, output_size);
    free(aligned_input);
    free(aligned_output);
    return false;
  }

  ESP_LOGD(TAG, "Hardware decode completed, output size: %u bytes", output_size);

  // Byte-swap RGB565 for correct endianness
  uint16_t *pixels = (uint16_t *) aligned_output;
  size_t pixel_count = output_size / 2;
  for (size_t i = 0; i < pixel_count; i++) {
    pixels[i] = (pixels[i] << 8) | (pixels[i] >> 8);
  }

  // Copy to output vector
  rgb565_data.resize(output_size);
  memcpy(rgb565_data.data(), aligned_output, output_size);

  // Free aligned buffers
  free(aligned_input);
  free(aligned_output);

  ESP_LOGD(TAG, "Decoded JPEG using hardware decoder: %dx%d", width, height);
  return true;
}
#endif

#ifdef USE_JPEGDEC
bool PictureViewer::decode_jpeg_jpegdec_(const std::vector<uint8_t> &jpeg_data, std::vector<uint8_t> &rgb565_data,
                                          int &width, int &height, int target_width, int target_height) {
  if (this->jpeg_decoder_ == nullptr) {
    ESP_LOGE(TAG, "JPEGDec decoder not initialized");
    return false;
  }

  // Open JPEG from memory
  if (this->jpeg_decoder_->openRAM(const_cast<uint8_t *>(jpeg_data.data()), jpeg_data.size(),
                                   PictureViewer::jpeg_decode_callback_) != 1) {
    ESP_LOGE(TAG, "Failed to open JPEG");
    return false;
  }

  width = this->jpeg_decoder_->getWidth();
  height = this->jpeg_decoder_->getHeight();

  // Allocate output buffer
  rgb565_data.resize(width * height * 2);  // RGB565 = 2 bytes per pixel
  this->decode_target_ = &rgb565_data;
  this->decode_width_ = width;

  // Decode
  if (this->jpeg_decoder_->decode(0, 0, 0) != 1) {
    ESP_LOGE(TAG, "Failed to decode JPEG");
    this->jpeg_decoder_->close();
    return false;
  }

  this->jpeg_decoder_->close();
  this->decode_target_ = nullptr;

  ESP_LOGD(TAG, "Decoded JPEG using JPEGDec: %dx%d", width, height);
  return true;
}

int PictureViewer::jpeg_decode_callback_(JPEGDRAW *draw) {
  // This is called during decode to provide the RGB565 data
  // We need to copy it to our buffer
  if (draw == nullptr || draw->pUser == nullptr) {
    return 0;
  }

  // Note: This is a static callback, so we can't access instance members directly
  // The decode_target_ and decode_width_ should be set before decode() is called
  return 1;
}
#endif

void PictureViewer::resize_image_(const std::vector<uint8_t> &src_data, int src_width, int src_height,
                                   std::vector<uint8_t> &dst_data, int dst_width, int dst_height) {
  // Simple nearest-neighbor scaling for RGB565
  dst_data.resize(dst_width * dst_height * 2);

  for (int y = 0; y < dst_height; y++) {
    int src_y = (y * src_height) / dst_height;
    for (int x = 0; x < dst_width; x++) {
      int src_x = (x * src_width) / dst_width;
      int src_idx = (src_y * src_width + src_x) * 2;
      int dst_idx = (y * dst_width + x) * 2;
      dst_data[dst_idx] = src_data[src_idx];
      dst_data[dst_idx + 1] = src_data[src_idx + 1];
    }
  }
}

void PictureViewer::update_canvas_() {
#ifdef USE_LVGL
  if (this->canvas_ == nullptr) {
    ESP_LOGW(TAG, "Canvas not set");
    return;
  }

  if (this->current_image_data_ == nullptr) {
    ESP_LOGW(TAG, "No image data to display");
    return;
  }

  // Get canvas dimensions
  lv_coord_t canvas_width = lv_obj_get_width(this->canvas_);
  lv_coord_t canvas_height = lv_obj_get_height(this->canvas_);

  ESP_LOGD(TAG, "Canvas: %dx%d, Image: %dx%d, Fit mode: %d", canvas_width, canvas_height, this->current_image_width_,
           this->current_image_height_, static_cast<int>(this->fit_mode_));

  // Calculate drawing dimensions and position based on fit mode
  int draw_x = 0, draw_y = 0;
  int draw_width = this->current_image_width_;
  int draw_height = this->current_image_height_;

  switch (this->fit_mode_) {
    case ImageFitMode::SCALE_TO_FIT: {
      // Scale to fit, maintain aspect ratio
      float scale_x = static_cast<float>(canvas_width) / this->current_image_width_;
      float scale_y = static_cast<float>(canvas_height) / this->current_image_height_;
      float scale = std::min(scale_x, scale_y);

      draw_width = static_cast<int>(this->current_image_width_ * scale);
      draw_height = static_cast<int>(this->current_image_height_ * scale);

      // Center in canvas
      draw_x = (canvas_width - draw_width) / 2;
      draw_y = (canvas_height - draw_height) / 2;

      // Clear canvas with black background
      lv_canvas_fill_bg(this->canvas_, lv_color_black(), LV_OPA_COVER);
      break;
    }

    case ImageFitMode::SCALE_TO_FILL: {
      // Scale to fill, maintain aspect ratio, may crop
      float scale_x = static_cast<float>(canvas_width) / this->current_image_width_;
      float scale_y = static_cast<float>(canvas_height) / this->current_image_height_;
      float scale = std::max(scale_x, scale_y);

      draw_width = static_cast<int>(this->current_image_width_ * scale);
      draw_height = static_cast<int>(this->current_image_height_ * scale);

      // Center in canvas (may be cropped)
      draw_x = (canvas_width - draw_width) / 2;
      draw_y = (canvas_height - draw_height) / 2;
      break;
    }

    case ImageFitMode::STRETCH: {
      // Stretch to fill canvas, ignore aspect ratio
      draw_width = canvas_width;
      draw_height = canvas_height;
      draw_x = 0;
      draw_y = 0;
      break;
    }

    case ImageFitMode::CENTER: {
      // Center without scaling
      draw_x = (canvas_width - this->current_image_width_) / 2;
      draw_y = (canvas_height - this->current_image_height_) / 2;

      // Clear canvas with black background
      lv_canvas_fill_bg(this->canvas_, lv_color_black(), LV_OPA_COVER);
      break;
    }
  }

  // Need to scale/copy the image data to canvas if dimensions don't match
  if (draw_width == this->current_image_width_ && draw_height == this->current_image_height_) {
    // Direct copy - no scaling needed
    lv_img_dsc_t img_dsc;
    img_dsc.header.always_zero = 0;
    img_dsc.header.w = this->current_image_width_;
    img_dsc.header.h = this->current_image_height_;
    img_dsc.header.cf = LV_IMG_CF_TRUE_COLOR;  // RGB565
    img_dsc.data_size = this->current_image_size_;
    img_dsc.data = this->current_image_data_;

    lv_canvas_draw_img(this->canvas_, draw_x, draw_y, &img_dsc, nullptr);
  } else {
    // Need to scale the image
    std::vector<uint8_t> scaled_data;
    this->resize_image_(std::vector<uint8_t>(this->current_image_data_,
                                              this->current_image_data_ + this->current_image_size_),
                        this->current_image_width_, this->current_image_height_, scaled_data, draw_width, draw_height);

    lv_img_dsc_t img_dsc;
    img_dsc.header.always_zero = 0;
    img_dsc.header.w = draw_width;
    img_dsc.header.h = draw_height;
    img_dsc.header.cf = LV_IMG_CF_TRUE_COLOR;  // RGB565
    img_dsc.data_size = scaled_data.size();
    img_dsc.data = scaled_data.data();

    lv_canvas_draw_img(this->canvas_, draw_x, draw_y, &img_dsc, nullptr);
  }

  // Invalidate canvas to trigger redraw
  lv_obj_invalidate(this->canvas_);

  ESP_LOGD(TAG, "Canvas updated successfully");
#endif
}

bool PictureViewer::generate_thumbnail_(ImageEntry &entry) {
  if (!this->enable_thumbnails_) {
    return false;
  }

  // Load and decode thumbnail
  std::vector<uint8_t> rgb565_data;
  int width, height;
  if (!this->load_jpeg_(entry.path, rgb565_data, width, height, this->thumbnail_width_, this->thumbnail_height_)) {
    return false;
  }

  // Resize to thumbnail size if needed
  if (width != this->thumbnail_width_ || height != this->thumbnail_height_) {
    this->resize_image_(rgb565_data, width, height, entry.thumbnail_data, this->thumbnail_width_,
                        this->thumbnail_height_);
  } else {
    entry.thumbnail_data = std::move(rgb565_data);
  }

  entry.width = width;
  entry.height = height;
  entry.thumbnail_loaded = true;

  return true;
}

void PictureViewer::update_canvas_dimensions_() {
#ifdef USE_LVGL
  if (this->canvas_ != nullptr) {
    this->canvas_width_ = lv_obj_get_width(this->canvas_);
    this->canvas_height_ = lv_obj_get_height(this->canvas_);
    ESP_LOGD(TAG, "Canvas dimensions: %dx%d", this->canvas_width_, this->canvas_height_);
  } else {
    // Fallback to defaults if canvas not set
    if (this->canvas_width_ == 0 || this->canvas_height_ == 0) {
      this->canvas_width_ = 800;
      this->canvas_height_ = 480;
      ESP_LOGW(TAG, "Canvas not set, using default dimensions: %dx%d", this->canvas_width_, this->canvas_height_);
    }
  }
#endif
}

bool PictureViewer::read_file_(const std::string &path, std::vector<uint8_t> &data) {
#ifdef USE_STORAGE_HOST
  if (this->file_manager_ == nullptr) {
    ESP_LOGW(TAG, "File manager not set");
    return false;
  }

  FILE *f = fopen(path.c_str(), "rb");
  if (f == nullptr) {
    ESP_LOGE(TAG, "Failed to open file: %s", path.c_str());
    return false;
  }

  // Get file size
  fseek(f, 0, SEEK_END);
  long size = ftell(f);
  fseek(f, 0, SEEK_SET);

  // Read file
  data.resize(size);
  size_t read = fread(data.data(), 1, size, f);
  fclose(f);

  if (read != static_cast<size_t>(size)) {
    ESP_LOGE(TAG, "Failed to read file: %s", path.c_str());
    return false;
  }

  return true;
#else
  ESP_LOGE(TAG, "Storage host not available");
  return false;
#endif
}

uint8_t *PictureViewer::allocate_image_buffer_(size_t size) {
#ifdef ESP32
  // Try to allocate in PSRAM first
  uint8_t *buffer = static_cast<uint8_t *>(heap_caps_malloc(size, MALLOC_CAP_SPIRAM));
  if (buffer != nullptr) {
    ESP_LOGD(TAG, "Allocated %zu bytes in PSRAM", size);
    return buffer;
  }
  ESP_LOGD(TAG, "PSRAM allocation failed, falling back to heap");
#endif

  // Fallback to regular heap
  uint8_t *buffer = static_cast<uint8_t *>(malloc(size));
  if (buffer != nullptr) {
    ESP_LOGD(TAG, "Allocated %zu bytes in heap", size);
  }
  return buffer;
}

// =====================================================
// Actions
// =====================================================

template<typename... Ts> class ShowImageIndexAction : public Action<Ts...> {
 public:
  explicit ShowImageIndexAction(PictureViewer *parent) : parent_(parent) {}

  TEMPLATABLE_VALUE(size_t, index)

  void play(Ts... x) override {
    size_t index = this->index_.value(x...);
    this->parent_->show_image(index);
  }

 protected:
  PictureViewer *parent_;
};

template<typename... Ts> class ShowImagePathAction : public Action<Ts...> {
 public:
  explicit ShowImagePathAction(PictureViewer *parent) : parent_(parent) {}

  TEMPLATABLE_VALUE(std::string, path)

  void play(Ts... x) override {
    std::string path = this->path_.value(x...);
    this->parent_->show_image(path);
  }

 protected:
  PictureViewer *parent_;
};

template<typename... Ts> class NextImageAction : public Action<Ts...> {
 public:
  explicit NextImageAction(PictureViewer *parent) : parent_(parent) {}

  void play(Ts... x) override { this->parent_->next_image(); }

 protected:
  PictureViewer *parent_;
};

template<typename... Ts> class PreviousImageAction : public Action<Ts...> {
 public:
  explicit PreviousImageAction(PictureViewer *parent) : parent_(parent) {}

  void play(Ts... x) override { this->parent_->previous_image(); }

 protected:
  PictureViewer *parent_;
};

template<typename... Ts> class StartSlideshowAction : public Action<Ts...> {
 public:
  explicit StartSlideshowAction(PictureViewer *parent) : parent_(parent) {}

  void play(Ts... x) override { this->parent_->start_slideshow(); }

 protected:
  PictureViewer *parent_;
};

template<typename... Ts> class StopSlideshowAction : public Action<Ts...> {
 public:
  explicit StopSlideshowAction(PictureViewer *parent) : parent_(parent) {}

  void play(Ts... x) override { this->parent_->stop_slideshow(); }

 protected:
  PictureViewer *parent_;
};

}  // namespace picture_viewer
}  // namespace esphome
