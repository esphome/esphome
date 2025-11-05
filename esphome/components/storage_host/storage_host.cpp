#include "storage_host.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"  // For App.feed_wdt()
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <algorithm>

// Include yield function for ESP32/ESP8266
#ifdef ESP32
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#define yield() taskYIELD()
#elif defined(ESP8266)
#include <Esp.h>
// yield() is already available on ESP8266
#else
// Fallback for other platforms
#define yield() delayMicroseconds(1)
#endif

namespace esphome {
namespace storage_host {

static const char *const TAG = "storage_host";

// Global decoder instances for callbacks
static StorageImage *current_storage_image = nullptr;

// =====================================================
// StorageHost Implementation
// =====================================================

void StorageHost::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Storage Host Component...");
  ESP_LOGCONFIG(TAG, "  Mounts configured: %zu", this->mounts_.size());
  for (const auto &mount : this->mounts_) {
    ESP_LOGCONFIG(TAG, "    - %s (platform: %s)", mount.first.c_str(), mount.second.c_str());
  }
}

void StorageHost::loop() {
  // Nothing to do in loop
}

void StorageHost::dump_config() {
  ESP_LOGCONFIG(TAG, "Storage Host Component:");
  ESP_LOGCONFIG(TAG, "  Mounts: %zu", this->mounts_.size());
  for (const auto &mount : this->mounts_) {
    ESP_LOGCONFIG(TAG, "    - %s (platform: %s)", mount.first.c_str(), mount.second.c_str());
  }
}

void StorageHost::register_mount(const std::string &path, const std::string &platform) {
  this->mounts_[path] = platform;
  ESP_LOGD(TAG, "Registered mount: %s (platform: %s)", path.c_str(), platform.c_str());
}

std::string StorageHost::find_mount_for_path(const std::string &path) {
  // Find the longest matching mount point
  std::string best_mount;
  size_t best_length = 0;

  for (const auto &mount : this->mounts_) {
    if (path.compare(0, mount.first.length(), mount.first) == 0) {
      if (mount.first.length() > best_length) {
        best_mount = mount.first;
        best_length = mount.first.length();
      }
    }
  }

  return best_mount;
}

bool StorageHost::file_exists(const std::string &path) {
  struct stat st;
  return stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

std::string StorageHost::read_file(const std::string &path) {
  FILE *file = fopen(path.c_str(), "rb");

  if (!file) {
    ESP_LOGE(TAG, "Failed to open file: %s (errno: %d)", path.c_str(), errno);
    return "";
  }

  // Get file size safely
  if (fseek(file, 0, SEEK_END) != 0) {
    ESP_LOGE(TAG, "Failed to seek to end of file: %s", path.c_str());
    fclose(file);
    return "";
  }

  long size = ftell(file);
  if (size < 0 || size > 10 * 1024 * 1024) {  // 10MB limit
    ESP_LOGE(TAG, "Invalid file size: %ld bytes", size);
    fclose(file);
    return "";
  }

  if (fseek(file, 0, SEEK_SET) != 0) {
    ESP_LOGE(TAG, "Failed to seek to beginning of file: %s", path.c_str());
    fclose(file);
    return "";
  }

  std::string data(size, '\0');
  size_t read_size = fread(&data[0], 1, size, file);
  fclose(file);

  if (read_size != static_cast<size_t>(size)) {
    ESP_LOGE(TAG, "Failed to read complete file: expected %ld, got %zu", size, read_size);
    return "";
  }

  return data;
}

bool StorageHost::write_file(const std::string &path, const std::string &data) {
  FILE *file = fopen(path.c_str(), "wb");

  if (!file) {
    ESP_LOGE(TAG, "Failed to create file: %s", path.c_str());
    return false;
  }

  size_t written = fwrite(data.data(), 1, data.size(), file);
  fclose(file);

  return written == data.size();
}

std::vector<std::string> StorageHost::list_files(const std::string &path) {
  std::vector<std::string> files;

  DIR *dir = opendir(path.c_str());
  if (!dir) {
    ESP_LOGE(TAG, "Cannot open directory: %s", path.c_str());
    return files;
  }

  struct dirent *entry;
  while ((entry = readdir(dir)) != nullptr) {
    if (entry->d_type == DT_REG) {
      files.push_back(entry->d_name);
    }
  }

  closedir(dir);
  return files;
}

// =====================================================
// StorageImage Implementation
// =====================================================

void StorageImage::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Storage Image Component...");
  ESP_LOGCONFIG(TAG, "  File path: %s", this->file_path_.c_str());
  ESP_LOGCONFIG(TAG, "  Format: %s", this->format_to_string().c_str());
  ESP_LOGCONFIG(TAG, "  Auto load: %s", this->auto_load_ ? "YES" : "NO");
  ESP_LOGCONFIG(TAG, "  Storage host: %s", this->storage_host_ ? "configured" : "not configured");
  ESP_LOGCONFIG(TAG, "  Retry enabled: %s", this->retry_enabled_ ? "YES" : "NO");
  if (this->retry_enabled_) {
    ESP_LOGCONFIG(TAG, "  Retry interval: %u ms", this->retry_interval_ms_);
    ESP_LOGCONFIG(TAG, "  Retry max attempts: %u", this->retry_max_attempts_);
  }
  ESP_LOGCONFIG(TAG, "  Decoders: JPEG available");

  // Auto-load image if enabled
  if (this->auto_load_) {
    if (this->file_path_.empty()) {
      ESP_LOGW(TAG, "Auto-load enabled but no file path configured!");
      return;
    }

    if (!this->storage_host_) {
      ESP_LOGW(TAG, "Auto-load enabled but no storage host configured!");
      return;
    }

    ESP_LOGI(TAG, "Attempting to auto-load image from: %s", this->file_path_.c_str());
    if (this->load_image()) {
      ESP_LOGI(TAG, "Image auto-loaded successfully!");
      this->retry_count_ = 0;  // Reset retry counter on success
    } else {
      if (this->retry_enabled_) {
        ESP_LOGI(TAG, "Initial load failed, retry mechanism enabled (will retry every %u ms)",
                 this->retry_interval_ms_);
        this->last_retry_time_ = millis();
      } else {
        ESP_LOGW(TAG, "Failed to auto-load image (retry disabled)");
      }
    }
  }
}

void StorageImage::loop() {
  // NEW: Retry mechanism (Ansatz 2)
  // If image is not loaded and retry is enabled, periodically try to load it
  if (!this->image_loaded_ && this->retry_enabled_ && this->auto_load_) {
    uint32_t now = millis();

    // Check if enough time has passed since last retry
    if (now - this->last_retry_time_ >= this->retry_interval_ms_) {
      // Check if we haven't exceeded max attempts
      if (this->retry_count_ < this->retry_max_attempts_) {
        this->retry_count_++;
        this->last_retry_time_ = now;

        ESP_LOGD(TAG, "Retry attempt %u/%u to load image from: %s", this->retry_count_, this->retry_max_attempts_,
                 this->file_path_.c_str());

        if (this->load_image()) {
          ESP_LOGI(TAG, "Image loaded successfully on retry attempt %u!", this->retry_count_);
          this->retry_count_ = 0;  // Reset counter on success
        }
      } else if (this->retry_count_ == this->retry_max_attempts_) {
        // Log once when max attempts reached
        ESP_LOGW(TAG, "Max retry attempts (%u) reached for image: %s", this->retry_max_attempts_,
                 this->file_path_.c_str());
        this->retry_count_++;  // Increment to prevent repeated logging
      }
    }
  }
}

void StorageImage::dump_config() {
  ESP_LOGCONFIG(TAG, "SD Image Component:");
  ESP_LOGCONFIG(TAG, "  File: %s", this->file_path_.c_str());
  ESP_LOGCONFIG(TAG, "  Dimensions: %dx%d", this->image_width_, this->image_height_);
  ESP_LOGCONFIG(TAG, "  Format: %s", this->format_to_string().c_str());
  ESP_LOGCONFIG(TAG, "  Loaded: %s", this->image_loaded_ ? "YES" : "NO");
  if (this->image_loaded_) {
    ESP_LOGCONFIG(TAG, "  Buffer size: %zu bytes", this->image_buffer_.size());
    ESP_LOGCONFIG(TAG, "  Base Image - W:%d H:%d Type:%d Data:%p", this->width_, this->height_, this->type_,
                  this->data_start_);
  }
}

// Set image format from string (called from Python codegen)
void StorageImage::set_format(const std::string &format) {
  if (format == "RGB565") {
    this->format_ = ImageFormat::RGB565;
  } else if (format == "RGB888") {
    this->format_ = ImageFormat::RGB888;
  } else if (format == "RGBA") {
    this->format_ = ImageFormat::RGBA;
  } else {
    ESP_LOGW(TAG, "Unknown format: %s, using RGB565", format.c_str());
    this->format_ = ImageFormat::RGB565;
  }
}

// Implementation de la méthode draw() selon le code source ESPHome
void StorageImage::draw(int x, int y, display::Display *display, Color color_on, Color color_off) {
  if (!this->image_loaded_ || this->image_buffer_.empty()) {
    ESP_LOGW(TAG, "Cannot draw: image not loaded");
    return;
  }

  ESP_LOGD(TAG, "Drawing SD image %dx%d at position %d,%d (Base: W:%d H:%d Data:%p)", this->get_current_width(),
           this->get_current_height(), x, y, this->width_, this->height_, this->data_start_);

  // Si les données de base sont correctes, utiliser la méthode optimisée d'ESPHome
  if (this->data_start_ && this->width_ > 0 && this->height_ > 0) {
    ESP_LOGD(TAG, "Using ESPHome base image draw method");
    // Appeler la méthode de base qui gère le clipping et l'optimisation
    image::Image::draw(x, y, display, color_on, color_off);
  } else {
    ESP_LOGD(TAG, "Using fallback pixel-by-pixel drawing");
    // Fallback: dessiner pixel par pixel
    this->draw_pixels_directly(x, y, display, color_on, color_off);
  }
}

// Loading methods
bool StorageImage::load_image() { return this->load_image_from_path(this->file_path_); }

bool StorageImage::load_image_from_path(const std::string &path) {
  ESP_LOGI(TAG, "Loading image from: %s", path.c_str());

  if (!this->storage_host_) {
    ESP_LOGE(TAG, "Storage host not available");
    return false;
  }

  // Unload previous image
  this->unload_image();

  // Auto-detect mount from file path (if not explicitly specified)
  std::string mount_platform;
  if (this->mount_source_.empty()) {
    std::string detected_mount = this->storage_host_->find_mount_for_path(path);
    if (detected_mount.empty()) {
      ESP_LOGW(TAG, "File path does not match any configured mount: %s", path.c_str());
      ESP_LOGW(TAG, "Configured mounts:");
      for (const auto &mount : this->storage_host_->get_mounts()) {
        ESP_LOGW(TAG, "  - %s", mount.first.c_str());
      }
      return false;
    }
    ESP_LOGD(TAG, "Auto-detected mount for path: %s -> %s", path.c_str(), detected_mount.c_str());

    // Get platform for this mount
    const auto &mounts = this->storage_host_->get_mounts();
    auto mount_iter = mounts.find(detected_mount);
    if (mount_iter != mounts.end()) {
      mount_platform = mount_iter->second;
      ESP_LOGI(TAG, "Mount platform: %s", mount_platform.c_str());
      // Store platform for use in decoder
      this->current_mount_platform_ = mount_platform;
    }
  } else {
    // Use explicitly specified mount source
    const auto &mounts = this->storage_host_->get_mounts();
    auto mount_iter = mounts.find(this->mount_source_);
    if (mount_iter != mounts.end()) {
      mount_platform = mount_iter->second;
      ESP_LOGI(TAG, "Mount platform (explicit): %s", mount_platform.c_str());
      this->current_mount_platform_ = mount_platform;
    }
  }

  // Check file existence
  if (!this->storage_host_->file_exists(path)) {
    ESP_LOGE(TAG, "Image file not found: %s", path.c_str());

    // List directory for debugging
    std::string dir_path = path.substr(0, path.find_last_of("/"));
    if (dir_path.empty())
      dir_path = "/";
    this->list_directory_contents(dir_path);

    return false;
  }

  // Read file data
  std::string file_data_str = this->storage_host_->read_file(path);
  if (file_data_str.empty()) {
    ESP_LOGE(TAG, "Failed to read image file: %s", path.c_str());
    return false;
  }

  // Convert std::string to std::vector<uint8_t>
  std::vector<uint8_t> file_data(file_data_str.begin(), file_data_str.end());

  ESP_LOGI(TAG, "Read %zu bytes from file", file_data.size());

  // Show first few bytes for debugging
  if (file_data.size() >= 16) {
    ESP_LOGI(TAG, "First 16 bytes: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
             file_data[0], file_data[1], file_data[2], file_data[3], file_data[4], file_data[5], file_data[6],
             file_data[7], file_data[8], file_data[9], file_data[10], file_data[11], file_data[12], file_data[13],
             file_data[14], file_data[15]);
  }

  // Decode image
  if (!this->decode_image(file_data)) {
    ESP_LOGE(TAG, "Failed to decode image: %s", path.c_str());
    return false;
  }

  this->file_path_ = path;
  this->image_loaded_ = true;

  // Finaliser le chargement en mettant à jour les propriétés de base
  this->finalize_image_load();

  ESP_LOGI(TAG, "Image loaded successfully: %dx%d, %zu bytes", this->image_width_, this->image_height_,
           this->image_buffer_.size());

  return true;
}

void StorageImage::unload_image() {
  this->image_buffer_.clear();
  this->image_buffer_.shrink_to_fit();
  this->image_loaded_ = false;
  this->image_width_ = 0;
  this->image_height_ = 0;

  // Cleanup hardware JPEG decoder if it exists
#ifdef USE_HARDWARE_JPEG_DECODER
  if (this->hw_decoder_ != nullptr) {
    jpeg_del_decoder_engine(this->hw_decoder_);
    this->hw_decoder_ = nullptr;
    ESP_LOGD(TAG, "Hardware JPEG decoder engine released");
  }
#endif

  // Réinitialiser aussi les propriétés de la classe de base
  this->width_ = 0;
  this->height_ = 0;
  this->data_start_ = nullptr;
  this->bpp_ = 0;
  this->stride_ = 0;
}

bool StorageImage::reload_image() {
  std::string path = this->file_path_;
  this->unload_image();
  return this->load_image_from_path(path);
}

// Implémentation de finalize_image_load()
void StorageImage::finalize_image_load() {
  if (this->image_loaded_) {
    this->update_base_image_properties();
    ESP_LOGI(TAG, "Image properties updated - W:%d H:%d Type:%d Data:%p BPP:%d", this->width_, this->height_,
             this->type_, this->data_start_, this->bpp_);
  }
}

// Mise à jour des propriétés de la classe de base selon le code source ESPHome
void StorageImage::update_base_image_properties() {
  // Mettre à jour les membres de la classe de base
  this->width_ = this->get_current_width();
  this->height_ = this->get_current_height();
  this->type_ = this->get_esphome_image_type();
  this->transparency_ = image::TRANSPARENCY_OPAQUE;

  if (!this->image_buffer_.empty()) {
    this->data_start_ = this->image_buffer_.data();

    // Calculer bpp selon le code source ESPHome
    switch (this->type_) {
      case image::IMAGE_TYPE_BINARY:
        this->bpp_ = 1;
        break;
      case image::IMAGE_TYPE_GRAYSCALE:
        this->bpp_ = 8;
        break;
      case image::IMAGE_TYPE_RGB565:
        this->bpp_ = 16;
        break;
      case image::IMAGE_TYPE_RGB:
        this->bpp_ = 24;
        break;
      default:
        this->bpp_ = 16;
        break;
    }

    // Calculer stride (distance en bytes entre deux lignes)
    this->stride_ = (this->width_ * this->bpp_ + 7u) / 8u;
  } else {
    this->data_start_ = nullptr;
    this->bpp_ = 0;
    this->stride_ = 0;
  }
}

int StorageImage::get_current_width() const {
  return this->resize_width_ > 0 ? this->resize_width_ : this->image_width_;
}

int StorageImage::get_current_height() const {
  return this->resize_height_ > 0 ? this->resize_height_ : this->image_height_;
}

image::ImageType StorageImage::get_esphome_image_type() const {
  switch (this->format_) {
    case ImageFormat::RGB565:
      return image::IMAGE_TYPE_RGB565;
    case ImageFormat::RGB888:
      return image::IMAGE_TYPE_RGB;
    case ImageFormat::RGBA:
      return image::IMAGE_TYPE_RGB;  // ESPHome n'a pas de RGBA natif
    default:
      return image::IMAGE_TYPE_RGB565;
  }
}

void StorageImage::draw_pixels_directly(int x, int y, display::Display *display, Color color_on, Color color_off) {
  // Méthode de fallback pour dessiner directement
  ESP_LOGD(TAG, "Drawing %dx%d pixels directly", this->get_current_width(), this->get_current_height());

  for (int img_y = 0; img_y < this->get_current_height(); img_y++) {
    for (int img_x = 0; img_x < this->get_current_width(); img_x++) {
      Color pixel_color = this->get_pixel_color(img_x, img_y);
      display->draw_pixel_at(x + img_x, y + img_y, pixel_color);
    }

    // Yield périodique pour éviter le watchdog
    if (img_y % 32 == 0) {
      App.feed_wdt();
      yield();
    }
  }
}

void StorageImage::draw_pixel_at(display::Display *display, int screen_x, int screen_y, int img_x, int img_y) {
  Color pixel_color = this->get_pixel_color(img_x, img_y);
  display->draw_pixel_at(screen_x, screen_y, pixel_color);
}

Color StorageImage::get_pixel_color(int x, int y) const {
  if (x < 0 || x >= this->get_current_width() || y < 0 || y >= this->get_current_height()) {
    return Color::BLACK;
  }

  size_t offset = (y * this->get_current_width() + x) * this->get_pixel_size();

  if (offset + this->get_pixel_size() > this->image_buffer_.size()) {
    return Color::BLACK;
  }

  switch (this->format_) {
    case ImageFormat::RGB565: {
      uint16_t rgb565 = this->image_buffer_[offset] | (this->image_buffer_[offset + 1] << 8);
      uint8_t r = ((rgb565 >> 11) & 0x1F) << 3;
      uint8_t g = ((rgb565 >> 5) & 0x3F) << 2;
      uint8_t b = (rgb565 & 0x1F) << 3;
      return Color(r, g, b);
    }
    case ImageFormat::RGB888:
      return Color(this->image_buffer_[offset], this->image_buffer_[offset + 1], this->image_buffer_[offset + 2]);
    case ImageFormat::RGBA:
      return Color(this->image_buffer_[offset], this->image_buffer_[offset + 1], this->image_buffer_[offset + 2],
                   this->image_buffer_[offset + 3]);
    default:
      return Color::BLACK;
  }
}

// File type detection
StorageImage::FileType StorageImage::detect_file_type(const std::vector<uint8_t> &data) const {
  if (this->is_jpeg_data(data))
    return FileType::JPEG;
  return FileType::UNKNOWN;
}

bool StorageImage::is_jpeg_data(const std::vector<uint8_t> &data) const {
  return data.size() >= 4 && data[0] == 0xFF && data[1] == 0xD8 && data[2] == 0xFF;
}

// Image decoding
bool StorageImage::decode_image(const std::vector<uint8_t> &data) {
  FileType type = this->detect_file_type(data);

  switch (type) {
    case FileType::JPEG:
      ESP_LOGI(TAG, "Decoding JPEG image");
      return this->decode_jpeg_image(data);

    default:
      ESP_LOGE(TAG, "Unsupported image format (only JPEG supported in this build)");
      return false;
  }
}

// =====================================================
// Hardware JPEG Decoder Implementation (ESP32-P4)
// =====================================================

#ifdef USE_HARDWARE_JPEG_DECODER
bool StorageImage::decode_jpeg_hardware(const std::vector<uint8_t> &jpeg_data) {
  ESP_LOGI(TAG, "Using hardware JPEG decoder (ESP32-P4)");

  // Safety check: Minimum JPEG data size
  if (jpeg_data.size() < 100) {
    ESP_LOGE(TAG, "JPEG data too small: %u bytes", jpeg_data.size());
    return false;
  }

  // Get image info first
  jpeg_decode_picture_info_t pic_info;
  esp_err_t ret = jpeg_decoder_get_info(jpeg_data.data(), jpeg_data.size(), &pic_info);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to get JPEG info: %s", esp_err_to_name(ret));
    return false;
  }

