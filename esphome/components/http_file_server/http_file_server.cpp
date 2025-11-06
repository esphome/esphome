#include "http_file_server.h"
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
namespace http_file_server {

void HttpFileServer::setup() {
  ESP_LOGI(TAG, "Starting HTTP File Server on port %d with prefix: %s", this->port_, this->url_prefix_.c_str());
  ESP_LOGI(TAG, "Root path: %s", this->root_path_.c_str());
  ESP_LOGI(TAG, "Upload: %s, Download: %s, Delete: %s", this->upload_enabled_ ? "YES" : "NO",
           this->download_enabled_ ? "YES" : "NO", this->deletion_enabled_ ? "YES" : "NO");

  if (!this->start_server()) {
    ESP_LOGE(TAG, "Failed to start HTTP File Server");
    this->mark_failed();
  } else {
    ESP_LOGI(TAG, "HTTP File Server started successfully");
  }
}

void HttpFileServer::loop() {
  // HTTP server handles everything in background
}

void HttpFileServer::dump_config() {
  ESP_LOGCONFIG(TAG, "HTTP File Server:");
  ESP_LOGCONFIG(TAG, "  Root path: %s", this->root_path_.c_str());
  ESP_LOGCONFIG(TAG, "  URL prefix: %s", this->url_prefix_.c_str());
  ESP_LOGCONFIG(TAG, "  Port: %d", this->port_);
  ESP_LOGCONFIG(TAG, "  Auth enabled: %s", this->auth_enabled_ ? "YES" : "NO");
  ESP_LOGCONFIG(TAG, "  Upload enabled: %s", this->upload_enabled_ ? "YES" : "NO");
  ESP_LOGCONFIG(TAG, "  Download enabled: %s", this->download_enabled_ ? "YES" : "NO");
  ESP_LOGCONFIG(TAG, "  Deletion enabled: %s", this->deletion_enabled_ ? "YES" : "NO");
}

bool HttpFileServer::start_server() {
  // Stop existing server if running
  if (this->server_ != nullptr) {
    ESP_LOGW(TAG, "Server already running, stopping it first");
    httpd_stop(this->server_);
    this->server_ = nullptr;
  }

  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = this->port_;
  config.ctrl_port = this->port_ + 1000;
  config.max_uri_handlers = 10;
  config.stack_size = 8192;
  config.recv_wait_timeout = 60;
  config.send_wait_timeout = 60;
  config.lru_purge_enable = true;
  config.max_resp_headers = 16;
  config.max_open_sockets = 7;
  config.uri_match_fn = httpd_uri_match_wildcard;  // Enable wildcard URI matching

  esp_err_t ret = httpd_start(&this->server_, &config);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(ret));
    return false;
  }

  // Build URI pattern for this file server
  std::string uri_pattern = this->url_prefix_;
  if (!uri_pattern.empty() && uri_pattern.back() != '/') {
    uri_pattern += '/';
  }
  uri_pattern += "*";  // Match all paths under prefix

  // Register HTTP handlers
  httpd_uri_t get_handler = {
      .uri = uri_pattern.c_str(),
      .method = HTTP_GET,
      .handler = HttpFileServer::handle_get,
      .user_ctx = this,
  };

  httpd_uri_t post_handler = {
      .uri = uri_pattern.c_str(),
      .method = HTTP_POST,
      .handler = HttpFileServer::handle_post,
      .user_ctx = this,
  };

  httpd_uri_t delete_handler = {
      .uri = uri_pattern.c_str(),
      .method = HTTP_DELETE,
      .handler = HttpFileServer::handle_delete,
      .user_ctx = this,
  };

  esp_err_t err;

  err = httpd_register_uri_handler(this->server_, &get_handler);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to register GET handler: %s", esp_err_to_name(err));
    httpd_stop(this->server_);
    return false;
  }
  ESP_LOGD(TAG, "Registered GET handler for %s", uri_pattern.c_str());

  err = httpd_register_uri_handler(this->server_, &post_handler);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to register POST handler: %s", esp_err_to_name(err));
    httpd_stop(this->server_);
    return false;
  }
  ESP_LOGD(TAG, "Registered POST handler for %s", uri_pattern.c_str());

  err = httpd_register_uri_handler(this->server_, &delete_handler);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to register DELETE handler: %s", esp_err_to_name(err));
    httpd_stop(this->server_);
    return false;
  }
  ESP_LOGD(TAG, "Registered DELETE handler for %s", uri_pattern.c_str());

  ESP_LOGI(TAG, "All handlers registered successfully");
  return true;
}

