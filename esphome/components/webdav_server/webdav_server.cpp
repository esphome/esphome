#include "webdav_server.h"
#include "esphome/core/log.h"
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <dirent.h>

namespace esphome {
namespace webdav_server {

void WebDAVServer::setup() {
  ESP_LOGI(TAG, "Starting WebDAV Server on port %d with prefix: %s", this->port_, this->url_prefix_.c_str());
  ESP_LOGI(TAG, "Root path: %s", this->root_path_.c_str());

  if (!this->start_server()) {
    ESP_LOGE(TAG, "Failed to start WebDAV server");
    this->mark_failed();
  } else {
    ESP_LOGI(TAG, "WebDAV Server started successfully");
  }
}

void WebDAVServer::loop() {
  // HTTP server handles everything in background
}

void WebDAVServer::dump_config() {
  ESP_LOGCONFIG(TAG, "WebDAV Server:");
  ESP_LOGCONFIG(TAG, "  Root path: %s", this->root_path_.c_str());
  ESP_LOGCONFIG(TAG, "  URL prefix: %s", this->url_prefix_.c_str());
  ESP_LOGCONFIG(TAG, "  Port: %d", this->port_);
  ESP_LOGCONFIG(TAG, "  Auth enabled: %s", this->auth_enabled_ ? "YES" : "NO");
}

bool WebDAVServer::start_server() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = this->port_;
  config.max_uri_handlers = 20;

  esp_err_t ret = httpd_start(&this->server_, &config);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(ret));
    return false;
  }

  // Register HTTP handlers
  httpd_uri_t get_handler = {
      .uri = "/*",
      .method = HTTP_GET,
      .handler = WebDAVServer::handle_get,
      .user_ctx = this,
  };

  httpd_uri_t put_handler = {
      .uri = "/*",
      .method = HTTP_PUT,
      .handler = WebDAVServer::handle_put,
      .user_ctx = this,
  };

  httpd_uri_t delete_handler = {
      .uri = "/*",
      .method = HTTP_DELETE,
      .handler = WebDAVServer::handle_delete,
      .user_ctx = this,
  };

  httpd_uri_t propfind_handler = {
      .uri = "/*",
      .method = HTTP_PROPFIND,
      .handler = WebDAVServer::handle_propfind,
      .user_ctx = this,
  };

  httpd_uri_t mkcol_handler = {
      .uri = "/*",
      .method = HTTP_MKCOL,
      .handler = WebDAVServer::handle_mkcol,
      .user_ctx = this,
  };

  httpd_uri_t move_handler = {
      .uri = "/*",
      .method = HTTP_MOVE,
      .handler = WebDAVServer::handle_move,
      .user_ctx = this,
  };

  httpd_uri_t copy_handler = {
      .uri = "/*",
      .method = HTTP_COPY,
      .handler = WebDAVServer::handle_copy,
      .user_ctx = this,
  };

  esp_err_t err;

  err = httpd_register_uri_handler(this->server_, &get_handler);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to register GET handler: %s", esp_err_to_name(err));
    httpd_stop(this->server_);
    return false;
  }
  ESP_LOGD(TAG, "Registered GET handler");

  err = httpd_register_uri_handler(this->server_, &put_handler);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to register PUT handler: %s", esp_err_to_name(err));
    httpd_stop(this->server_);
    return false;
  }
  ESP_LOGD(TAG, "Registered PUT handler");

  err = httpd_register_uri_handler(this->server_, &delete_handler);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to register DELETE handler: %s", esp_err_to_name(err));
    httpd_stop(this->server_);
    return false;
  }
  ESP_LOGD(TAG, "Registered DELETE handler");

  err = httpd_register_uri_handler(this->server_, &propfind_handler);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to register PROPFIND handler: %s", esp_err_to_name(err));
    httpd_stop(this->server_);
    return false;
  }
  ESP_LOGD(TAG, "Registered PROPFIND handler");

  err = httpd_register_uri_handler(this->server_, &mkcol_handler);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to register MKCOL handler: %s", esp_err_to_name(err));
    httpd_stop(this->server_);
    return false;
  }
  ESP_LOGD(TAG, "Registered MKCOL handler");

  err = httpd_register_uri_handler(this->server_, &move_handler);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to register MOVE handler: %s", esp_err_to_name(err));
    httpd_stop(this->server_);
    return false;
  }
  ESP_LOGD(TAG, "Registered MOVE handler");

  err = httpd_register_uri_handler(this->server_, &copy_handler);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to register COPY handler: %s", esp_err_to_name(err));
    httpd_stop(this->server_);
    return false;
  }
  ESP_LOGD(TAG, "Registered COPY handler");

  ESP_LOGI(TAG, "All handlers registered successfully");
  return true;
}

