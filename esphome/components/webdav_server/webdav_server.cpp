#include "webdav_server.h"
#include "esphome/components/storage_host/storage_host.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <dirent.h>
#include <algorithm>
#include <ctime>
#include <cstring>
#include <cerrno>

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
  httpd_uri_t options_handler = {
      .uri = "/*",
      .method = HTTP_OPTIONS,
      .handler = WebDAVServer::handle_options,
      .user_ctx = this,
  };

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

  httpd_uri_t status_handler = {
      .uri = "/status",
      .method = HTTP_GET,
      .handler = WebDAVServer::handle_status,
      .user_ctx = this,
  };

  esp_err_t err;

  // Register specific routes BEFORE wildcard routes!
  // Status endpoint must be registered before GET wildcard
  err = httpd_register_uri_handler(this->server_, &status_handler);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to register STATUS handler: %s", esp_err_to_name(err));
    httpd_stop(this->server_);
    return false;
  }
  ESP_LOGD(TAG, "Registered STATUS handler");

  err = httpd_register_uri_handler(this->server_, &options_handler);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to register OPTIONS handler: %s", esp_err_to_name(err));
    httpd_stop(this->server_);
    return false;
  }
  ESP_LOGD(TAG, "Registered OPTIONS handler");

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

  // Handle empty path (root access) - return root_path as-is
  if (relative_path.empty()) {
    ESP_LOGD(TAG, "Root access: %s", this->root_path_.c_str());
    return this->root_path_;
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

std::string WebDAVServer::extract_path_from_url(const std::string &url) {
  // Check if it's a full URL (starts with http:// or https://)
  if (url.find("http://") == 0 || url.find("https://") == 0) {
    // Find the third slash (after protocol://host:port/)
    size_t first_slash = url.find("//");
    if (first_slash != std::string::npos) {
      size_t path_start = url.find('/', first_slash + 2);
      if (path_start != std::string::npos) {
        std::string path = url.substr(path_start);
        ESP_LOGD(TAG, "Extracted path from URL: %s -> %s", url.c_str(), path.c_str());
        return path;
      }
    }
  }
  // Not a URL or parsing failed - return as-is
  return url;
}

std::string WebDAVServer::generate_prop_xml(const std::string &href, bool is_directory, time_t modified, size_t size) {
  char time_buf[50];
  struct tm *gmt = gmtime(&modified);
  strftime(time_buf, sizeof(time_buf), "%a, %d %b %Y %H:%M:%S GMT", gmt);

  // Extract display name efficiently
  std::string_view href_view(href);
  if (!href_view.empty() && href_view.back() == '/') {
    href_view.remove_suffix(1);
  }
  size_t last_slash = href_view.find_last_of('/');
  std::string_view display_name = (last_slash != std::string_view::npos) ? href_view.substr(last_slash + 1) : href_view;
  if (display_name.empty() && href == "/") {
    display_name = "Root";
  }

  // Pre-allocate string with estimated size to reduce allocations
  std::string xml;
  xml.reserve(512);  // Typical XML response is 300-500 bytes

  xml.append("  <D:response>\n    <D:href>");
  xml.append(href);
  xml.append("</D:href>\n    <D:propstat>\n      <D:prop>\n        <D:resourcetype>");
  if (is_directory) {
    xml.append("<D:collection/>");
  }
  xml.append("</D:resourcetype>\n        <D:getlastmodified>");
  xml.append(time_buf);
  xml.append("</D:getlastmodified>\n        <D:creationdate>");
  xml.append(time_buf);
  xml.append("</D:creationdate>\n        <D:displayname>");
  xml.append(display_name);
  xml.append("</D:displayname>\n");

  if (!is_directory) {
    xml.append("        <D:getcontentlength>");
    xml.append(std::to_string(size));
    xml.append("</D:getcontentlength>\n");

    // Determine content type efficiently using string_view
    const char *content_type = "application/octet-stream";
    size_t dot_pos = href.find_last_of('.');
    if (dot_pos != std::string::npos && dot_pos + 1 < href.length()) {
      std::string_view ext_view(&href[dot_pos + 1]);

      // Fast case-insensitive comparison using first character
      char first_lower = std::tolower(static_cast<unsigned char>(ext_view[0]));

      if (first_lower == 't') {
        if (ext_view == "txt" || ext_view == "TXT")
          content_type = "text/plain";
      } else if (first_lower == 'h') {
        if (ext_view == "html" || ext_view == "HTML" || ext_view == "htm" || ext_view == "HTM")
          content_type = "text/html";
      } else if (first_lower == 'c') {
        if (ext_view == "css" || ext_view == "CSS")
          content_type = "text/css";
      } else if (first_lower == 'j') {
        if (ext_view == "js" || ext_view == "JS")
          content_type = "application/javascript";
        else if (ext_view == "jpg" || ext_view == "JPG" || ext_view == "jpeg" || ext_view == "JPEG")
          content_type = "image/jpeg";
      } else if (first_lower == 'p') {
        if (ext_view == "png" || ext_view == "PNG")
          content_type = "image/png";
        else if (ext_view == "pdf" || ext_view == "PDF")
          content_type = "application/pdf";
      } else if (first_lower == 'g') {
        if (ext_view == "gif" || ext_view == "GIF")
          content_type = "image/gif";
      } else if (first_lower == 'm') {
        if (ext_view == "mp3" || ext_view == "MP3")
          content_type = "audio/mpeg";
        else if (ext_view == "mp4" || ext_view == "MP4")
          content_type = "video/mp4";
      } else if (first_lower == 'f') {
        if (ext_view == "flac" || ext_view == "FLAC")
          content_type = "audio/flac";
      }
    }

    xml.append("        <D:getcontenttype>");
    xml.append(content_type);
    xml.append("</D:getcontenttype>\n");
  }

  xml.append("      </D:prop>\n      <D:status>HTTP/1.1 200 OK</D:status>\n    </D:propstat>\n  </D:response>\n");

  return xml;
}

esphome::FixedVector<std::string> WebDAVServer::list_dir(const std::string &path) {
  esphome::FixedVector<std::string> files;
  DIR *dir = opendir(path.c_str());
  if (dir != nullptr) {
    // Count entries first to allocate once
    size_t count = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != nullptr) {
      if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..")) {
        count++;
        if (count >= MAX_DIR_ENTRIES) {
          ESP_LOGW(TAG, "Directory %s has more than %zu entries, truncating", path.c_str(), MAX_DIR_ENTRIES);
          break;
        }
      }
    }

    // Allocate exact size needed (single allocation)
    files.init(count);

    // Rewind and fill
    rewinddir(dir);
    while ((entry = readdir(dir)) != nullptr && files.size() < count) {
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

esp_err_t WebDAVServer::handle_options(httpd_req_t *req) {
  ESP_LOGD(TAG, "OPTIONS request for: %s", req->uri);

  // Set WebDAV capability headers
  httpd_resp_set_hdr(req, "DAV", "1, 2");
  httpd_resp_set_hdr(req, "Allow", "OPTIONS, GET, HEAD, PUT, DELETE, PROPFIND, PROPPATCH, MKCOL, COPY, MOVE");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Methods",
                     "OPTIONS, GET, HEAD, PUT, DELETE, PROPFIND, PROPPATCH, MKCOL, COPY, MOVE");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Authorization, Depth, Content-Type, Destination, Overwrite");
  httpd_resp_set_hdr(req, "MS-Author-Via", "DAV");

  httpd_resp_send(req, nullptr, 0);
  return ESP_OK;
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

    // Use architecture-specific buffer size
    auto buffer = std::make_unique<char[]>(FILE_BUFFER_SIZE);
    while (file.read(buffer.get(), FILE_BUFFER_SIZE)) {
      httpd_resp_send_chunk(req, buffer.get(), file.gcount());
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

  // Use architecture-specific buffer size
  auto buffer = std::make_unique<char[]>(FILE_BUFFER_SIZE);
  int received;
  while ((received = httpd_req_recv(req, buffer.get(), FILE_BUFFER_SIZE)) > 0) {
    file.write(buffer.get(), received);
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

  // Special handling for virtual root "/" when root_path is "/"
  bool is_virtual_root = (server->root_path_ == "/" && filepath == "/");
  bool is_directory = false;
  struct stat st;

  if (is_virtual_root) {
    // Virtual root - list mount points from storage_host
    ESP_LOGI(TAG, "Virtual root access - listing mount points from storage_host");
    is_directory = true;
    // Set dummy stat data for root directory
    st.st_mode = S_IFDIR | 0755;
    st.st_mtime = time(nullptr);
    st.st_size = 0;
  } else {
    // Regular file/directory access
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

    is_directory = S_ISDIR(st.st_mode);
  }
  std::string depth_header = "0";

  // Read Depth header
  char depth_value[DEPTH_BUFFER_SIZE] = {0};
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
    if (is_virtual_root) {
      // Virtual root - list mount points from storage_host
      const auto &mounts = server->storage_host_->get_mounts();
      ESP_LOGI(TAG, "Found %d mount points", mounts.size());

      for (const auto &mount : mounts) {
        // mount.path = mount_path (e.g., "/usb0")
        // mount.platform = platform (e.g., "usb_msc")
        std::string mount_path = mount.path;
        std::string mount_name = mount_path;

        // Remove leading slash for href construction
        if (!mount_name.empty() && mount_name.front() == '/') {
          mount_name = mount_name.substr(1);
        }

        // Build href
        std::string href = uri_path;
        if (!href.empty() && href.back() != '/') {
          href += '/';
        }
        href += mount_name;
        if (!href.empty() && href.back() != '/') {
          href += '/';
        }

        ESP_LOGI(TAG, "Adding mount point %s (%s) to PROPFIND response", href.c_str(), mount.platform.c_str());
        response += server->generate_prop_xml(href, true, st.st_mtime, 0);
      }
    } else {
      // Regular directory listing
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

// Operation tracking functions
std::string WebDAVServer::create_operation_id() {
  std::string op_id = "op_" + std::to_string(this->next_operation_id_++);
  return op_id;
}

void WebDAVServer::update_operation_status(const std::string &op_id, OperationStatus status, const std::string &error) {
  for (auto &op : this->operations_) {
    if (op.operation_id == op_id) {
      op.status = status;
      if (!error.empty()) {
        op.error_message = error;
      }
      ESP_LOGD(TAG, "Operation %s status updated to %d", op_id.c_str(), static_cast<int>(status));
      return;
    }
  }
}

void WebDAVServer::update_operation_progress(const std::string &op_id, size_t bytes_processed) {
  for (auto &op : this->operations_) {
    if (op.operation_id == op_id) {
      op.bytes_processed = bytes_processed;
      return;
    }
  }
}

OperationState *WebDAVServer::get_operation_status(const std::string &op_id) {
  for (auto &op : this->operations_) {
    if (op.operation_id == op_id) {
      return &op;
    }
  }
  return nullptr;
}

void WebDAVServer::cleanup_old_operations() {
  uint32_t now = millis();
  // Remove operations older than 5 minutes
  this->operations_.erase(std::remove_if(this->operations_.begin(), this->operations_.end(),
                                         [now](const OperationState &op) {
                                           return op.status != OperationStatus::IN_PROGRESS &&
                                                  (now - op.start_time) > 300000;
                                         }),
                          this->operations_.end());
}

// Helper function: Perform file copy operation (used by both sync and async paths)
bool WebDAVServer::perform_file_copy(const std::string &src_path, const std::string &dst_path, off_t file_size,
                                     const std::string &operation_id, httpd_req_t *req) {
  FILE *src = fopen(src_path.c_str(), "rb");
  if (!src) {
    ESP_LOGE(TAG, "Failed to open source file: %s (errno: %d, %s)", src_path.c_str(), errno, strerror(errno));
    return false;
  }

  FILE *dst = fopen(dst_path.c_str(), "wb");
  if (!dst) {
    ESP_LOGE(TAG, "Failed to open destination file: %s (errno: %d, %s)", dst_path.c_str(), errno, strerror(errno));
    fclose(src);
    return false;
  }

  // If req is provided, start chunked response for keepalive
  bool send_keepalive = (req != nullptr);
  if (send_keepalive) {
    httpd_resp_set_status(req, "201 Created");
    httpd_resp_set_type(req, "text/plain");
    // Don't call httpd_resp_send - we'll use httpd_resp_send_chunk for chunked transfer
  }

  auto buffer = std::make_unique<char[]>(FILE_BUFFER_SIZE);
  size_t bytes_read;
  size_t total_copied = 0;
  bool copy_success = true;
  bool read_error = false;
  uint32_t chunk_counter = 0;

  ESP_LOGI(TAG, "Starting file copy: %s -> %s (size: %lld bytes)", src_path.c_str(), dst_path.c_str(),
           (long long) file_size);

  while ((bytes_read = fread(buffer.get(), 1, FILE_BUFFER_SIZE, src)) > 0) {
    size_t bytes_written = fwrite(buffer.get(), 1, bytes_read, dst);
    if (bytes_written != bytes_read) {
      ESP_LOGE(TAG, "Write failed at offset %zu (errno: %d, %s)", total_copied, errno, strerror(errno));
      copy_success = false;
      break;
    }
    total_copied += bytes_written;

    // Update operation progress if tracking
    if (!operation_id.empty()) {
      update_operation_progress(operation_id, total_copied);
    }

    // Send HTTP keepalive chunk every 64 buffer reads (~256KB for 4KB buffers, ~1MB for 16KB buffers)
    chunk_counter++;
    if (send_keepalive && chunk_counter % 64 == 0) {
      int progress_percent = (file_size > 0) ? (int) ((total_copied * 100) / file_size) : 0;
      char progress_msg[64];
      snprintf(progress_msg, sizeof(progress_msg), "Progress: %d%% (%zu/%lld bytes)\n", progress_percent, total_copied,
               (long long) file_size);

      if (httpd_resp_send_chunk(req, progress_msg, strlen(progress_msg)) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to send keepalive chunk - client may have disconnected");
        // Continue copying even if client disconnects
      }

      ESP_LOGD(TAG, "Sent keepalive: %s", progress_msg);
    }

    // Log progress for large files
    if (total_copied % (FILE_BUFFER_SIZE * 64) == 0) {
      ESP_LOGD(TAG, "Copy progress: %zu / %lld bytes (%.1f%%)", total_copied, (long long) file_size,
               (total_copied * 100.0) / file_size);
    }
  }

  // Check for read errors
  if (ferror(src)) {
    ESP_LOGE(TAG, "Read error at offset %zu (errno: %d, %s)", total_copied, errno, strerror(errno));
    read_error = true;
    copy_success = false;
  }

  // Flush and close files
  if (fflush(dst) != 0) {
    ESP_LOGE(TAG, "Flush failed (errno: %d, %s)", errno, strerror(errno));
    copy_success = false;
  }

  int close_src_result = fclose(src);
  int close_dst_result = fclose(dst);

  if (close_src_result != 0) {
    ESP_LOGE(TAG, "Failed to close source file (errno: %d, %s)", errno, strerror(errno));
  }
  if (close_dst_result != 0) {
    ESP_LOGE(TAG, "Failed to close destination file (errno: %d, %s)", errno, strerror(errno));
    copy_success = false;
  }

  if (copy_success && !read_error && total_copied == static_cast<size_t>(file_size)) {
    ESP_LOGI(TAG, "File copy completed successfully: %zu bytes", total_copied);

    // Send final completion chunk if using keepalive
    if (send_keepalive) {
      const char *completion_msg = "Copy completed successfully\n";
      httpd_resp_send_chunk(req, completion_msg, strlen(completion_msg));
      // Send empty chunk to signal end of chunked response
      httpd_resp_send_chunk(req, nullptr, 0);
    }

    return true;
  } else {
    ESP_LOGE(TAG, "File copy failed: %s to %s (copied %zu of %lld bytes, read_error=%d)", src_path.c_str(),
             dst_path.c_str(), total_copied, (long long) file_size, read_error);

    // Send failure chunk if using keepalive
    if (send_keepalive) {
      const char *failure_msg = "Copy failed\n";
      httpd_resp_send_chunk(req, failure_msg, strlen(failure_msg));
      // Send empty chunk to signal end of chunked response
      httpd_resp_send_chunk(req, nullptr, 0);
    }

    // Clean up partial file
    if (remove(dst_path.c_str()) != 0) {
      ESP_LOGE(TAG, "Failed to remove partial destination file (errno: %d, %s)", errno, strerror(errno));
    }
    return false;
  }
}

// Helper function: Perform file move operation (copy + delete source)
bool WebDAVServer::perform_file_move(const std::string &src_path, const std::string &dst_path, off_t file_size,
                                     const std::string &operation_id, httpd_req_t *req) {
  if (perform_file_copy(src_path, dst_path, file_size, operation_id, req)) {
    if (remove(src_path.c_str()) == 0) {
      ESP_LOGI(TAG, "File move completed successfully");
      return true;
    } else {
      ESP_LOGE(TAG, "Failed to delete source after copy: %s (errno: %d, %s)", src_path.c_str(), errno, strerror(errno));
      // File was copied but source remains
      return true;  // Still consider it success since copy worked
    }
  }
  return false;
}

#ifdef USE_ESP32
// FreeRTOS task: Async file copy
void WebDAVServer::copy_task(void *params) {
  auto *task_params = static_cast<CopyTaskParams *>(params);

  ESP_LOGI(TAG, "Async copy task started [%s]: %s -> %s", task_params->operation_id.c_str(),
           task_params->src_path.c_str(), task_params->dst_path.c_str());

  // Perform the copy operation
  bool success = task_params->server->perform_file_copy(task_params->src_path, task_params->dst_path,
                                                        task_params->file_size, task_params->operation_id);

  // Update final status
  task_params->server->update_operation_status(task_params->operation_id,
                                               success ? OperationStatus::COMPLETED : OperationStatus::FAILED,
                                               success ? "" : "Copy operation failed");

  // Clean up
  delete task_params;

  // Delete this task
  vTaskDelete(nullptr);
}

// FreeRTOS task: Async file move
void WebDAVServer::move_task(void *params) {
  auto *task_params = static_cast<MoveTaskParams *>(params);

  ESP_LOGI(TAG, "Async move task started [%s]: %s -> %s", task_params->operation_id.c_str(),
           task_params->src_path.c_str(), task_params->dst_path.c_str());

  // Perform the move operation
  bool success = task_params->server->perform_file_move(task_params->src_path, task_params->dst_path,
                                                        task_params->file_size, task_params->operation_id);

  // Update final status
  task_params->server->update_operation_status(task_params->operation_id,
                                               success ? OperationStatus::COMPLETED : OperationStatus::FAILED,
                                               success ? "" : "Move operation failed");

  // Clean up
  delete task_params;

  // Delete this task
  vTaskDelete(nullptr);
}
#endif

esp_err_t WebDAVServer::handle_move(httpd_req_t *req) {
  auto *server = static_cast<WebDAVServer *>(req->user_ctx);

  std::string filepath = server->uri_to_filepath(req->uri);
  ESP_LOGD(TAG, "MOVE request for: %s", filepath.c_str());

  char dest_buf[DEST_BUFFER_SIZE];
  if (httpd_req_get_hdr_value_str(req, "Destination", dest_buf, sizeof(dest_buf)) != ESP_OK) {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Destination header required");
    return ESP_OK;
  }

  // Extract path from full URL if needed
  std::string dest_path = server->extract_path_from_url(dest_buf);
  std::string dest_filepath = server->uri_to_filepath(dest_path);

  ESP_LOGD(TAG, "MOVE destination: %s", dest_filepath.c_str());

  // Check if source exists
  struct stat src_stat;
  if (stat(filepath.c_str(), &src_stat) != 0) {
    ESP_LOGE(TAG, "Source not found: %s", filepath.c_str());
    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Source not found");
    return ESP_OK;
  }

  // Try atomic rename first (works within same filesystem)
  int rename_result = rename(filepath.c_str(), dest_filepath.c_str());
  int rename_errno = errno;  // Save errno immediately

  ESP_LOGD(TAG, "rename() result: %d, errno: %d (%s)", rename_result, rename_errno, strerror(rename_errno));

  if (rename_result == 0) {
    ESP_LOGI(TAG, "Move completed (atomic rename)");
    httpd_resp_set_status(req, "201 Created");
    httpd_resp_send(req, "Resource moved", -1);
    return ESP_OK;
  }

  // If rename fails with EXDEV (cross-device), fall back to copy+delete
  ESP_LOGI(TAG, "rename() failed with errno=%d (EXDEV=%d)", rename_errno, EXDEV);
  if (rename_errno == EXDEV) {
    ESP_LOGI(TAG, "Cross-mount move detected, using copy+delete fallback");

    if (S_ISDIR(src_stat.st_mode)) {
      ESP_LOGE(TAG, "Cross-mount directory move not supported: %s", filepath.c_str());
      httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Cross-mount directory move not supported");
      return ESP_OK;
    }

    // Perform synchronous cross-mount move with keepalive (HTTP server already handles multi-client via tasks)
    // Pass req to enable chunked progress updates during copy+delete
    if (server->perform_file_move(filepath, dest_filepath, src_stat.st_size, "", req)) {
      // Response already sent via chunked transfer in perform_file_copy
      // No need to send response here
    } else {
      // Only send error if not already handled by chunked transfer
      // perform_file_copy sends final chunk on both success and failure
    }
  } else {
    // Other rename error
    ESP_LOGE(TAG, "Failed to move %s to %s (errno: %d, %s)", filepath.c_str(), dest_filepath.c_str(), rename_errno,
             strerror(rename_errno));
    httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Cannot move resource");
  }

  return ESP_OK;
}

esp_err_t WebDAVServer::handle_copy(httpd_req_t *req) {
  auto *server = static_cast<WebDAVServer *>(req->user_ctx);

  std::string filepath = server->uri_to_filepath(req->uri);
  ESP_LOGD(TAG, "COPY request for: %s", filepath.c_str());

  char dest_buf[DEST_BUFFER_SIZE];
  if (httpd_req_get_hdr_value_str(req, "Destination", dest_buf, sizeof(dest_buf)) != ESP_OK) {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Destination header required");
    return ESP_OK;
  }

  // Extract path from full URL if needed
  std::string dest_path = server->extract_path_from_url(dest_buf);
  std::string dest_filepath = server->uri_to_filepath(dest_path);

  ESP_LOGD(TAG, "COPY destination: %s", dest_filepath.c_str());

  // Check if source exists and is a file
  struct stat src_stat;
  if (stat(filepath.c_str(), &src_stat) != 0) {
    ESP_LOGE(TAG, "Source file not found: %s", filepath.c_str());
    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Source not found");
    return ESP_OK;
  }

  if (S_ISDIR(src_stat.st_mode)) {
    ESP_LOGE(TAG, "Directory copy not supported: %s", filepath.c_str());
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Directory copy not supported");
    return ESP_OK;
  }

  // Perform synchronous copy with keepalive (HTTP server already handles multi-client via tasks)
  // Pass req to enable chunked progress updates during copy
  if (server->perform_file_copy(filepath, dest_filepath, src_stat.st_size, "", req)) {
    // Response already sent via chunked transfer in perform_file_copy
    // No need to send response here
  } else {
    // Only send error if not already handled by chunked transfer
    // perform_file_copy sends final chunk on both success and failure
  }

  return ESP_OK;
}

esp_err_t WebDAVServer::handle_status(httpd_req_t *req) {
  auto *server = static_cast<WebDAVServer *>(req->user_ctx);

  // Extract operation_id from query parameter
  char query_buf[128];
  if (httpd_req_get_url_query_str(req, query_buf, sizeof(query_buf)) != ESP_OK) {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing query parameters");
    return ESP_OK;
  }

  // Parse op_id from query string
  char op_id_buf[64];
  if (httpd_query_key_value(query_buf, "op_id", op_id_buf, sizeof(op_id_buf)) != ESP_OK) {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing op_id parameter");
    return ESP_OK;
  }

  std::string op_id = op_id_buf;
  ESP_LOGD(TAG, "Status request for operation: %s", op_id.c_str());

  // Get operation status
  OperationState *op_state = server->get_operation_status(op_id);
  if (op_state == nullptr) {
    ESP_LOGW(TAG, "Operation not found: %s", op_id.c_str());
    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Operation not found");
    return ESP_OK;
  }

  // Build JSON response
  std::string status_str;
  switch (op_state->status) {
    case OperationStatus::IN_PROGRESS:
      status_str = "in_progress";
      break;
    case OperationStatus::COMPLETED:
      status_str = "completed";
      break;
    case OperationStatus::FAILED:
      status_str = "failed";
      break;
  }

  // Calculate progress percentage
  int progress_percent = 0;
  if (op_state->file_size > 0) {
    progress_percent = (int) ((op_state->bytes_processed * 100) / op_state->file_size);
  }

  // Calculate elapsed time
  uint32_t elapsed_ms = millis() - op_state->start_time;

  // Build JSON response
  std::string json = "{";
  json += "\"operation_id\":\"" + op_state->operation_id + "\",";
  json += "\"operation_type\":\"" + op_state->operation_type + "\",";
  json += "\"status\":\"" + status_str + "\",";
  json += "\"src_path\":\"" + op_state->src_path + "\",";
  json += "\"dst_path\":\"" + op_state->dst_path + "\",";
  json += "\"file_size\":" + std::to_string(op_state->file_size) + ",";
  json += "\"bytes_processed\":" + std::to_string(op_state->bytes_processed) + ",";
  json += "\"progress_percent\":" + std::to_string(progress_percent) + ",";
  json += "\"elapsed_ms\":" + std::to_string(elapsed_ms);

  if (!op_state->error_message.empty()) {
    json += ",\"error\":\"" + op_state->error_message + "\"";
  }

  json += "}";

  ESP_LOGD(TAG, "Status response: %s", json.c_str());

  // Send JSON response
  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, json.c_str(), json.length());

  return ESP_OK;
}

}  // namespace webdav_server
}  // namespace esphome