void HttpFileServer::stop_server() {
  if (this->server_ != nullptr) {
    httpd_stop(this->server_);
    this->server_ = nullptr;
  }
}

std::string HttpFileServer::uri_to_filepath(const std::string &uri) {
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

bool HttpFileServer::authenticate(const std::string &auth_header) {
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

std::string HttpFileServer::url_decode(const std::string &src) {
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

std::string HttpFileServer::extract_path_from_url(const std::string &url) {
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

esphome::FixedVector<FileInfo> HttpFileServer::list_directory(const std::string &path) {
  esphome::FixedVector<FileInfo> files;
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
        FileInfo info;
        info.name = entry->d_name;
        info.path = Path::join(path, entry->d_name);

        struct stat file_stat;
        if (stat(info.path.c_str(), &file_stat) == 0) {
          info.is_directory = S_ISDIR(file_stat.st_mode);
          info.size = file_stat.st_size;
          info.modified = file_stat.st_mtime;
        } else {
          info.is_directory = false;
          info.size = 0;
          info.modified = 0;
        }

        files.push_back(info);
      }
    }
    closedir(dir);
  } else {
    ESP_LOGE(TAG, "Cannot open directory: %s (errno: %d)", path.c_str(), errno);
  }
  return files;
}

std::string HttpFileServer::generate_html_header(const std::string &title) {
  std::string html = R"(<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1, user-scalable=no">
  <title>)";
  html += title;
  html += R"(</title>
  <style>
    body {
      font-family: 'Segoe UI', system-ui, sans-serif;
      margin: 0;
      padding: 2rem;
      background: #f5f5f7;
      color: #1d1d1f;
    }
    h1 {
      color: #0066cc;
      margin-bottom: 1.5rem;
      display: flex;
      align-items: center;
      gap: 1rem;
    }
    .container {
      max-width: 1200px;
      margin: 0 auto;
      background: white;
      border-radius: 12px;
      box-shadow: 0 4px 12px rgba(0, 0, 0, 0.1);
      padding: 2rem;
    }
    table {
      width: 100%;
      border-collapse: collapse;
      margin-top: 1.5rem;
    }
    th, td {
      padding: 12px;
      text-align: left;
      border-bottom: 1px solid #e0e0e0;
    }
    th {
      background: #f8f9fa;
      font-weight: 500;
    }
    .file-actions {
      display: flex;
      gap: 8px;
    }
    button {
      padding: 6px 12px;
      border: none;
      border-radius: 6px;
      background: #0066cc;
      color: white;
      cursor: pointer;
      transition: background 0.2s;
    }
    button:hover {
      background: #0052a3;
    }
    button.delete {
      background: #dc3545;
    }
    button.delete:hover {
      background: #c82333;
    }
    .upload-form {
      margin-bottom: 2rem;
      padding: 1rem;
      background: #f8f9fa;
      border-radius: 8px;
    }
    .upload-form input[type="file"] {
      margin-right: 1rem;
    }
    .breadcrumb {
      margin-bottom: 1.5rem;
      font-size: 0.9rem;
      color: #666;
    }
    .breadcrumb a {
      color: #0066cc;
      text-decoration: none;
    }
    .breadcrumb a:hover {
      text-decoration: underline;
    }
    .breadcrumb span:not(:last-child)::after {
      display: inline-block;
      margin: 0 .25rem;
      content: ">";
    }
    .folder {
      color: #0066cc;
      font-weight: 500;
    }
    .file-type {
      color: #666;
      font-size: 0.9rem;
    }
    .header-actions {
      display: flex;
      align-items: center;
      justify-content: space-between;
      margin-bottom: 1rem;
    }
    .header-actions button {
      background: #4CAF50;
    }
    .header-actions button:hover {
      background: #45a049;
    }
  </style>
</head>
<body>
<div class="container">
)";
  return html;
}

std::string HttpFileServer::generate_html_footer() {
  return R"(
</div>
<script>
function delete_file(path) {
  if (confirm('Are you sure you want to delete this file?')) {
    fetch(path, {method: 'DELETE'})
      .then(response => {
        if (response.ok) {
          location.reload();
        } else {
          alert('Failed to delete file');
        }
      })
      .catch(error => {
        alert('Error: ' + error);
      });
  }
}
function download_file(path, filename) {
  fetch(path)
    .then(response => response.blob())
    .then(blob => {
      const link = document.createElement('a');
      link.href = URL.createObjectURL(blob);
      link.download = filename;
      link.click();
    })
    .catch(error => {
      alert('Error: ' + error);
    });
}
</script>
</body>
</html>
)";
}