  ESP_LOGI(TAG, "JPEG original dimensions: %dx%d", pic_info.width, pic_info.height);

  // Validate dimensions with safety limits
  if (pic_info.width <= 0 || pic_info.height <= 0 || pic_info.width > 1920 || pic_info.height > 1080) {
    ESP_LOGE(TAG, "Invalid JPEG dimensions: %dx%d (max 1920x1080)", pic_info.width, pic_info.height);
    return false;
  }

  // Hardware decoder decodes to full size first
  int decoded_width = pic_info.width;
  int decoded_height = pic_info.height;

  // Check if we have enough memory before allocating
  size_t required_memory =
      jpeg_data.size() + (decoded_width * decoded_height * (this->format_ == ImageFormat::RGB565 ? 2 : 3));
  size_t free_heap = heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  ESP_LOGI(TAG, "Required memory: %u bytes, Free PSRAM: %u bytes", required_memory, free_heap);

  if (free_heap < required_memory + 100000) {  // Keep 100KB safety margin
    ESP_LOGE(TAG, "Not enough memory for hardware decode (need %u, have %u)", required_memory, free_heap);
    return false;
  }

  // Initialize decoder engine if not already done
  if (this->hw_decoder_ == nullptr) {
    jpeg_decode_engine_cfg_t eng_cfg = {
        .intr_priority = 0,
        .timeout_ms = 200,
    };
    ret = jpeg_new_decoder_engine(&eng_cfg, &this->hw_decoder_);
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "Failed to create JPEG decoder engine: %s", esp_err_to_name(ret));
      return false;
    }
    ESP_LOGI(TAG, "Hardware JPEG decoder engine initialized");
  }

  // Configure decode parameters
  jpeg_decode_cfg_t decode_cfg = {};
  if (this->format_ == ImageFormat::RGB565) {
    decode_cfg.output_format = JPEG_DECODE_OUT_FORMAT_RGB565;
    ESP_LOGI(TAG, "Using RGB565 output format");
  } else if (this->format_ == ImageFormat::RGB888) {
    decode_cfg.output_format = JPEG_DECODE_OUT_FORMAT_RGB888;
    ESP_LOGI(TAG, "Using RGB888 output format");
  } else {
    ESP_LOGE(TAG, "Unsupported format for hardware decoder (only RGB565 and RGB888 supported)");
    return false;
  }

  // Try RGB order instead of BGR
  decode_cfg.rgb_order = JPEG_DEC_RGB_ELEMENT_ORDER_RGB;
  ESP_LOGI(TAG, "Using RGB pixel order for hardware decoder");

  // Calculate buffer sizes with alignment
  uint32_t input_size = jpeg_data.size();
  uint32_t output_size = decoded_width * decoded_height * (this->format_ == ImageFormat::RGB565 ? 2 : 3);

  // Allocate aligned buffers (hardware requires 16-byte alignment)
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

  // Decode with timing
  uint32_t decode_start = millis();
  ESP_LOGD(TAG, "Calling jpeg_decoder_process: decoder=%p, cfg=%p, in=%p, in_size=%u, out=%p, out_size=%u",
           this->hw_decoder_, &decode_cfg, aligned_input, input_size, aligned_output, output_size);
  ESP_LOGD(TAG, "Decode config: output_format=%d, rgb_order=%d", decode_cfg.output_format, decode_cfg.rgb_order);

  ret = jpeg_decoder_process(this->hw_decoder_, &decode_cfg, aligned_input, input_size, aligned_output, output_size,
                             &output_size);

  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Hardware JPEG decode failed: %s (decoder=%p, in_size=%u, out_size=%u)", esp_err_to_name(ret),
             this->hw_decoder_, input_size, output_size);
    ESP_LOGE(TAG, "Mount platform: %s, Current file: %s", this->current_mount_platform_.c_str(),
             this->file_path_.c_str());
    free(aligned_input);
    free(aligned_output);
    return false;
  }

  uint32_t decode_time = millis() - decode_start;
  ESP_LOGI(TAG, "Hardware decode completed in %u ms, output size: %u bytes", decode_time, output_size);

  // Byte-swap RGB565 for correct endianness
  // Hardware decoder outputs in one endianness, but display may expect the other
  if (this->format_ == ImageFormat::RGB565) {
    ESP_LOGI(TAG, "Byte-swapping RGB565 data for correct endianness (mount: %s)",
             this->current_mount_platform_.c_str());
    uint16_t *pixels = (uint16_t *) aligned_output;
    size_t pixel_count = output_size / 2;
    for (size_t i = 0; i < pixel_count; i++) {
      // Swap bytes: 0xAABB -> 0xBBAA
      pixels[i] = (pixels[i] << 8) | (pixels[i] >> 8);
    }
  }

  // Now handle resize if needed (Option 1: Software resize after hardware decode)
  bool needs_resize = (this->resize_width_ > 0 && this->resize_height_ > 0 &&
                       (this->resize_width_ != decoded_width || this->resize_height_ != decoded_height));

  if (needs_resize) {
    ESP_LOGI(TAG, "Resizing from %dx%d to %dx%d using software", decoded_width, decoded_height, this->resize_width_,
             this->resize_height_);

    // Set final dimensions to resize target
    this->image_width_ = this->resize_width_;
    this->image_height_ = this->resize_height_;

    // Allocate final buffer
    if (!this->allocate_image_buffer()) {
      free(aligned_input);
      free(aligned_output);
      return false;
    }

    // Perform simple nearest-neighbor resize
    for (int dst_y = 0; dst_y < this->resize_height_; dst_y++) {
      // Yield every 10 rows to prevent watchdog timeout
      if (dst_y % 10 == 0) {
        yield();
      }

      for (int dst_x = 0; dst_x < this->resize_width_; dst_x++) {
        // Map destination coordinates to source
        float src_x = (float) dst_x * decoded_width / this->resize_width_;
        float src_y = (float) dst_y * decoded_height / this->resize_height_;

        int src_x_int = (int) src_x;
        int src_y_int = (int) src_y;

        // Clamp to source bounds
        if (src_x_int >= decoded_width)
          src_x_int = decoded_width - 1;
        if (src_y_int >= decoded_height)
          src_y_int = decoded_height - 1;

        int src_offset = (src_y_int * decoded_width + src_x_int) * (this->format_ == ImageFormat::RGB565 ? 2 : 3);
        int dst_offset = (dst_y * this->resize_width_ + dst_x) * (this->format_ == ImageFormat::RGB565 ? 2 : 3);

        // Copy pixel data directly (no byte swap)
        if (this->format_ == ImageFormat::RGB565) {
          this->image_buffer_[dst_offset] = aligned_output[src_offset];
          this->image_buffer_[dst_offset + 1] = aligned_output[src_offset + 1];
        } else {  // RGB888
          this->image_buffer_[dst_offset] = aligned_output[src_offset];
          this->image_buffer_[dst_offset + 1] = aligned_output[src_offset + 1];
          this->image_buffer_[dst_offset + 2] = aligned_output[src_offset + 2];
        }
      }
    }
  } else {
    // No resize needed, use decoded size
    this->image_width_ = decoded_width;
    this->image_height_ = decoded_height;

    // Allocate buffer for final image
    if (!this->allocate_image_buffer()) {
      free(aligned_input);
      free(aligned_output);
      return false;
    }

    // Copy decoded data to image buffer (direct copy, no byte swap)
    size_t copy_size = std::min((size_t) output_size, this->image_buffer_.size());
    memcpy(this->image_buffer_.data(), aligned_output, copy_size);
  }

  // Free aligned buffers
  free(aligned_input);
  free(aligned_output);

  ESP_LOGI(TAG, "Hardware JPEG decode successful, final size: %dx%d", this->image_width_, this->image_height_);
  return true;
}
#endif  // USE_HARDWARE_JPEG_DECODER

