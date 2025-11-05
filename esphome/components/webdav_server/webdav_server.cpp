#include "webdav_server.h"
#include "esphome/core/log.h"
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <dirent.h>
#include <algorithm>
#include <ctime>
#include <cstring>

namespace esphome {
namespace webdav_server {

void WebDAVServer::setup() {
  ESP_LOGE(TAG, "=== WEBDAV SETUP CALLED === port=%d, root=%s", this->port_, this->root_path_.c_str());
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
  // Stop existing server if running
  if (this->server_ != nullptr) {
    ESP_LOGW(TAG, "Server already running, stopping it first");
    httpd_stop(this->server_);
    this->server_ = nullptr;
  }

  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = this->port_;
  config.ctrl_port = this->port_ + 1000;
  config.max_uri_handlers = 20;
  config.stack_size = 8192;
  config.recv_wait_timeout = 60;
  config.send_wait_timeout = 60;
  config.lru_purge_enable = true;
  config.max_resp_headers = 32;
  config.max_open_sockets = 7;
  config.uri_match_fn = httpd_uri_match_wildcard;  // CRITICAL: Enable wildcard URI matching for "/*" patterns

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
  // Decode URL first
  std::string decoded_uri = url_decode(uri);
  ESP_LOGD(TAG, "URI: %s -> Decoded: %s", uri.c_str(), decoded_uri.c_str());

  // Remove URL prefix from URI to get relative path
  std::string relative_path = decoded_uri;

  if (decoded_uri.find(this->url_prefix_) == 0) {
    relative_path = decoded_uri.substr(this->url_prefix_.length());
  }

  // Remove leading slash if present
  if (!relative_path.empty() && relative_path[0] == '/') {
    relative_path = relative_path.substr(1);
  }

  // Handle empty path (root access)
  if (relative_path.empty()) {
    std::string root = this->root_path_;
    // Remove trailing slash for root
    if (!root.empty() && root.back() == '/') {
      root = root.substr(0, root.length() - 1);
    }
    ESP_LOGD(TAG, "Root access: %s", root.c_str());
    return root;
  }

  // Construct full path
  std::string full_path = this->root_path_;
  if (!this->root_path_.empty() && this->root_path_.back() != '/') {
    full_path += "/";
  }
  full_path += relative_path;

  ESP_LOGD(TAG, "Mapped URI %s to path %s", uri.c_str(), full_path.c_str());

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

std::string WebDAVServer::url_decode(const std::string &src) {
  std::string result;
  result.reserve(src.length());

  const char *str = src.c_str();
  int i = 0;
  char ch;
  int j;

  while (str[i]) {
    if (str[i] == '%' && str[i + 1] && str[i + 2]) {
      if (sscanf(str + i + 1, "%2x", &j) == 1) {
        ch = static_cast<char>(j);
        result += ch;
        i += 3;
      } else {
        result += str[i++];
      }
    } else if (str[i] == '+') {
      result += ' ';
      i++;
    } else {
      result += str[i++];
    }
  }

  return result;
}

std::string WebDAVServer::generate_prop_xml(const std::string &href, bool is_directory, time_t modified,
                                            size_t size) {
  char time_buf[50];
  struct tm *gmt = gmtime(&modified);
  strftime(time_buf, sizeof(time_buf), "%a, %d %b %Y %H:%M:%S GMT", gmt);

  std::string display_name = href;
  if (href.back() == '/') {
    display_name = href.substr(0, href.length() - 1);
  }
  size_t last_slash = display_name.find_last_of('/');
  if (last_slash != std::string::npos) {
    display_name = display_name.substr(last_slash + 1);
  }
  if (display_name.empty() && href == "/") {
    display_name = "Root";
  }

  std::string xml = "  <D:response>\n";
  xml += "    <D:href>" + href + "</D:href>\n";
  xml += "    <D:propstat>\n";
  xml += "      <D:prop>\n";
  xml += "        <D:resourcetype>";
  if (is_directory) {
    xml += "<D:collection/>";
  }
  xml += "</D:resourcetype>\n";
  xml += "        <D:getlastmodified>" + std::string(time_buf) + "</D:getlastmodified>\n";
  xml += "        <D:creationdate>" + std::string(time_buf) + "</D:creationdate>\n";
  xml += "        <D:displayname>" + display_name + "</D:displayname>\n";

  if (!is_directory) {
    xml += "        <D:getcontentlength>" + std::to_string(size) + "</D:getcontentlength>\n";

    std::string content_type = "application/octet-stream";
    size_t dot_pos = href.find_last_of(".");
    if (dot_pos != std::string::npos) {
      std::string ext = href.substr(dot_pos + 1);
      std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });

      if (ext == "txt")
        content_type = "text/plain";
      else if (ext == "html" || ext == "htm")
        content_type = "text/html";
      else if (ext == "css")
        content_type = "text/css";
      else if (ext == "js")
        content_type = "application/javascript";
      else if (ext == "jpg" || ext == "jpeg")
        content_type = "image/jpeg";
      else if (ext == "png")
        content_type = "image/png";
      else if (ext == "gif")
        content_type = "image/gif";
      else if (ext == "mp3")
        content_type = "audio/mpeg";
      else if (ext == "mp4")
        content_type = "video/mp4";
      else if (ext == "pdf")
        content_type = "application/pdf";
      else if (ext == "flac")
        content_type = "audio/flac";
    }

    xml += "        <D:getcontenttype>" + content_type + "</D:getcontenttype>\n";
  }