std::string HttpFileServer::generate_breadcrumb(const std::string &current_path) {
  std::string breadcrumb = R"(<div class="breadcrumb">)";
  breadcrumb += "<span><a href=\"" + this->url_prefix_ + "\">Home</a></span>";

  std::string relative_path = Path::remove_root_path(current_path, this->root_path_);
  std::vector<std::string> parts = Path::split_path(relative_path);

  std::string accumulated_path = this->url_prefix_;
  for (const std::string &part : parts) {
    if (!part.empty()) {
      accumulated_path = Path::join(accumulated_path, part);
      breadcrumb += "<span><a href=\"" + accumulated_path + "\">" + part + "</a></span>";
    }
  }

  breadcrumb += "</div>";
  return breadcrumb;
}

std::string HttpFileServer::generate_file_row(const FileInfo &info, const std::string &uri_prefix) {
  std::string row = "<tr><td>";

  std::string relative_path = Path::remove_root_path(info.path, this->root_path_);
  std::string file_uri = Path::join(uri_prefix, relative_path);

  if (info.is_directory) {
    row += "<a href=\"" + file_uri + "\" class=\"folder\">" + info.name + "</a>";
  } else {
    row += info.name;
  }

  row += "</td><td>";

  if (info.is_directory) {
    row += "Folder";
  } else {
    row += "<span class=\"file-type\">" + Path::file_type(info.name) + "</span>";
  }

  row += "</td><td>";

  if (!info.is_directory) {
    // Format file size
    if (info.size < 1024) {
      row += std::to_string(info.size) + " B";
    } else if (info.size < 1024 * 1024) {
      row += std::to_string(info.size / 1024) + " KB";
    } else {
      row += std::to_string(info.size / (1024 * 1024)) + " MB";
    }
  }

  row += "</td><td><div class=\"file-actions\">";

  if (!info.is_directory) {
    if (this->download_enabled_) {
      row += "<button onclick=\"download_file('" + file_uri + "', '" + info.name + "')\">Download</button>";
    }
    if (this->deletion_enabled_) {
      row += "<button class=\"delete\" onclick=\"delete_file('" + file_uri + "')\">Delete</button>";
    }
  }

  row += "</div></td></tr>";

  return row;
}

esp_err_t HttpFileServer::handle_get(httpd_req_t *req) {
  auto *server = static_cast<HttpFileServer *>(req->user_ctx);

  std::string filepath = server->uri_to_filepath(req->uri);
  ESP_LOGD(TAG, "GET request for: %s (mapped to: %s)", req->uri, filepath.c_str());

  struct stat file_stat;

  // Special handling for virtual root
  bool is_virtual_root = (server->root_path_ == "/" && filepath == "/");

  if (!is_virtual_root && stat(filepath.c_str(), &file_stat) != 0) {
    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File or directory not found");
    return ESP_OK;
  }

  if (is_virtual_root || S_ISDIR(file_stat.st_mode)) {
    return server->handle_directory_listing(req, filepath);
  } else {
    return server->handle_file_download(req, filepath);
  }
}

esp_err_t HttpFileServer::handle_directory_listing(httpd_req_t *req, const std::string &filepath) {
  bool is_virtual_root = (this->root_path_ == "/" && filepath == "/");

  // Generate HTML
  std::string html = this->generate_html_header("File Browser");

  html += "<div class=\"header-actions\">";
  html += "<h1>File Browser</h1>";
  html += "</div>";

  html += this->generate_breadcrumb(filepath);

  // Upload form
  if (this->upload_enabled_) {
    html += R"(<div class="upload-form">
      <form method="POST" enctype="multipart/form-data">
        <input type="file" name="file" required>
        <button type="submit">Upload</button>
      </form>
    </div>)";
  }

  // File table
  html += R"(<table>
    <thead>
      <tr>
        <th>Name</th>
        <th>Type</th>
        <th>Size</th>
        <th>Actions</th>
      </tr>
    </thead>
    <tbody>)";

  if (is_virtual_root) {
    // List mount points from storage_host
    const auto &mounts = this->storage_host_->get_mounts();
    ESP_LOGI(TAG, "Virtual root - listing %d mount points", mounts.size());

    for (const auto &mount : mounts) {
      FileInfo info;
      info.name = mount.path.substr(1);  // Remove leading slash
      if (info.name.empty())
        info.name = "root";
      info.path = mount.path;
      info.is_directory = true;
      info.size = 0;
      info.modified = 0;

      html += this->generate_file_row(info, this->url_prefix_);
    }
  } else {
    // Regular directory listing
    auto files = this->list_directory(filepath);
    ESP_LOGI(TAG, "Found %d files/folders in %s", files.size(), filepath.c_str());

    for (const auto &file : files) {
      html += this->generate_file_row(file, this->url_prefix_);
    }
  }

  html += "</tbody></table>";
  html += this->generate_html_footer();

  // Send response
  httpd_resp_set_type(req, "text/html");
  httpd_resp_send(req, html.c_str(), html.length());

  return ESP_OK;
}