// =====================================================
// JPEG Decoder Implementation (ESPHome style)
// =====================================================

bool StorageImage::decode_jpeg_image(const std::vector<uint8_t> &jpeg_data) {
#ifdef USE_HARDWARE_JPEG_DECODER
  // Try hardware decoder first if enabled
  if (this->use_hardware_decoder_) {
    ESP_LOGD(TAG, "Attempting hardware JPEG decode...");
    if (this->decode_jpeg_hardware(jpeg_data)) {
      return true;
    }
    ESP_LOGW(TAG, "Hardware decode failed, falling back to software decoder");
  }
#endif

  ESP_LOGD(TAG, "Using JPEGDEC software decoder");

  // Set current component for callback
  current_storage_image = this;

  // Create decoder
  this->jpeg_decoder_ = new JPEGDEC();
  if (!this->jpeg_decoder_) {
    ESP_LOGE(TAG, "Failed to allocate JPEG decoder");
    current_storage_image = nullptr;
    return false;
  }

  // Configuration correcte du décodeur JPEGDEC
  // Forcer le format RGB565 directement dans JPEGDEC
  this->format_ = ImageFormat::RGB565;

  // Open JPEG avec validation
  int result =
      this->jpeg_decoder_->openRAM((uint8_t *) jpeg_data.data(), jpeg_data.size(), StorageImage::jpeg_decode_callback);
  if (result != 1) {
    ESP_LOGE(TAG, "Failed to open JPEG data: %d", result);
    delete this->jpeg_decoder_;
    this->jpeg_decoder_ = nullptr;
    current_storage_image = nullptr;
    return false;
  }

  // Get image dimensions
  int orig_width = this->jpeg_decoder_->getWidth();
  int orig_height = this->jpeg_decoder_->getHeight();

  ESP_LOGI(TAG, "JPEG original dimensions: %dx%d", orig_width, orig_height);

  // Validate dimensions
  if (orig_width <= 0 || orig_height <= 0 || orig_width > 2048 || orig_height > 2048) {
    ESP_LOGE(TAG, "Invalid JPEG dimensions: %dx%d", orig_width, orig_height);
    this->jpeg_decoder_->close();
    delete this->jpeg_decoder_;
    this->jpeg_decoder_ = nullptr;
    current_storage_image = nullptr;
    return false;
  }

  // Gestion correcte du redimensionnement
  if (this->resize_width_ > 0 && this->resize_height_ > 0) {
    this->image_width_ = this->resize_width_;
    this->image_height_ = this->resize_height_;
    ESP_LOGI(TAG, "Will resize to: %dx%d", this->image_width_, this->image_height_);
  } else {
    this->image_width_ = orig_width;
    this->image_height_ = orig_height;
  }

  // Allocate buffer
  if (!this->allocate_image_buffer()) {
    this->jpeg_decoder_->close();
    delete this->jpeg_decoder_;
    this->jpeg_decoder_ = nullptr;
    current_storage_image = nullptr;
    return false;
  }

  // Initialisation propre du buffer
  std::fill(this->image_buffer_.begin(), this->image_buffer_.end(), 0);

  ESP_LOGI(TAG, "Starting JPEG decode to RGB565...");

  // Paramètres de décodage optimisés
  // decode(x, y, flags)
  int decode_flags = 0;

  result = this->jpeg_decoder_->decode(0, 0, decode_flags);

  // Cleanup
  this->jpeg_decoder_->close();
  delete this->jpeg_decoder_;
  this->jpeg_decoder_ = nullptr;
  current_storage_image = nullptr;

  if (result != 1) {
    ESP_LOGE(TAG, "Failed to decode JPEG: %d", result);
    return false;
  }

  ESP_LOGI(TAG, "JPEG decoded successfully to RGB565 format");

  // Validation finale
  if (this->image_buffer_.empty()) {
    ESP_LOGE(TAG, "Image buffer is empty after decoding");
    return false;
  }

  // Log amélioré pour debug
  if (this->image_buffer_.size() >= 8) {
    uint16_t pixel1 = (this->image_buffer_[1] << 8) | this->image_buffer_[0];
    uint16_t pixel2 = (this->image_buffer_[3] << 8) | this->image_buffer_[2];
    uint16_t pixel3 = (this->image_buffer_[5] << 8) | this->image_buffer_[4];
    uint16_t pixel4 = (this->image_buffer_[7] << 8) | this->image_buffer_[6];

    ESP_LOGD(TAG, "First 4 RGB565 pixels: 0x%04X 0x%04X 0x%04X 0x%04X", pixel1, pixel2, pixel3, pixel4);

    // Décoder les couleurs pour verification
    uint8_t r1 = ((pixel1 >> 11) & 0x1F) << 3;
    uint8_t g1 = ((pixel1 >> 5) & 0x3F) << 2;
    uint8_t b1 = (pixel1 & 0x1F) << 3;
    ESP_LOGD(TAG, "First pixel RGB: R=%d G=%d B=%d", r1, g1, b1);
  }

  return true;
}