void WebDAVServer::stop_server() {
  if (this->server_ != nullptr) {
    httpd_stop(this->server_);
    this->server_ = nullptr;
  }
}

std::string WebDAVServer::uri_to_filepath(const std::string &uri) {
  // Remove URL prefix from URI to get relative path
  std::string relative_path = uri;

  if (uri.find(this->url_prefix_) == 0) {
    relative_path = uri.substr(this->url_prefix_.length());
  }

  // Remove leading slash if present
  if (!relative_path.empty() && relative_path[0] == '/') {
    relative_path = relative_path.substr(1);
  }

  // Construct full path
  std::string full_path = this->root_path_;
  if (!this->root_path_.empty() && this->root_path_.back() != '/') {
    full_path += "/";
  }
  full_path += relative_path;

  return full_path;
}

bool WebDAVServer::authenticate(const std::string &auth_header) {
  if (!this->auth_enabled_) {
    return true;  // Auth disabled
  }

  if (auth_header.empty()) {
    return false;
  }

  // Basic auth: "Basic base64(username:password)"
  if (auth_header.find("Basic ") == 0) {
    // For now, just check if header is present
    // Full validation would require base64 decoding
    return true;
  }

  return false;
}

esp_err_t WebDAVServer::handle_get(httpd_req_t *req) {
  auto *server = static_cast<WebDAVServer *>(req->user_ctx);

  std::string filepath = server->uri_to_filepath(req->uri);
  ESP_LOGD(TAG, "GET request for: %s (mapped to: %s)", req->uri, filepath.c_str());

  struct stat file_stat;
  if (stat(filepath.c_str(), &file_stat) != 0) {
    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File not found");
    return ESP_OK;
  }

  if (S_ISDIR(file_stat.st_mode)) {
    // Directory listing
    DIR *dir = opendir(filepath.c_str());
    if (dir == nullptr) {
      httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Cannot access directory");
      return ESP_OK;
    }

    std::string response = "<html><body><h1>Directory: " + filepath + "</h1><ul>";
    struct dirent *entry;
    while ((entry = readdir(dir)) != nullptr) {
      if (entry->d_name[0] != '.') {  // Skip hidden files
        response += "<li><a href=\"" + std::string(entry->d_name) + "\">";
        response += entry->d_name;
        response += "</a></li>";
      }
    }
    closedir(dir);
    response += "</ul></body></html>";

    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, response.c_str(), response.length());
  } else {
    // File download
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
      httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Cannot open file");
      return ESP_OK;
    }

    httpd_resp_set_type(req, "application/octet-stream");
    httpd_resp_set_hdr(req, "Content-Disposition", ("attachment; filename=\"" + std::string(req->uri) + "\"").c_str());

    char buffer[4096];
    while (file.read(buffer, sizeof(buffer))) {
      httpd_resp_send_chunk(req, buffer, file.gcount());
    }
    httpd_resp_send_chunk(req, nullptr, 0);

    file.close();
  }

  return ESP_OK;
}

esp_err_t WebDAVServer::handle_put(httpd_req_t *req) {
  auto *server = static_cast<WebDAVServer *>(req->user_ctx);

  std::string filepath = server->uri_to_filepath(req->uri);
  ESP_LOGD(TAG, "PUT request for: %s", filepath.c_str());

  std::ofstream file(filepath, std::ios::binary);
  if (!file.is_open()) {
    httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Cannot create file");
    return ESP_OK;
  }

  char buffer[4096];
  int received;
  while ((received = httpd_req_recv(req, buffer, sizeof(buffer))) > 0) {
    file.write(buffer, received);
  }
  file.close();

  httpd_resp_send(req, "File uploaded successfully", -1);
  return ESP_OK;
}