esp_err_t HttpFileServer::handle_file_download(httpd_req_t *req, const std::string &filepath) {
  if (!this->download_enabled_) {
    httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "File download is disabled");
    return ESP_OK;
  }

  std::ifstream file(filepath, std::ios::binary);
  if (!file.is_open()) {
    httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Cannot open file");
    return ESP_OK;
  }

  // Get file size
  file.seekg(0, std::ios::end);
  size_t file_size = file.tellg();
  file.seekg(0, std::ios::beg);

  // Set content type based on file extension
  std::string mime_type = Path::mime_type(filepath);
  httpd_resp_set_type(req, mime_type.c_str());

  // Set content disposition
  std::string filename = Path::file_name(filepath);
  std::string content_disposition = "attachment; filename=\"" + filename + "\"";
  httpd_resp_set_hdr(req, "Content-Disposition", content_disposition.c_str());

  // Send file in chunks
  auto buffer = std::make_unique<char[]>(FILE_BUFFER_SIZE);
  while (file.read(buffer.get(), FILE_BUFFER_SIZE) || file.gcount() > 0) {
    httpd_resp_send_chunk(req, buffer.get(), file.gcount());
  }
  httpd_resp_send_chunk(req, nullptr, 0);  // End chunked response

  file.close();
  return ESP_OK;
}

esp_err_t HttpFileServer::handle_post(httpd_req_t *req) {
  auto *server = static_cast<HttpFileServer *>(req->user_ctx);

  if (!server->upload_enabled_) {
    httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "File upload is disabled");
    return ESP_OK;
  }

  std::string filepath = server->uri_to_filepath(req->uri);
  ESP_LOGD(TAG, "POST request for: %s", filepath.c_str());

  // Check if target is a directory
  struct stat file_stat;
  if (stat(filepath.c_str(), &file_stat) != 0 || !S_ISDIR(file_stat.st_mode)) {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Upload target must be a directory");
    return ESP_OK;
  }

  // Parse multipart form data
  // This is a simplified implementation - full multipart parsing would be more complex
  char *buf = new char[req->content_len + 1];
  int ret = httpd_req_recv(req, buf, req->content_len);
  if (ret <= 0) {
    delete[] buf;
    if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
      httpd_resp_send_err(req, HTTPD_408_REQ_TIMEOUT, "Request timeout");
    }
    return ESP_OK;
  }
  buf[ret] = '\0';

  // For simplicity, we'll just save the received data
  // In a full implementation, you'd parse the multipart boundary and extract the file
  std::string upload_path = Path::join(filepath, "uploaded_file");
  std::ofstream outfile(upload_path, std::ios::binary);
  if (!outfile.is_open()) {
    delete[] buf;
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to create file");
    return ESP_OK;
  }

  outfile.write(buf, ret);
  outfile.close();
  delete[] buf;

  // Redirect back to directory listing
  httpd_resp_set_status(req, "303 See Other");
  httpd_resp_set_hdr(req, "Location", req->uri);
  httpd_resp_send(req, nullptr, 0);

  return ESP_OK;
}

esp_err_t HttpFileServer::handle_delete(httpd_req_t *req) {
  auto *server = static_cast<HttpFileServer *>(req->user_ctx);

  if (!server->deletion_enabled_) {
    httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "File deletion is disabled");
    return ESP_OK;
  }

  std::string filepath = server->uri_to_filepath(req->uri);
  ESP_LOGD(TAG, "DELETE request for: %s", filepath.c_str());

  // Check if it's a directory
  struct stat file_stat;
  if (stat(filepath.c_str(), &file_stat) == 0 && S_ISDIR(file_stat.st_mode)) {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Cannot delete directories");
    return ESP_OK;
  }

  if (remove(filepath.c_str()) == 0) {
    httpd_resp_set_status(req, "204 No Content");
    httpd_resp_send(req, nullptr, 0);
  } else {
    httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Cannot delete file");
  }

  return ESP_OK;
}

