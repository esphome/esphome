#pragma once
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <cstring>
#include <cstdint>
#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/core/optional.h"
#include "esphome/components/image/image.h"
#include "esphome/components/display/display.h"
#include <map>

// Image decoder configuration for ESP-IDF
#ifdef ESP_IDF_VERSION
#define USE_JPEGDEC
#else
#define USE_JPEGDEC
#endif

// Image decoders - only JPEG for now
#ifdef USE_JPEGDEC
#include <JPEGDEC.h>
#endif

// Hardware JPEG decoder for ESP32-P4
#ifdef USE_HARDWARE_JPEG_DECODER
#include "driver/jpeg_decode.h"
#include "driver/jpeg_types.h"
#endif

namespace esphome {
namespace storage_host {

// Forward declarations
class StorageHost;
class StorageImage;

// Image format enums
enum class ImageFormat { RGB565, RGB888, RGBA };

enum class SdByteOrder { LITTLE_ENDIAN_SD, BIG_ENDIAN_SD };

// =====================================================
// StorageHost - Main Storage Class
// =====================================================
class StorageHost : public Component {
 public:
  StorageHost() = default;

  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  // File methods
  bool file_exists(const std::string &path);
  std::string read_file(const std::string &path);
  bool write_file(const std::string &path, const std::string &data);
  std::vector<std::string> list_files(const std::string &path);

  // Mount management
  void register_mount(const std::string &path, const std::string &platform);
  const std::map<std::string, std::string> &get_mounts() const { return this->mounts_; }
  std::string find_mount_for_path(const std::string &path);

 private:
  std::map<std::string, std::string> mounts_;
};

// =====================================================
// StorageImage - Dynamic Image Component
// =====================================================
class StorageImage : public Component, public image::Image {
 public:
  // Constructor - must initialize base class with valid data
  StorageImage() : Component(), image::Image(nullptr, 0, 0, image::IMAGE_TYPE_RGB565, image::TRANSPARENCY_OPAQUE) {}

  // Component lifecycle
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  // Configuration setters
  void set_file_path(const std::string &path) { this->file_path_ = path; }
  void set_storage_host(StorageHost *storage) { this->storage_host_ = storage; }
  void set_format(const std::string &format);
  void set_auto_load(bool auto_load) { this->auto_load_ = auto_load; }
  void set_mount_source(const std::string &mount_source) { this->mount_source_ = mount_source; }
  void set_resize(int width, int height) {
    this->resize_width_ = width;
    this->resize_height_ = height;
  }
  void set_retry_enabled(bool enabled) { this->retry_enabled_ = enabled; }
  void set_retry_interval(uint32_t interval_ms) { this->retry_interval_ms_ = interval_ms; }
  void set_retry_max_attempts(uint32_t max_attempts) { this->retry_max_attempts_ = max_attempts; }
  void set_use_hardware_decoder(bool use_hw) { this->use_hardware_decoder_ = use_hw; }

  // Loading/unloading
  bool load_image();
  bool load_image_from_path(const std::string &path);
  void unload_image();
  bool reload_image();

  // Finalize after load
  void finalize_image_load();

  // Status
  bool is_loaded() const { return this->image_loaded_; }
  const std::string &get_file_path() const { return this->file_path_; }

  // Image data access
  const std::vector<uint8_t> &get_image_buffer() const { return this->image_buffer_; }
  uint8_t *get_image_data() { return this->image_buffer_.empty() ? nullptr : this->image_buffer_.data(); }
  size_t get_image_data_size() const { return this->image_buffer_.size(); }

  // Override Image methods with exact signature from ESPHome
  void draw(int x, int y, display::Display *display, Color color_on, Color color_off) override;
  int get_width() const override { return this->get_current_width(); }
  int get_height() const override { return this->get_current_height(); }

  // Debug info
  std::string get_debug_info() const;

  // Mount ready callback (public so lambdas can access it)
  void on_mount_ready(const std::string &mount_path);

 protected:
  // Image state
  std::string file_path_;
  std::string mount_source_;
  std::string current_mount_platform_;  // Platform detected during load (usb_msc, sd_mmc, etc.)
  StorageHost *storage_host_{nullptr};
  std::vector<uint8_t> image_buffer_;
  bool image_loaded_{false};
  bool auto_load_{true};

  // Image properties
  int image_width_{0};
  int image_height_{0};
  int resize_width_{0};
  int resize_height_{0};
  ImageFormat format_{ImageFormat::RGB565};

  // NEW: Retry mechanism (Ansatz 2)
  bool retry_enabled_{true};          // Enable retry by default
  uint32_t retry_interval_ms_{2000};  // Retry every 2 seconds
  uint32_t retry_max_attempts_{30};   // Max 30 attempts (60 seconds total)
  uint32_t retry_count_{0};           // Current retry count
  uint32_t last_retry_time_{0};       // Timestamp of last retry attempt

 private:
  // Image processing
  bool allocate_image_buffer();
  void set_pixel(int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255);
  size_t get_pixel_size() const;
  size_t get_buffer_size() const;

  // Update base image properties
  void update_base_image_properties();

  int get_current_width() const;
  int get_current_height() const;
  image::ImageType get_esphome_image_type() const;

  // File type detection
  enum class FileType { UNKNOWN, JPEG };

  FileType detect_file_type(const std::vector<uint8_t> &data) const;
  bool is_jpeg_data(const std::vector<uint8_t> &data) const;

  // Image decoding
  bool decode_image(const std::vector<uint8_t> &data);
  bool decode_jpeg_image(const std::vector<uint8_t> &jpeg_data);

  // JPEG decoder
#ifdef USE_JPEGDEC
  static int jpeg_decode_callback(JPEGDRAW *draw);
  JPEGDEC *jpeg_decoder_{nullptr};
  bool jpeg_decode_pixel(int x, int y, uint8_t r, uint8_t g, uint8_t b);
#endif

  // Hardware JPEG decoder (ESP32-P4)
#ifdef USE_HARDWARE_JPEG_DECODER
  bool decode_jpeg_hardware(const std::vector<uint8_t> &jpeg_data);
  jpeg_decoder_handle_t hw_decoder_{nullptr};
#endif

  bool use_hardware_decoder_{true};  // Prefer HW decoder if available

  // Drawing helpers
  void draw_pixels_directly(int x, int y, display::Display *display, Color color_on, Color color_off);
  void draw_pixel_at(display::Display *display, int screen_x, int screen_y, int img_x, int img_y);
  Color get_pixel_color(int x, int y) const;

  // Utility
  void list_directory_contents(const std::string &dir_path);
  bool extract_jpeg_dimensions(const std::vector<uint8_t> &data, int &width, int &height) const;
  std::string format_to_string() const;
};

}  // namespace storage_host
}  // namespace esphome