// =====================================================
// JPEG Decoder Callback Implementation
// =====================================================

int StorageImage::jpeg_decode_callback(JPEGDRAW *pDraw) {
  // Get the current component instance
  if (!current_storage_image) {
    ESP_LOGE(TAG, "No current image component in callback");
    return 0;  // Stop decoding
  }

  StorageImage *component = current_storage_image;

  // Validate draw parameters
  if (!pDraw || !pDraw->pPixels) {
    ESP_LOGE(TAG, "Invalid draw parameters in callback");
    return 0;
  }

  // Log progress occasionally
  static int callback_count = 0;
  if (++callback_count % 100 == 0) {
    ESP_LOGD(TAG, "JPEG callback: %d,%d %dx%d", pDraw->x, pDraw->y, pDraw->iWidth, pDraw->iHeight);
  }

  // Process pixels - JPEGDEC provides RGB565 pixels directly
  uint16_t *pixels = (uint16_t *) pDraw->pPixels;

  for (int py = 0; py < pDraw->iHeight; py++) {
    for (int px = 0; px < pDraw->iWidth; px++) {
      int img_x = pDraw->x + px;
      int img_y = pDraw->y + py;

      // Apply resize scaling if needed
      if (component->resize_width_ > 0 && component->resize_height_ > 0) {
        int orig_width = component->jpeg_decoder_->getWidth();
        int orig_height = component->jpeg_decoder_->getHeight();

        if (orig_width > 0 && orig_height > 0) {
          img_x = (img_x * component->resize_width_) / orig_width;
          img_y = (img_y * component->resize_height_) / orig_height;
        }
      }

      // Bounds check
      if (img_x >= 0 && img_x < component->image_width_ && img_y >= 0 && img_y < component->image_height_) {
        // Get RGB565 pixel
        uint16_t rgb565 = pixels[py * pDraw->iWidth + px];

        // Store directly as RGB565 in buffer
        size_t offset = (img_y * component->image_width_ + img_x) * 2;
        if (offset + 1 < component->image_buffer_.size()) {
          component->image_buffer_[offset] = rgb565 & 0xFF;
          component->image_buffer_[offset + 1] = (rgb565 >> 8) & 0xFF;
        }
      }
    }

    // Yield periodically to prevent watchdog timeout
    if (py % 16 == 0) {
      App.feed_wdt();
      yield();
    }
  }

  return 1;  // Continue decoding
}