  xml += "      </D:prop>\n";
  xml += "      <D:status>HTTP/1.1 200 OK</D:status>\n";
  xml += "    </D:propstat>\n";
  xml += "  </D:response>\n";

  return xml;
}

std::vector<std::string> WebDAVServer::list_dir(const std::string &path) {
  std::vector<std::string> files;
  DIR *dir = opendir(path.c_str());
  if (dir != nullptr) {
    struct dirent *entry;
    while ((entry = readdir(dir)) != nullptr) {
      if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..")) {
        files.push_back(entry->d_name);
      }
    }
    closedir(dir);
  } else {
    ESP_LOGE(TAG, "Cannot open directory: %s (errno: %d)", path.c_str(), errno);
  }
  return files;
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
  ESP_LOGI(TAG, "PROPFIND on %s (URI: %s)", filepath.c_str(), req->uri);

  struct stat st;
  if (stat(filepath.c_str(), &st) != 0) {
    ESP_LOGE(TAG, "Path not found: %s (errno: %d)", filepath.c_str(), errno);
    return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Not Found");
  }

  // Log directory contents for debugging
  if (S_ISDIR(st.st_mode)) {
    DIR *dir = opendir(filepath.c_str());
    if (dir) {
      ESP_LOGI(TAG, "Directory contents of %s:", filepath.c_str());
      struct dirent *entry;
      while ((entry = readdir(dir)) != nullptr) {
        if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..")) {
          ESP_LOGI(TAG, "  - %s", entry->d_name);
        }
      }
      closedir(dir);
    }
  }

  bool is_directory = S_ISDIR(st.st_mode);
  std::string depth_header = "0";

  // Read Depth header
  char depth_value[10] = {0};
  if (httpd_req_get_hdr_value_str(req, "Depth", depth_value, sizeof(depth_value)) == ESP_OK) {
    depth_header = depth_value;
    ESP_LOGI(TAG, "Depth header: %s", depth_header.c_str());
  }

  // Build XML response
  std::string response = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";
  response += "<D:multistatus xmlns:D=\"DAV:\">\n";

  // Format URI for response
  std::string uri_path = req->uri;
  if (uri_path.empty() || uri_path == "/") {
    uri_path = "/";
  }
  if (is_directory && uri_path.back() != '/') {
    uri_path += '/';
  }

  ESP_LOGI(TAG, "Formatted URI for response: %s", uri_path.c_str());

  // Add properties for requested resource
  response += server->generate_prop_xml(uri_path, is_directory, st.st_mtime, st.st_size);

  // If directory and depth > 0, list contents
  if (is_directory && (depth_header == "1" || depth_header == "infinity")) {
    auto files = server->list_dir(filepath);
    ESP_LOGI(TAG, "Found %d files/folders in %s", files.size(), filepath.c_str());

    for (const auto &file_name : files) {
      std::string file_path = filepath;
      if (file_path.back() != '/') {
        file_path += '/';
      }
      file_path += file_name;

      struct stat file_stat;
      if (stat(file_path.c_str(), &file_stat) == 0) {
        bool is_file_dir = S_ISDIR(file_stat.st_mode);
        std::string href = uri_path;
        if (href.back() != '/') {
          href += '/';
        }
        href += file_name;
        if (is_file_dir) {
          href += '/';
        }

        ESP_LOGI(TAG, "Adding %s to PROPFIND response (is_dir: %d)", href.c_str(), is_file_dir);
        response += server->generate_prop_xml(href, is_file_dir, file_stat.st_mtime, file_stat.st_size);
      } else {
        ESP_LOGE(TAG, "Cannot stat %s (errno: %d)", file_path.c_str(), errno);
      }
    }
  }

  response += "</D:multistatus>";

  // Set response headers
  httpd_resp_set_type(req, "application/xml; charset=utf-8");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, HEAD, PUT, OPTIONS, DELETE, PROPFIND");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Authorization, Depth, Content-Type");
  httpd_resp_set_status(req, "207 Multi-Status");

  ESP_LOGD(TAG, "XML response: %s", response.c_str());

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
