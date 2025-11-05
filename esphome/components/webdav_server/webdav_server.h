#pragma once

#include "esphome/core/component.h"
#include <string>
#include <vector>
#include <map>
#include <esp_http_server.h>

// Forward declaration
namespace esphome {
namespace storage_host {
class StorageHost;
}
}  // namespace esphome

namespace esphome {
namespace webdav_server {

static const char *const TAG = "webdav_server";

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
  std::vector<std::string> list_dir(const std::string &path);

  // HTTP handlers (static, called via context)
  static esp_err_t handle_get(httpd_req_t *req);
  static esp_err_t handle_put(httpd_req_t *req);
  static esp_err_t handle_delete(httpd_req_t *req);
  static esp_err_t handle_propfind(httpd_req_t *req);
  static esp_err_t handle_mkcol(httpd_req_t *req);
  static esp_err_t handle_move(httpd_req_t *req);
  static esp_err_t handle_copy(httpd_req_t *req);
};

}  // namespace webdav_server
}  // namespace esphome