bool StorageImage::jpeg_decode_pixel(int x, int y, uint8_t r, uint8_t g, uint8_t b) {
  // Apply resize scaling if needed
  if (this->resize_width_ > 0 && this->resize_height_ > 0) {
    int orig_width = this->jpeg_decoder_->getWidth();
    int orig_height = this->jpeg_decoder_->getHeight();
    x = (x * this->resize_width_) / orig_width;
    y = (y * this->resize_height_) / orig_height;
  }

  // Bounds check
  if (x < 0 || x >= this->image_width_ || y < 0 || y >= this->image_height_) {
    return false;
  }

  this->set_pixel(x, y, r, g, b);
  return true;
}

// =====================================================
// Helper Methods
// =====================================================

bool StorageImage::allocate_image_buffer() {
  size_t buffer_size = this->get_buffer_size();

  if (buffer_size == 0 || buffer_size > 3 * 1024 * 1024) {  // 3MB limit for ESP32P4
    ESP_LOGE(TAG, "Invalid buffer size: %zu bytes", buffer_size);
    return false;
  }

  this->image_buffer_.clear();
  this->image_buffer_.resize(buffer_size, 0);
  ESP_LOGD(TAG, "Allocated image buffer: %zu bytes", buffer_size);
  return true;
}

void StorageImage::set_pixel(int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
  if (x < 0 || x >= this->image_width_ || y < 0 || y >= this->image_height_) {
    return;
  }

  size_t offset = (y * this->image_width_ + x) * this->get_pixel_size();

  if (offset + this->get_pixel_size() > this->image_buffer_.size()) {
    return;
  }

  switch (this->format_) {
    case ImageFormat::RGB565: {
      uint16_t rgb565 = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
      this->image_buffer_[offset] = rgb565 & 0xFF;
      this->image_buffer_[offset + 1] = (rgb565 >> 8) & 0xFF;
      break;
    }
    case ImageFormat::RGB888:
      this->image_buffer_[offset] = r;
      this->image_buffer_[offset + 1] = g;
      this->image_buffer_[offset + 2] = b;
      break;
    case ImageFormat::RGBA:
      this->image_buffer_[offset] = r;
      this->image_buffer_[offset + 1] = g;
      this->image_buffer_[offset + 2] = b;
      this->image_buffer_[offset + 3] = a;
      break;
  }
}