// Path utility implementations
std::string Path::file_name(const std::string &path) {
  size_t pos = path.rfind(separator);
  if (pos != std::string::npos) {
    return path.substr(pos + 1);
  }
  return path;
}

bool Path::is_absolute(const std::string &path) { return !path.empty() && path[0] == separator; }

bool Path::has_trailing_slash(const std::string &path) { return !path.empty() && path.back() == separator; }

std::string Path::join(const std::string &first, const std::string &second) {
  if (first.empty())
    return second;
  if (second.empty())
    return first;

  std::string result = first;
  if (!has_trailing_slash(first) && !is_absolute(second)) {
    result.push_back(separator);
  }
  if (has_trailing_slash(first) && is_absolute(second)) {
    result.pop_back();
  }
  result.append(second);
  return result;
}

std::string Path::remove_root_path(const std::string &path, const std::string &root) {
  if (path.find(root) != 0)
    return path;
  if (path.size() == root.size())
    return "/";
  if (path.size() < root.size())
    return "/";

  std::string result = path.substr(root.size());
  if (result.empty() || result[0] != '/')
    result = "/" + result;
  return result;
}

std::vector<std::string> Path::split_path(const std::string &path) {
  std::vector<std::string> parts;
  std::string current_path = path;
  size_t pos = 0;

  while ((pos = current_path.find(separator)) != std::string::npos) {
    std::string part = current_path.substr(0, pos);
    if (!part.empty()) {
      parts.push_back(part);
    }
    current_path.erase(0, pos + 1);
  }

  if (!current_path.empty()) {
    parts.push_back(current_path);
  }

  return parts;
}

std::string Path::extension(const std::string &file) {
  size_t pos = file.find_last_of('.');
  if (pos == std::string::npos || pos == file.size() - 1)
    return "";
  return file.substr(pos + 1);
}

std::string Path::file_type(const std::string &file) {
  static const std::array<std::pair<const char *, const char *>, 20> file_types = {{
      {"mp3", "Audio (MP3)"},
      {"wav", "Audio (WAV)"},
      {"flac", "Audio (FLAC)"},
      {"png", "Image (PNG)"},
      {"jpg", "Image (JPG)"},
      {"jpeg", "Image (JPEG)"},
      {"gif", "Image (GIF)"},
      {"bmp", "Image (BMP)"},
      {"txt", "Text (TXT)"},
      {"log", "Text (LOG)"},
      {"csv", "Text (CSV)"},
      {"html", "Web (HTML)"},
      {"css", "Web (CSS)"},
      {"js", "Web (JS)"},
      {"json", "Data (JSON)"},
      {"xml", "Data (XML)"},
      {"zip", "Archive (ZIP)"},
      {"gz", "Archive (GZ)"},
      {"tar", "Archive (TAR)"},
      {"mp4", "Video (MP4)"},
  }};

  std::string ext = extension(file);
  if (ext.empty())
    return "File";

  // Convert to lowercase
  std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });

  for (const auto &[file_ext, file_type_str] : file_types) {
    if (ext == file_ext)
      return file_type_str;
  }

  return "File (" + ext + ")";
}

std::string Path::mime_type(const std::string &file) {
  static const std::array<std::pair<const char *, const char *>, 25> mime_types = {{
      {"mp3", "audio/mpeg"},
      {"wav", "audio/wav"},
      {"flac", "audio/flac"},
      {"png", "image/png"},
      {"jpg", "image/jpeg"},
      {"jpeg", "image/jpeg"},
      {"gif", "image/gif"},
      {"bmp", "image/bmp"},
      {"txt", "text/plain"},
      {"log", "text/plain"},
      {"csv", "text/csv"},
      {"html", "text/html"},
      {"htm", "text/html"},
      {"css", "text/css"},
      {"js", "application/javascript"},
      {"json", "application/json"},
      {"xml", "application/xml"},
      {"pdf", "application/pdf"},
      {"zip", "application/zip"},
      {"gz", "application/gzip"},
      {"tar", "application/x-tar"},
      {"mp4", "video/mp4"},
      {"avi", "video/x-msvideo"},
      {"webm", "video/webm"},
      {"mkv", "video/x-matroska"},
  }};

  std::string ext = extension(file);
  if (ext.empty())
    return "application/octet-stream";

  // Convert to lowercase
  std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });

  for (const auto &[file_ext, mime] : mime_types) {
    if (ext == file_ext)
      return mime;
  }

  return "application/octet-stream";
}

}  // namespace http_file_server
}  // namespace esphome
