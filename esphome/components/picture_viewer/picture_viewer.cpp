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

#ifdef USE_HARDWARE_JPEG_DECODER
  if (this->hw_decoder_ != nullptr) {
    jpeg_del_decoder_engine(this->hw_decoder_);
    this->hw_decoder_ = nullptr;
  }
#endif

#ifdef USE_JPEGDEC
  if (this->jpeg_decoder_ != nullptr) {
    delete this->jpeg_decoder_;
    this->jpeg_decoder_ = nullptr;
  }
#endif
}

void PictureViewer::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Picture Viewer...");

#ifdef USE_HARDWARE_JPEG_DECODER
  // Initialize hardware JPEG decoder for ESP32-P4
  jpeg_decode_engine_cfg_t decode_eng_cfg = {
      .timeout_ms = 40,
  };
  esp_err_t ret = jpeg_new_decoder_engine(&decode_eng_cfg, &this->hw_decoder_);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to create hardware JPEG decoder: %d", ret);
    this->mark_failed();
    return;
  }
  ESP_LOGI(TAG, "Hardware JPEG decoder initialized");
#endif

#ifdef USE_JPEGDEC
  // Initialize JPEGDec library
  this->jpeg_decoder_ = new JPEGDEC();
  ESP_LOGI(TAG, "JPEGDec library initialized");
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

  // Scan directory
  this->scan_directory_();

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

void PictureViewer::scan_directory_() {
#ifdef USE_STORAGE_HOST
  if (this->file_manager_ == nullptr) {
    ESP_LOGW(TAG, "File manager not set");
    return;
  }

  auto files = this->file_manager_->get_files();
  for (const auto &file : files) {
    // Filter JPEG files
    std::string lower_filename = file.filename;
    std::transform(lower_filename.begin(), lower_filename.end(), lower_filename.begin(), ::tolower);

    if (lower_filename.ends_with(".jpg") || lower_filename.ends_with(".jpeg")) {
      ImageEntry entry;
      entry.path = file.path;
      entry.filename = file.filename;
      entry.size = file.size;
      this->images_.push_back(entry);
    }
  }

  // Sort by filename
  std::sort(this->images_.begin(), this->images_.end(),
            [](const ImageEntry &a, const ImageEntry &b) { return a.filename < b.filename; });
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
  if (this->hw_decoder_ == nullptr) {
    ESP_LOGE(TAG, "Hardware JPEG decoder not initialized");
    return false;
  }

  jpeg_decode_picture_info_t pic_info = {};
  esp_err_t ret =
      jpeg_decoder_get_info(jpeg_data.data(), static_cast<uint32_t>(jpeg_data.size()), &pic_info);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to get JPEG info: %d", ret);
    return false;
  }

  width = pic_info.width;
  height = pic_info.height;

  // Allocate output buffer for RGB565
  size_t output_size = width * height * 2;  // RGB565 = 2 bytes per pixel
  rgb565_data.resize(output_size);

  jpeg_decode_cfg_t decode_cfg = {
      .output_format = JPEG_DECODE_OUT_FORMAT_RGB565,
      .rgb_order = JPEG_DEC_RGB_ELEMENT_ORDER_RGB,
  };

  ret = jpeg_decoder_process(this->hw_decoder_, &decode_cfg, jpeg_data.data(),
                             static_cast<uint32_t>(jpeg_data.size()), rgb565_data.data(),
                             static_cast<uint32_t>(output_size), nullptr);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Hardware JPEG decode failed: %d", ret);
    return false;
  }

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
  if (this->canvas_id_.empty()) {
    ESP_LOGW(TAG, "Canvas ID not set");
    return;
  }

  // TODO: Update LVGL canvas with current_image_data_
  // This requires LVGL integration to find the canvas widget and update it
  ESP_LOGD(TAG, "Updating canvas: %s", this->canvas_id_.c_str());
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
  // TODO: Get canvas dimensions from LVGL widget
  // For now, use defaults
  if (this->canvas_width_ == 0 || this->canvas_height_ == 0) {
    this->canvas_width_ = 800;
    this->canvas_height_ = 480;
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