size_t StorageImage::get_pixel_size() const {
  switch (this->format_) {
    case ImageFormat::RGB565:
      return 2;
    case ImageFormat::RGB888:
      return 3;
    case ImageFormat::RGBA:
      return 4;
    default:
      return 2;
  }
}

size_t StorageImage::get_buffer_size() const {
  return this->image_width_ * this->image_height_ * this->get_pixel_size();
}

std::string StorageImage::format_to_string() const {
  switch (this->format_) {
    case ImageFormat::RGB565:
      return "RGB565";
    case ImageFormat::RGB888:
      return "RGB888";
    case ImageFormat::RGBA:
      return "RGBA";
    default:
      return "Unknown";
  }
}

std::string StorageImage::get_debug_info() const {
  char buffer[256];
  snprintf(buffer, sizeof(buffer), "SdImage[%s]: %dx%d, %s, loaded=%s, size=%zu bytes", this->file_path_.c_str(),
           this->image_width_, this->image_height_, this->format_to_string().c_str(),
           this->image_loaded_ ? "yes" : "no", this->image_buffer_.size());
  return std::string(buffer);
}

void StorageImage::list_directory_contents(const std::string &dir_path) {
  ESP_LOGI(TAG, "Directory listing for: %s", dir_path.c_str());

  DIR *dir = opendir(dir_path.c_str());
  if (!dir) {
    ESP_LOGE(TAG, "Cannot open directory: %s (errno: %d)", dir_path.c_str(), errno);
    return;
  }

  struct dirent *entry;
  int file_count = 0;

  while ((entry = readdir(dir)) != nullptr) {
    std::string full_path = dir_path;
    if (full_path.back() != '/')
      full_path += "/";
    full_path += entry->d_name;

    struct stat st;
    if (stat(full_path.c_str(), &st) == 0) {
      if (S_ISREG(st.st_mode)) {
        ESP_LOGI(TAG, "  📄 %s (%ld bytes)", entry->d_name, (long) st.st_size);
        file_count++;
      } else if (S_ISDIR(st.st_mode)) {
        ESP_LOGI(TAG, "  📁 %s/", entry->d_name);
      }
    }
  }

  closedir(dir);
  ESP_LOGI(TAG, "Total files: %d", file_count);
}