esp_err_t WebDAVServer::handle_delete(httpd_req_t *req) {
  auto *server = static_cast<WebDAVServer *>(req->user_ctx);

  std::string filepath = server->uri_to_filepath(req->uri);
  ESP_LOGD(TAG, "DELETE request for: %s", filepath.c_str());

  if (remove(filepath.c_str()) == 0) {
    httpd_resp_send(req, "File deleted successfully", -1);
  } else {
    httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Cannot delete file");
  }

  return ESP_OK;
}

esp_err_t WebDAVServer::handle_propfind(httpd_req_t *req) {
  auto *server = static_cast<WebDAVServer *>(req->user_ctx);

  std::string filepath = server->uri_to_filepath(req->uri);
  ESP_LOGD(TAG, "PROPFIND request for: %s", filepath.c_str());

  struct stat file_stat;
  if (stat(filepath.c_str(), &file_stat) != 0) {
    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Not found");
    return ESP_OK;
  }

  // Build WebDAV propfind response
  std::string response = "<?xml version=\"1.0\" encoding=\"utf-8\"?>";
  response += "<D:multistatus xmlns:D=\"DAV:\">";
  response += "<D:response>";
  response += "<D:href>" + std::string(req->uri) + "</D:href>";
  response += "<D:propstat>";
  response += "<D:prop>";
  response += "<D:resourcetype>";
  if (S_ISDIR(file_stat.st_mode)) {
    response += "<D:collection/>";
  }
  response += "</D:resourcetype>";
  response += "<D:getcontentlength>" + std::to_string(file_stat.st_size) + "</D:getcontentlength>";
  response += "</D:prop>";
  response += "<D:status>HTTP/1.1 200 OK</D:status>";
  response += "</D:propstat>";
  response += "</D:response>";
  response += "</D:multistatus>";

  httpd_resp_set_type(req, "application/xml");
  httpd_resp_send(req, response.c_str(), response.length());

  return ESP_OK;
}

esp_err_t WebDAVServer::handle_mkcol(httpd_req_t *req) {
  auto *server = static_cast<WebDAVServer *>(req->user_ctx);

  std::string filepath = server->uri_to_filepath(req->uri);
  ESP_LOGD(TAG, "MKCOL request for: %s", filepath.c_str());

  if (mkdir(filepath.c_str(), 0755) == 0) {
    httpd_resp_send(req, "Collection created", -1);
  } else {
    httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Cannot create collection");
  }

  return ESP_OK;
}

esp_err_t WebDAVServer::handle_move(httpd_req_t *req) {
  auto *server = static_cast<WebDAVServer *>(req->user_ctx);

  std::string filepath = server->uri_to_filepath(req->uri);

  char dest_buf[512];
  if (httpd_req_get_hdr_value_str(req, "Destination", dest_buf, sizeof(dest_buf)) != ESP_OK) {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Destination header required");
    return ESP_OK;
  }

  std::string dest_filepath = server->uri_to_filepath(dest_buf);

  if (rename(filepath.c_str(), dest_filepath.c_str()) == 0) {
    httpd_resp_send(req, "Resource moved", -1);
  } else {
    httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Cannot move resource");
  }

  return ESP_OK;
}

esp_err_t WebDAVServer::handle_copy(httpd_req_t *req) {
  auto *server = static_cast<WebDAVServer *>(req->user_ctx);

  std::string filepath = server->uri_to_filepath(req->uri);

  char dest_buf[512];
  if (httpd_req_get_hdr_value_str(req, "Destination", dest_buf, sizeof(dest_buf)) != ESP_OK) {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Destination header required");
    return ESP_OK;
  }

  std::string dest_filepath = server->uri_to_filepath(dest_buf);

  // Simple file copy
  std::ifstream src(filepath, std::ios::binary);
  std::ofstream dst(dest_filepath, std::ios::binary);

  if (src.is_open() && dst.is_open()) {
    dst << src.rdbuf();
    src.close();
    dst.close();
    httpd_resp_send(req, "Resource copied", -1);
  } else {
    httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Cannot copy resource");
  }

  return ESP_OK;
}

}  // namespace webdav_server
}  // namespace esphome
