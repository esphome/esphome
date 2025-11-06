#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include <string>
#include <string_view>
#include <memory>
#include <vector>
#include <esp_http_server.h>

#ifdef USE_ESP32
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif

// Forward declaration
namespace esphome {
namespace storage_host {
class StorageHost;
}
}  // namespace esphome

namespace esphome {
namespace http_file_server {

static const char *const TAG = "http_file_server";

// Forward declaration
class HttpFileServer;

// Architecture-specific buffer sizes
#if defined(USE_ESP32_VARIANT_ESP32P4)
static constexpr size_t FILE_BUFFER_SIZE = 16384;  // 16KB for P4
static constexpr size_t MAX_DIR_ENTRIES = 512;
#elif defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3)
static constexpr size_t FILE_BUFFER_SIZE = 8192;  // 8KB for S2/S3
static constexpr size_t MAX_DIR_ENTRIES = 512;
#else
static constexpr size_t FILE_BUFFER_SIZE = 4096;  // 4KB default
static constexpr size_t MAX_DIR_ENTRIES = 256;
#endif

// Path utilities
struct Path {
  static constexpr char separator = '/';

  // Return the name of the file/directory
  static std::string file_name(const std::string &path);

  // Is the path an absolute path?
  static bool is_absolute(const std::string &path);

  // Does the path have a trailing slash?
  static bool has_trailing_slash(const std::string &path);

  // Join two paths
  static std::string join(const std::string &first, const std::string &second);

  // Remove root path from absolute path
  static std::string remove_root_path(const std::string &path, const std::string &root);

  // Split path into components
  static std::vector<std::string> split_path(const std::string &path);

  // Get file extension
  static std::string extension(const std::string &file);

  // Get human-readable file type
  static std::string file_type(const std::string &file);

  // Get MIME type for file
  static std::string mime_type(const std::string &file);
};

// File info structure for directory listings
struct FileInfo {
  std::string name;
  std::string path;
  bool is_directory;
  size_t size;
  time_t modified;
};

class HttpFileServer : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  // Configuration setters
  void set_storage_host(storage_host::StorageHost *storage_host) { this->storage_host_ = storage_host; }
  void set_root_path(const std::string &root_path) { this->root_path_ = root_path; }
  void set_url_prefix(const std::string &url_prefix) { this->url_prefix_ = url_prefix; }
  void set_port(uint16_t port) { this->port_ = port; }
  void set_credentials(const std::string &username, const std::string &password) {
    this->username_ = username;
    this->password_ = password;
    this->auth_enabled_ = true;
  }
  void set_upload_enabled(bool enabled) { this->upload_enabled_ = enabled; }
  void set_download_enabled(bool enabled) { this->download_enabled_ = enabled; }
  void set_deletion_enabled(bool enabled) { this->deletion_enabled_ = enabled; }

  // Getters
  const std::string &get_root_path() const { return this->root_path_; }
  const std::string &get_url_prefix() const { return this->url_prefix_; }
  uint16_t get_port() const { return this->port_; }
  bool is_upload_enabled() const { return this->upload_enabled_; }
  bool is_download_enabled() const { return this->download_enabled_; }
  bool is_deletion_enabled() const { return this->deletion_enabled_; }

 private:
  // HTTP server handle
  httpd_handle_t server_{nullptr};

  // Storage host reference
  storage_host::StorageHost *storage_host_{nullptr};

  // Configuration
  std::string root_path_{};
  std::string url_prefix_;
  uint16_t port_;
  std::string username_;
  std::string password_;
  bool auth_enabled_{false};
  bool upload_enabled_{false};
  bool download_enabled_{true};
  bool deletion_enabled_{false};

  // Helper methods
  bool start_server();
  void stop_server();
  bool authenticate(const std::string &auth_header);
  std::string uri_to_filepath(const std::string &uri);
  std::string url_decode(const std::string &src);
  std::string extract_path_from_url(const std::string &url);
  esphome::FixedVector<FileInfo> list_directory(const std::string &path);

  // Authentication helpers
  std::string base64_decode(const std::string &encoded);

  // Multipart form data parsing helpers
  struct MultipartFile {
    std::string filename;
    std::string content_type;
    const char *data;
    size_t size;
  };
  bool parse_multipart_form(const char *body, size_t body_len, const std::string &boundary, MultipartFile &file);

  // HTML generation helpers
  std::string generate_html_header(const std::string &title);
  std::string generate_html_footer();
  std::string generate_breadcrumb(const std::string &current_path);
  std::string generate_file_row(const FileInfo &info, const std::string &uri_prefix);

  // HTTP handlers (static, called via context)
  static esp_err_t handle_get(httpd_req_t *req);
  static esp_err_t handle_post(httpd_req_t *req);
  static esp_err_t handle_delete(httpd_req_t *req);

  // Specific handlers for different GET requests
  esp_err_t handle_directory_listing(httpd_req_t *req, const std::string &filepath);
  esp_err_t handle_file_download(httpd_req_t *req, const std::string &filepath);
};

}  // namespace http_file_server
}  // namespace esphome