bool StorageImage::extract_jpeg_dimensions(const std::vector<uint8_t> &data, int &width, int &height) const {
  for (size_t i = 0; i < data.size() - 10; i++) {
    if (data[i] == 0xFF) {
      uint8_t marker = data[i + 1];
      if (marker >= 0xC0 && marker <= 0xC3) {
        if (i + 9 < data.size()) {
          height = (data[i + 5] << 8) | data[i + 6];
          width = (data[i + 7] << 8) | data[i + 8];
          return true;
        }
      }
    }
  }
  return false;
}

// NEW: Event-based loading (Ansatz 3)
// Called when a USB mount becomes ready
void StorageImage::on_mount_ready(const std::string &mount_path) {
  // Check if this mount is relevant for our image
  if (this->file_path_.empty()) {
    return;
  }

  // Check if file path starts with this mount path
  if (this->file_path_.compare(0, mount_path.length(), mount_path) != 0) {
    ESP_LOGD(TAG, "Mount '%s' not relevant for image '%s'", mount_path.c_str(), this->file_path_.c_str());
    return;
  }

  ESP_LOGI(TAG, "Mount '%s' is now ready, attempting to load image from: %s", mount_path.c_str(),
           this->file_path_.c_str());

  // Attempt to load the image
  if (this->load_image()) {
    ESP_LOGI(TAG, "Image loaded successfully after mount ready event!");
    this->retry_count_ = 0;  // Reset retry counter on success
  } else {
    ESP_LOGW(TAG, "Failed to load image even after mount ready event");
  }
}

}  // namespace storage_host
}  // namespace esphome
