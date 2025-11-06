#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include <string>
#include <string_view>
#include <memory>
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
namespace webdav_server {

static const char *const TAG = "webdav_server";

// Forward declaration
class WebDAVServer;

// Operation status tracking
enum class OperationStatus { IN_PROGRESS, COMPLETED, FAILED };

struct OperationState {
  std::string operation_id;
  std::string operation_type;  // "copy" or "move"
  std::string src_path;
  std::string dst_path;
  off_t file_size;
  size_t bytes_processed;
  OperationStatus status;
  uint32_t start_time;  // millis()
  std::string error_message;
};

// Task parameter structures for async operations
struct CopyTaskParams {
  WebDAVServer *server;
  std::string operation_id;
  std::string src_path;
  std::string dst_path;
  off_t file_size;
};

struct MoveTaskParams {
  WebDAVServer *server;
  std::string operation_id;
  std::string src_path;
  std::string dst_path;
  off_t file_size;
};

// Architecture-specific buffer sizes
#if defined(USE_ESP32_VARIANT_ESP32P4)
static constexpr size_t FILE_BUFFER_SIZE = 16384;  // 16KB for P4
static constexpr size_t DEST_BUFFER_SIZE = 2048;
static constexpr size_t DEPTH_BUFFER_SIZE = 40;
static constexpr size_t MAX_DIR_ENTRIES = 512;
#elif defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3)
static constexpr size_t FILE_BUFFER_SIZE = 8192;  // 8KB for S2/S3
static constexpr size_t DEST_BUFFER_SIZE = 1024;
static constexpr size_t DEPTH_BUFFER_SIZE = 20;
static constexpr size_t MAX_DIR_ENTRIES = 512;
#else
static constexpr size_t FILE_BUFFER_SIZE = 4096;  // 4KB default
static constexpr size_t DEST_BUFFER_SIZE = 512;
static constexpr size_t DEPTH_BUFFER_SIZE = 10;
static constexpr size_t MAX_DIR_ENTRIES = 256;
#endif

class WebDAVServer : public Component {
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

  // Getters
  const std::string &get_root_path() const { return this->root_path_; }
  const std::string &get_url_prefix() const { return this->url_prefix_; }
  uint16_t get_port() const { return this->port_; }

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

  // Helper methods
  bool start_server();
  void stop_server();
  bool authenticate(const std::string &auth_header);
  std::string uri_to_filepath(const std::string &uri);
  std::string url_decode(const std::string &src);
  std::string extract_path_from_url(const std::string &url);
  std::string generate_prop_xml(const std::string &href, bool is_directory, time_t modified, size_t size);
  esphome::FixedVector<std::string> list_dir(const std::string &path);

  // HTTP handlers (static, called via context)
  static esp_err_t handle_options(httpd_req_t *req);
  static esp_err_t handle_get(httpd_req_t *req);
  static esp_err_t handle_put(httpd_req_t *req);
  static esp_err_t handle_delete(httpd_req_t *req);
  static esp_err_t handle_propfind(httpd_req_t *req);
  static esp_err_t handle_mkcol(httpd_req_t *req);
  static esp_err_t handle_move(httpd_req_t *req);
  static esp_err_t handle_copy(httpd_req_t *req);
  static esp_err_t handle_status(httpd_req_t *req);  // Status endpoint

  // Async task functions (FreeRTOS tasks)
#ifdef USE_ESP32
  static void copy_task(void *params);
  static void move_task(void *params);
#endif

  // Helper for performing synchronous file operations
  bool perform_file_copy(const std::string &src_path, const std::string &dst_path, off_t file_size,
                         const std::string &operation_id = "", httpd_req_t *req = nullptr);
  bool perform_file_move(const std::string &src_path, const std::string &dst_path, off_t file_size,
                         const std::string &operation_id = "", httpd_req_t *req = nullptr);

  // Operation tracking
  std::string create_operation_id();
  void update_operation_status(const std::string &op_id, OperationStatus status, const std::string &error = "");
  void update_operation_progress(const std::string &op_id, size_t bytes_processed);
  OperationState *get_operation_status(const std::string &op_id);
  void cleanup_old_operations();

 private:
  // Operation tracking map (operation_id -> state)
  std::vector<OperationState> operations_;
  uint32_t next_operation_id_{1};
};

}  // namespace webdav_server
}  // namespace esphome
