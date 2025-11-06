#include "http_file_server.h"
#include "esphome/components/storage_host/storage_host.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esphome/core/application.h"
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
  ESP_LOGI(TAG, "Setting up HTTP File Server with prefix: %s", this->url_prefix_.c_str());
  ESP_LOGI(TAG, "Root path: %s", this->root_path_.c_str());
  ESP_LOGI(TAG, "Upload: %s, Download: %s, Delete: %s", this->upload_enabled_ ? "YES" : "NO",
           this->download_enabled_ ? "YES" : "NO", this->deletion_enabled_ ? "YES" : "NO");

  // Register directly with AsyncWebServer to bypass web_server_base's auth middleware
  // This allows http_file_server to have its own independent authentication
  auto server = this->base_->get_server();
  if (server) {
    server->addHandler(this);
    ESP_LOGI(TAG, "HTTP File Server registered successfully (bypassing base auth)");
  } else {
    ESP_LOGE(TAG, "Failed to get web server instance");
  }
}

void HttpFileServer::dump_config() {
  ESP_LOGCONFIG(TAG, "HTTP File Server:");
  ESP_LOGCONFIG(TAG, "  Root path: %s", this->root_path_.c_str());
  ESP_LOGCONFIG(TAG, "  URL prefix: %s", this->url_prefix_.c_str());
  ESP_LOGCONFIG(TAG, "  Auth enabled: %s", this->auth_enabled_ ? "YES" : "NO");
  ESP_LOGCONFIG(TAG, "  Upload enabled: %s", this->upload_enabled_ ? "YES" : "NO");
  ESP_LOGCONFIG(TAG, "  Download enabled: %s", this->download_enabled_ ? "YES" : "NO");
  ESP_LOGCONFIG(TAG, "  Deletion enabled: %s", this->deletion_enabled_ ? "YES" : "NO");
}

// AsyncWebHandler interface implementation
bool HttpFileServer::canHandle(AsyncWebServerRequest *request) const {
  // Handle requests that start with our URL prefix
  std::string uri = request->url().c_str();
  const char *method_name = (request->method() == HTTP_GET) ? "GET" : (request->method() == HTTP_POST) ? "POST" : (request->method() == HTTP_DELETE) ? "DELETE" : "OTHER";

  ESP_LOGD(TAG, "canHandle called: %s %s (prefix: %s)", method_name, uri.c_str(), this->url_prefix_.c_str());

  // Check if URI starts with our prefix
  if (uri.find(this->url_prefix_) != 0) {
    ESP_LOGD(TAG, "  -> NO: URI doesn't start with prefix");
    return false;
  }

  // We handle GET, POST, and DELETE methods
  if (request->method() == HTTP_GET || request->method() == HTTP_POST || request->method() == HTTP_DELETE) {
    ESP_LOGI(TAG, "  -> YES: Will handle %s %s", method_name, uri.c_str());
    return true;
  }

  ESP_LOGW(TAG, "  -> NO: Unsupported method %s", method_name);
  return false;
}

void HttpFileServer::handleRequest(AsyncWebServerRequest *request) {
  // Check authentication if enabled
  if (this->auth_enabled_) {
    if (!request->authenticate(this->username_.c_str(), this->password_.c_str())) {
      return request->requestAuthentication();
    }
  }

  std::string uri = request->url().c_str();

  // Check for API endpoints
  if (uri.find(this->url_prefix_ + "/api/copy") == 0 && request->method() == HTTP_POST) {
    ESP_LOGD(TAG, "API COPY endpoint hit, body_buffer size: %zu", this->body_buffer_.size());
    this->handle_api_copy(request);
  } else if (uri.find(this->url_prefix_ + "/api/move") == 0 && request->method() == HTTP_POST) {
    ESP_LOGD(TAG, "API MOVE endpoint hit, body_buffer size: %zu", this->body_buffer_.size());
    this->handle_api_move(request);
  } else if (uri.find(this->url_prefix_ + "/api/rename") == 0 && request->method() == HTTP_POST) {
    ESP_LOGD(TAG, "API RENAME endpoint hit, body_buffer size: %zu", this->body_buffer_.size());
    this->handle_api_rename(request);
  } else if (uri.find(this->url_prefix_ + "/api/mkdir") == 0 && request->method() == HTTP_POST) {
    ESP_LOGD(TAG, "API MKDIR endpoint hit, body_buffer size: %zu", this->body_buffer_.size());
    this->handle_api_mkdir(request);
  } else if (uri.find(this->url_prefix_ + "/api/delete") == 0 && request->method() == HTTP_POST) {
    ESP_LOGD(TAG, "API DELETE endpoint hit, body_buffer size: %zu", this->body_buffer_.size());
    this->handle_api_delete(request);
  } else if (uri.find(this->url_prefix_ + "/api/mount") == 0 && request->method() == HTTP_POST) {
    ESP_LOGD(TAG, "API MOUNT endpoint hit, body_buffer size: %zu", this->body_buffer_.size());
    this->handle_api_mount(request);
  } else if (uri.find(this->url_prefix_ + "/api/unmount") == 0 && request->method() == HTTP_POST) {
    ESP_LOGD(TAG, "API UNMOUNT endpoint hit, body_buffer size: %zu", this->body_buffer_.size());
    this->handle_api_unmount(request);
  } else if (uri.find(this->url_prefix_ + "/api/exists") == 0 && request->method() == HTTP_GET) {
    this->handle_api_exists(request);
  } else if (uri.find(this->url_prefix_ + "/api/dirisempty") == 0 && request->method() == HTTP_GET) {
    this->handle_api_dirisempty(request);
  } else if (uri.find(this->url_prefix_ + "/api/dirinfo") == 0 && request->method() == HTTP_GET) {
    this->handle_api_dirinfo(request);
  } else if (uri.find(this->url_prefix_ + "/api/progress") == 0 && request->method() == HTTP_GET) {
    this->handle_api_progress(request);
  } else if (request->method() == HTTP_GET) {
    // Handle GET request (directory listing or file download)
    std::string filepath = this->uri_to_filepath(uri);
    ESP_LOGD(TAG, "GET request for: %s (mapped to: %s)", uri.c_str(), filepath.c_str());

    struct stat file_stat;
    bool is_virtual_root = (this->root_path_ == "/" && filepath == "/");

    if (!is_virtual_root && stat(filepath.c_str(), &file_stat) != 0) {
      request->send(404, "text/plain", "File or directory not found");
      return;
    }

    if (is_virtual_root || S_ISDIR(file_stat.st_mode)) {
      this->handle_directory_listing(request, filepath);
    } else {
      this->handle_file_download(request, filepath);
    }
  } else if (request->method() == HTTP_POST) {
    // Handle POST for file uploads (non-API endpoints)
    ESP_LOGD(TAG, "POST handler - upload for URI: %s", uri.c_str());

    // Check if this is a file upload (has filename query parameter)
    auto *filename_param = request->getParam("filename");
    if (filename_param) {
      this->handle_file_upload(request, filename_param->value().c_str());
    } else {
      request->send(400, "text/plain", "Missing filename parameter");
    }
  } else {
    request->send(405, "application/json", "{\"error\":\"Method not allowed\"}");
  }
}

void HttpFileServer::handleUpload(AsyncWebServerRequest *request, const PlatformString &filename, size_t index,
                                  uint8_t *data, size_t len, bool final) {
  // Check authentication if enabled
  if (this->auth_enabled_) {
    if (!request->authenticate(this->username_.c_str(), this->password_.c_str())) {
      return;
    }
  }

  if (!this->upload_enabled_) {
    if (final) {
      request->send(403, "text/plain", "File upload is disabled");
    }
    return;
  }

  // Get upload directory from request URL
  if (index == 0) {
    std::string uri = request->url().c_str();
    this->upload_directory_ = this->uri_to_filepath(uri);
    this->upload_filename_ = filename.c_str();

    // Check if target is a directory
    struct stat file_stat;
    if (stat(this->upload_directory_.c_str(), &file_stat) != 0 || !S_ISDIR(file_stat.st_mode)) {
      if (final) {
        request->send(400, "text/plain", "Upload target must be a directory");
      }
      return;
    }

    // Open file for writing
    std::string upload_path = Path::join(this->upload_directory_, this->upload_filename_);
    ESP_LOGI(TAG, "Starting upload: %s", upload_path.c_str());

    // Initialize progress tracking
    this->progress_.operation = "upload";
    this->progress_.source = filename.c_str();
    this->progress_.destination = upload_path;
    this->progress_.total_bytes = 0;  // We'll try to get this from Content-Length
    this->progress_.transferred_bytes = 0;
    this->progress_.in_progress = true;
    this->progress_.start_time = millis();

    // Get content length from request
    this->progress_.total_bytes = request->contentLength();

    this->upload_file_ = fopen(upload_path.c_str(), "wb");
    if (!this->upload_file_) {
      ESP_LOGE(TAG, "Failed to open file for upload: %s", upload_path.c_str());
      if (final) {
        request->send(500, "text/plain", "Failed to create file");
      }
      return;
    }
  }

  // Write data chunk
  if (this->upload_file_ && len > 0) {
    size_t written = fwrite(data, 1, len, this->upload_file_);
    if (written != len) {
      ESP_LOGE(TAG, "Failed to write upload data");
      fclose(this->upload_file_);
      this->upload_file_ = nullptr;
      this->progress_.in_progress = false;
      if (final) {
        request->send(500, "text/plain", "Failed to write file");
      }
      return;
    }

    // Update progress
    if (this->progress_.in_progress) {
      this->progress_.transferred_bytes += written;
    }
  }

  // Finalize upload
  if (final) {
    if (this->upload_file_) {
      fclose(this->upload_file_);
      this->upload_file_ = nullptr;
      ESP_LOGI(TAG, "Upload completed: %s", this->upload_filename_.c_str());

      // Mark progress as complete
      this->progress_.in_progress = false;

      request->send(201, "text/plain", "File uploaded successfully");
    } else {
      this->progress_.in_progress = false;
      request->send(500, "text/plain", "Upload failed");
    }
  }
}

void HttpFileServer::handleBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
  // Check authentication if enabled
  if (this->auth_enabled_) {
    if (!request->authenticate(this->username_.c_str(), this->password_.c_str())) {
      return;
    }
  }

  // Accumulate body data in buffer
  if (index == 0) {
    ESP_LOGD(TAG, "handleBody called: total=%zu bytes", total);
    this->body_buffer_.clear();
    this->body_buffer_.reserve(total);
  }

  this->body_buffer_.append(reinterpret_cast<char *>(data), len);
  ESP_LOGV(TAG, "handleBody: appended %zu bytes (total now: %zu/%zu)", len, this->body_buffer_.size(), total);
}

// Helper methods
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

esphome::FixedVector<FileInfo> HttpFileServer::list_directory(const std::string &path) {
  esphome::FixedVector<FileInfo> files;

  ESP_LOGD(TAG, "Attempting to open directory: %s", path.c_str());
  DIR *dir = opendir(path.c_str());

  if (dir != nullptr) {
    ESP_LOGV(TAG, "Directory opened successfully: %s", path.c_str());
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

// HTML generation helpers
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
    .progress-modal {
      display: none;
      position: fixed;
      top: 0;
      left: 0;
      width: 100%;
      height: 100%;
      background: rgba(0, 0, 0, 0.5);
      z-index: 1000;
      align-items: center;
      justify-content: center;
    }
    .progress-modal.active {
      display: flex;
    }
    .progress-content {
      background: white;
      padding: 2rem;
      border-radius: 12px;
      min-width: 400px;
      box-shadow: 0 8px 24px rgba(0, 0, 0, 0.2);
    }
    .progress-title {
      font-size: 1.2rem;
      font-weight: 500;
      margin-bottom: 1rem;
      color: #1d1d1f;
    }
    .progress-bar-container {
      width: 100%;
      height: 20px;
      background: #e0e0e0;
      border-radius: 10px;
      overflow: hidden;
      margin: 1rem 0;
    }
    .progress-bar {
      height: 100%;
      background: linear-gradient(90deg, #0066cc, #0052a3);
      width: 0%;
      transition: width 0.3s ease;
      display: flex;
      align-items: center;
      justify-content: center;
      color: white;
      font-size: 0.8rem;
      font-weight: 500;
    }
    .progress-details {
      color: #666;
      font-size: 0.9rem;
      margin-top: 0.5rem;
    }
    .progress-file-info {
      margin-top: 1rem;
      padding: 1rem;
      background: #f8f9fa;
      border-radius: 8px;
      font-size: 0.85rem;
      word-break: break-all;
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
<div id="progressModal" class="progress-modal">
  <div class="progress-content">
    <div class="progress-title" id="progressTitle">Processing...</div>
    <div class="progress-bar-container">
      <div class="progress-bar" id="progressBar">0%</div>
    </div>
    <div class="progress-details" id="progressDetails">Initializing...</div>
    <div class="progress-file-info">
      <div><strong>From:</strong> <span id="progressSource">-</span></div>
      <div><strong>To:</strong> <span id="progressDest">-</span></div>
    </div>
  </div>
</div>
<script>
// API base path for this file server instance
const API_BASE = ')" + this->url_prefix_ + R"(';
let progressPollInterval = null;

function showProgressModal(operation, source, destination) {
  const modal = document.getElementById('progressModal');
  const title = document.getElementById('progressTitle');
  const bar = document.getElementById('progressBar');
  const details = document.getElementById('progressDetails');
  const sourceEl = document.getElementById('progressSource');
  const destEl = document.getElementById('progressDest');

  // Set title based on operation type
  if (operation === 'copy') {
    title.textContent = 'Copying File...';
  } else if (operation === 'move') {
    title.textContent = 'Moving File...';
  } else if (operation === 'upload') {
    title.textContent = 'Uploading File...';
  } else {
    title.textContent = 'Processing...';
  }

  bar.style.width = '0%';
  bar.textContent = '0%';
  details.textContent = 'Starting operation...';
  sourceEl.textContent = source;

  // Strip URL prefix from destination if present
  let cleanDest = destination;
  if (cleanDest.startsWith(API_BASE)) {
    cleanDest = cleanDest.substring(API_BASE.length);
  }
  destEl.textContent = cleanDest;

  modal.classList.add('active');
}

function hideProgressModal() {
  const modal = document.getElementById('progressModal');
  modal.classList.remove('active');
  if (progressPollInterval) {
    clearInterval(progressPollInterval);
    progressPollInterval = null;
  }
}

function formatBytes(bytes) {
  if (bytes === 0) return '0 B';
  const k = 1024;
  const sizes = ['B', 'KB', 'MB', 'GB'];
  const i = Math.floor(Math.log(bytes) / Math.log(k));
  return (bytes / Math.pow(k, i)).toFixed(1) + ' ' + sizes[i];
}

function formatTime(ms) {
  if (ms < 1000) return ms + 'ms';
  const seconds = Math.floor(ms / 1000);
  if (seconds < 60) return seconds + 's';
  const minutes = Math.floor(seconds / 60);
  const remainingSeconds = seconds % 60;
  return minutes + 'm ' + remainingSeconds + 's';
}

let progressPollCount = 0;
let hasSeenProgress = false;

function pollProgress() {
  progressPollCount++;

  fetch(API_BASE + '/api/progress')
    .then(response => response.json())
    .then(data => {
      console.log('Progress update #' + progressPollCount + ':', data);

      if (!data.in_progress) {
        // Only close if we've seen progress before, or after 10 polls
        if (hasSeenProgress || progressPollCount > 10) {
          console.log('Operation complete, closing modal');
          hideProgressModal();
          location.reload();
        } else {
          console.log('Waiting for operation to start...');
        }
        return;
      }

      // Mark that we've seen progress
      hasSeenProgress = true;

      const bar = document.getElementById('progressBar');
      const details = document.getElementById('progressDetails');

      const percentage = data.percentage || 0;
      bar.style.width = percentage + '%';
      bar.textContent = percentage.toFixed(1) + '%';

      let detailText = formatBytes(data.transferred_bytes) + ' / ' + formatBytes(data.total_bytes);
      if (data.elapsed_ms) {
        detailText += ' • Elapsed: ' + formatTime(data.elapsed_ms);
      }
      if (data.remaining_ms) {
        detailText += ' • Remaining: ' + formatTime(data.remaining_ms);
      }

      details.textContent = detailText;
      console.log('Updated progress bar:', percentage.toFixed(1) + '%');
    })
    .catch(error => {
      console.error('Progress polling error:', error);
    });
}

function startProgressPolling() {
  // Reset progress tracking
  progressPollCount = 0;
  hasSeenProgress = false;

  if (progressPollInterval) {
    clearInterval(progressPollInterval);
  }
  // Poll every 500ms
  progressPollInterval = setInterval(pollProgress, 500);
  // First poll after 250ms delay to give backend time to start tracking
  setTimeout(pollProgress, 250);
}

function delete_file(path) {
  if (confirm('Are you sure you want to delete this file?')) {
    fetch(API_BASE + '/api/delete', {
      method: 'POST',
      headers: {'Content-Type': 'application/x-www-form-urlencoded'},
      body: 'path=' + encodeURIComponent(path)
    })
      .then(response => {
        if (!response.ok) {
          return response.text().then(text => {
            throw new Error('HTTP ' + response.status + ': ' + text);
          });
        }
        return response.json();
      })
      .then(data => {
        if (data.success) {
          alert('File deleted successfully!');
          location.reload();
        } else {
          alert('Delete failed: ' + (data.error || 'Unknown error'));
        }
      })
      .catch(error => {
        alert('Error: ' + error);
      });
  }
}

async function delete_directory(path) {
  try {
    // First do lightweight check if directory is empty
    const emptyCheck = await fetch(API_BASE + '/api/dirisempty?path=' + encodeURIComponent(path));
    const emptyInfo = await emptyCheck.json();

    if (emptyInfo.error) {
      alert('Error checking directory: ' + emptyInfo.error);
      return;
    }

    let message;
    if (emptyInfo.is_empty) {
      // Fast path for empty directories
      message = 'Delete empty directory?';
    } else {
      // Only count contents if directory is not empty
      const response = await fetch(API_BASE + '/api/dirinfo?path=' + encodeURIComponent(path));
      const info = await response.json();

      if (info.error) {
        alert('Error checking directory contents: ' + info.error);
        return;
      }

      const dirName = path.split('/').pop();
      message = 'Delete directory "' + dirName + '" and all its contents?\n\n';
      message += 'This will delete:\n';
      if (info.file_count > 0) {
        message += '- ' + info.file_count + ' file' + (info.file_count > 1 ? 's' : '') + '\n';
      }
      if (info.dir_count > 0) {
        message += '- ' + info.dir_count + ' subdirector' + (info.dir_count > 1 ? 'ies' : 'y') + '\n';
      }
      message += '\nThis action cannot be undone!';
    }

    if (!confirm(message)) {
      return;
    }

    // Perform deletion using API endpoint
    fetch(API_BASE + '/api/delete', {
      method: 'POST',
      headers: {'Content-Type': 'application/x-www-form-urlencoded'},
      body: 'path=' + encodeURIComponent(path)
    })
      .then(response => {
        if (!response.ok) {
          return response.text().then(text => {
            throw new Error('HTTP ' + response.status + ': ' + text);
          });
        }
        return response.json();
      })
      .then(data => {
        if (data.success) {
          alert('Directory deleted successfully!');
          location.reload();
        } else {
          alert('Delete failed: ' + (data.error || 'Unknown error'));
        }
      })
      .catch(error => {
        alert('Error: ' + error);
      });
  } catch (error) {
    alert('Error: ' + error);
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

// Helper function to suggest a new filename with (1) appended before extension
function suggestNewFilename(filepath) {
  const lastSlashIndex = filepath.lastIndexOf('/');
  const path = filepath.substring(0, lastSlashIndex + 1);
  const filename = filepath.substring(lastSlashIndex + 1);

  const lastDotIndex = filename.lastIndexOf('.');
  if (lastDotIndex === -1) {
    // No extension
    return filepath + ' (1)';
  } else {
    const basename = filename.substring(0, lastDotIndex);
    const extension = filename.substring(lastDotIndex);
    return path + basename + ' (1)' + extension;
  }
}

// Helper function to check if a file exists
async function fileExists(filepath) {
  try {
    const response = await fetch(API_BASE + '/api/exists?path=' + encodeURIComponent(filepath));
    const data = await response.json();
    return data.exists;
  } catch (error) {
    console.error('Error checking file existence:', error);
    return false;
  }
}

// Helper function to handle file copy with existence check
async function performCopy(source, destination) {
  showProgressModal('copy', source, destination);
  startProgressPolling();

  fetch(API_BASE + '/api/copy', {
    method: 'POST',
    headers: {'Content-Type': 'application/x-www-form-urlencoded'},
    body: 'source=' + encodeURIComponent(source) + '&destination=' + encodeURIComponent(destination)
  })
    .then(response => response.json())
    .then(data => {
      if (!data.success) {
        hideProgressModal();
        alert('Copy failed: ' + (data.error || 'Unknown error'));
      }
    })
    .catch(error => {
      hideProgressModal();
      alert('Error: ' + error);
    });
}

async function copy_file(source) {
  let destination = prompt('Enter destination path:', source + '.copy');
  if (!destination) return;

  // Check if destination exists
  const exists = await fileExists(destination);
  if (exists) {
    const choice = confirm('File already exists. Click OK to overwrite, or Cancel to change the filename.');
    if (!choice) {
      // User wants to change filename - suggest new name with (1) appended
      const suggested = suggestNewFilename(destination);
      destination = prompt('Enter new destination path:', suggested);
      if (!destination) return;
    }
  }

  performCopy(source, destination);
}

// Helper function to handle file move with existence check
async function performMove(source, destination) {
  showProgressModal('move', source, destination);
  startProgressPolling();

  fetch(API_BASE + '/api/move', {
    method: 'POST',
    headers: {'Content-Type': 'application/x-www-form-urlencoded'},
    body: 'source=' + encodeURIComponent(source) + '&destination=' + encodeURIComponent(destination)
  })
    .then(response => response.json())
    .then(data => {
      if (!data.success) {
        hideProgressModal();
        alert('Move failed: ' + (data.error || 'Unknown error'));
      }
    })
    .catch(error => {
      hideProgressModal();
      alert('Error: ' + error);
    });
}

async function move_file(source) {
  let destination = prompt('Enter destination path:', source);
  if (!destination) return;

  // Check if destination exists
  const exists = await fileExists(destination);
  if (exists) {
    const choice = confirm('File already exists. Click OK to overwrite, or Cancel to change the filename.');
    if (!choice) {
      // User wants to change filename - suggest new name with (1) appended
      const suggested = suggestNewFilename(destination);
      destination = prompt('Enter new destination path:', suggested);
      if (!destination) return;
    }
  }

  performMove(source, destination);
}
function rename_file(source) {
  const currentName = source.split('/').pop();
  const newName = prompt('Enter new name:', currentName);
  if (!newName || newName === currentName) return;

  fetch(API_BASE + '/api/rename', {
    method: 'POST',
    headers: {'Content-Type': 'application/x-www-form-urlencoded'},
    body: 'source=' + encodeURIComponent(source) + '&name=' + encodeURIComponent(newName)
  })
    .then(response => response.json())
    .then(data => {
      if (data.success) {
        alert('File renamed successfully!');
        location.reload();
      } else {
        alert('Rename failed: ' + (data.error || 'Unknown error'));
      }
    })
    .catch(error => alert('Error: ' + error));
}
function create_directory() {
  const name = prompt('Enter directory name:');
  if (!name || !name.trim()) return;

  // Create directory in current location
  let currentPath = window.location.pathname;
  if (currentPath.endsWith('/')) {
    currentPath = currentPath.slice(0, -1);
  }
  const fullPath = currentPath + '/' + name.trim();

  fetch(API_BASE + '/api/mkdir', {
    method: 'POST',
    headers: {'Content-Type': 'application/x-www-form-urlencoded'},
    body: 'name=' + encodeURIComponent(fullPath)
  })
    .then(response => response.json())
    .then(data => {
      if (data.success) {
        alert('Directory created successfully!');
        location.reload();
      } else {
        alert('Create directory failed: ' + (data.error || 'Unknown error'));
      }
    })
    .catch(error => alert('Error: ' + error));
}

function handleUpload(event) {
  event.preventDefault();

  const fileInput = document.getElementById('uploadFile');
  const file = fileInput.files[0];

  if (!file) {
    alert('Please select a file');
    return false;
  }

  // Show progress modal
  showProgressModal('upload', file.name, window.location.pathname);
  startProgressPolling();

  // Send file as raw binary with filename in URL query parameter
  let uploadUrl = window.location.pathname;
  if (!uploadUrl.endsWith('/')) {
    uploadUrl += '/';
  }
  uploadUrl += '?filename=' + encodeURIComponent(file.name);

  fetch(uploadUrl, {
    method: 'POST',
    headers: {
      'Content-Type': 'application/octet-stream'
    },
    body: file
  })
    .then(response => {
      if (!response.ok) {
        return response.text().then(text => {
          throw new Error('Upload failed: ' + text);
        });
      }
      return response.text();
    })
    .then(() => {
      // Upload complete - polling will detect and close modal
      setTimeout(() => {
        hideProgressModal();
        alert('File uploaded successfully!');
        location.reload();
      }, 500);
    })
    .catch(error => {
      hideProgressModal();
      alert('Error: ' + error);
    });

  return false;
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
    // Download button
    if (this->download_enabled_) {
      row += "<button onclick=\"download_file('" + file_uri + "', '" + info.name + "')\">Download</button>";
    }
    // Copy button
    row += "<button onclick=\"copy_file('" + info.path + "')\">Copy</button>";
    // Move button
    row += "<button onclick=\"move_file('" + info.path + "')\">Move</button>";
    // Rename button
    row += "<button onclick=\"rename_file('" + info.path + "')\">Rename</button>";
    // Delete button
    if (this->deletion_enabled_) {
      row += "<button class=\"delete\" onclick=\"delete_file('" + file_uri + "')\">Delete</button>";
    }
  } else {
    // Directory actions
    if (!info.is_mount_point) {
      // Rename button (not for mount points)
      row += "<button onclick=\"rename_file('" + info.path + "')\">Rename</button>";
      // Delete button (not for mount points)
      if (this->deletion_enabled_) {
        row += "<button class=\"delete\" onclick=\"delete_directory('" + file_uri + "')\">Delete</button>";
      }
    }
    // TODO: Add Mount/Unmount buttons for mount points
    // This requires integration with usb_msc_host and sd_mmc_card automation actions
  }

  row += "</div></td></tr>";

  return row;
}

// Request handlers
void HttpFileServer::handle_directory_listing(AsyncWebServerRequest *request, const std::string &filepath) {
  bool is_virtual_root = (this->root_path_ == "/" && filepath == "/");

  // Generate HTML
  std::string html = this->generate_html_header("File Browser");

  html += "<div class=\"header-actions\">";
  html += "<h1>File Browser</h1>";
  html += "<button onclick=\"create_directory()\">New Folder</button>";
  html += "</div>";

  html += this->generate_breadcrumb(filepath);

  // Upload form
  if (this->upload_enabled_) {
    html += R"(<div class="upload-form">
      <form id="uploadForm" onsubmit="return handleUpload(event);">
        <input type="file" name="file" id="uploadFile" required>
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
      info.is_mount_point = true;
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

  // Send response with cache-busting headers to ensure browser gets latest JS
  AsyncWebServerResponse *response = request->beginResponse(200, "text/html", html);
  response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  response->addHeader("Pragma", "no-cache");
  response->addHeader("Expires", "0");
  request->send(response);
}

void HttpFileServer::handle_file_download(AsyncWebServerRequest *request, const std::string &filepath) {
  if (!this->download_enabled_) {
    request->send(403, "text/plain", "File download is disabled");
    return;
  }

  // Open the file
  FILE *file = fopen(filepath.c_str(), "rb");
  if (!file) {
    request->send(404, "text/plain", "File not found");
    return;
  }

  // Get file size
  fseek(file, 0, SEEK_END);
  size_t file_size = ftell(file);
  fseek(file, 0, SEEK_SET);

  // Set content type based on file extension
  std::string mime_type = Path::mime_type(filepath);
  std::string filename = Path::file_name(filepath);

  ESP_LOGI(TAG, "Starting file download: %s (size: %zu bytes)", filename.c_str(), file_size);

  // Use AsyncWebServer's response but with a size limit for practical memory usage
  // For files > 16MB, we'd need a proper streaming solution outside AsyncWebServer
  const size_t CHUNK_READ_SIZE = 16 * 1024 * 1024;  // 16MB chunks

  if (file_size <= CHUNK_READ_SIZE) {
    // Small enough to read into memory at once
    std::string content;
    content.resize(file_size);
    size_t bytes_read = fread(content.data(), 1, file_size, file);
    fclose(file);

    if (bytes_read != file_size) {
      ESP_LOGE(TAG, "Failed to read file completely");
      request->send(500, "text/plain", "Failed to read file");
      return;
    }

    AsyncWebServerResponse *response = request->beginResponse(200, mime_type.c_str(), content);
    response->addHeader("Content-Disposition", ("attachment; filename=\"" + filename + "\"").c_str());
    request->send(response);
    ESP_LOGI(TAG, "Download sent: %zu bytes", bytes_read);
  } else {
    // File too large - would need proper streaming support
    fclose(file);
    ESP_LOGW(TAG, "File too large: %zu bytes (max %zu)", file_size, CHUNK_READ_SIZE);
    request->send(413, "text/plain", "File too large - maximum 16MB");
  }
}

void HttpFileServer::handle_file_upload(AsyncWebServerRequest *request, const std::string &filename) {
  if (!this->upload_enabled_) {
    request->send(403, "text/plain", "File upload is disabled");
    return;
  }

  // Get the directory path from URI
  std::string uri = request->url().c_str();
  std::string dir_path = this->uri_to_filepath(uri);

  // Check if target is a directory
  struct stat file_stat;
  if (stat(dir_path.c_str(), &file_stat) != 0 || !S_ISDIR(file_stat.st_mode)) {
    request->send(400, "text/plain", "Upload target must be a directory");
    return;
  }

  // Build full upload path
  std::string upload_path = Path::join(dir_path, filename);
  ESP_LOGI(TAG, "Starting upload: %s", upload_path.c_str());

  // Initialize progress tracking
  this->progress_.operation = "upload";
  this->progress_.source = filename.c_str();
  this->progress_.destination = upload_path;
  this->progress_.total_bytes = request->contentLength();
  this->progress_.transferred_bytes = 0;
  this->progress_.in_progress = true;
  this->progress_.start_time = millis();

  // Open file for writing
  FILE *file = fopen(upload_path.c_str(), "wb");
  if (!file) {
    ESP_LOGE(TAG, "Failed to open file for writing: %s", upload_path.c_str());
    this->progress_.in_progress = false;
    request->send(500, "text/plain", "Failed to open file for writing");
    return;
  }

  // Get raw httpd_req_t to read POST body
  httpd_req_t *req = static_cast<httpd_req_t *>(*request);
  size_t remaining = request->contentLength();
  const size_t BUFFER_SIZE = 2048;  // Smaller buffer to reduce stack usage
  uint8_t buffer[BUFFER_SIZE];
  bool success = true;

  ESP_LOGI(TAG, "Reading upload data: %zu bytes total", remaining);

  // Read and write in chunks
  while (remaining > 0) {
    // Feed the watchdog to prevent timeout on large uploads
    App.feed_wdt();

    size_t to_read = (remaining > BUFFER_SIZE) ? BUFFER_SIZE : remaining;
    int received = httpd_req_recv(req, reinterpret_cast<char *>(buffer), to_read);

    if (received <= 0) {
      ESP_LOGE(TAG, "Failed to receive data: %d (errno: %d)", received, errno);
      success = false;
      break;
    }

    size_t written = fwrite(buffer, 1, received, file);
    fflush(file);  // Ensure data is written immediately

    if (written != static_cast<size_t>(received)) {
      ESP_LOGE(TAG, "Failed to write to file: wrote %zu of %d bytes", written, received);
      success = false;
      break;
    }

    this->progress_.transferred_bytes += written;
    remaining -= received;

    // Log progress every ~50KB
    if (this->progress_.transferred_bytes % (50 * 1024) < BUFFER_SIZE) {
      ESP_LOGD(TAG, "Upload progress: %zu / %zu bytes (%.1f%%)", this->progress_.transferred_bytes,
               this->progress_.total_bytes,
               (float) this->progress_.transferred_bytes / this->progress_.total_bytes * 100.0f);
    }
  }

  fclose(file);

  // Mark progress as complete
  this->progress_.in_progress = false;

  if (success) {
    ESP_LOGI(TAG, "Upload complete: %zu bytes", this->progress_.transferred_bytes);
    request->send(200, "text/plain", "File uploaded successfully");
  } else {
    ESP_LOGE(TAG, "Upload failed");
    remove(upload_path.c_str());  // Clean up partial file
    request->send(500, "text/plain", "Upload failed");
  }
}

// Directory helpers
bool HttpFileServer::is_directory_empty(const std::string &path) {
  DIR *dir = opendir(path.c_str());
  if (!dir) {
    return true;  // Can't open, treat as empty
  }

  struct dirent *entry;
  while ((entry = readdir(dir)) != nullptr) {
    // Skip . and ..
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
      continue;
    }
    // Found at least one entry - not empty
    closedir(dir);
    return false;
  }

  closedir(dir);
  return true;  // No entries found - empty
}

void HttpFileServer::count_directory_contents(const std::string &path, int &file_count, int &dir_count) {
  DIR *dir = opendir(path.c_str());
  if (!dir) {
    return;
  }

  struct dirent *entry;
  while ((entry = readdir(dir)) != nullptr) {
    // Skip . and ..
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
      continue;
    }

    std::string entry_path = Path::join(path, entry->d_name);
    struct stat entry_stat;
    if (stat(entry_path.c_str(), &entry_stat) == 0) {
      if (S_ISDIR(entry_stat.st_mode)) {
        dir_count++;
        // Recursively count subdirectory contents
        this->count_directory_contents(entry_path, file_count, dir_count);
      } else {
        file_count++;
      }
    }
  }

  closedir(dir);
}

bool HttpFileServer::recursive_delete_directory(const std::string &path) {
  DIR *dir = opendir(path.c_str());
  if (!dir) {
    ESP_LOGE(TAG, "Cannot open directory for deletion: %s (errno: %d)", path.c_str(), errno);
    return false;
  }

  bool success = true;
  struct dirent *entry;
  while ((entry = readdir(dir)) != nullptr) {
    // Skip . and ..
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
      continue;
    }

    std::string entry_path = Path::join(path, entry->d_name);
    struct stat entry_stat;
    if (stat(entry_path.c_str(), &entry_stat) == 0) {
      if (S_ISDIR(entry_stat.st_mode)) {
        // Recursively delete subdirectory
        if (!this->recursive_delete_directory(entry_path)) {
          success = false;
          break;
        }
      } else {
        // Delete file
        if (remove(entry_path.c_str()) != 0) {
          ESP_LOGE(TAG, "Failed to delete file: %s (errno: %d, %s)", entry_path.c_str(), errno, strerror(errno));
          success = false;
          break;
        }
      }
    }
  }

  closedir(dir);

  // Delete the directory itself
  if (success) {
    if (rmdir(path.c_str()) != 0) {
      ESP_LOGE(TAG, "Failed to delete directory: %s (errno: %d, %s)", path.c_str(), errno, strerror(errno));
      return false;
    }
  }

  return success;
}

// API handlers
void HttpFileServer::handle_api_copy(AsyncWebServerRequest *request) {
  // Get parameters from POST body
  auto *source_param = request->getParam("source");
  auto *dest_param = request->getParam("destination");

  if (!source_param || !dest_param) {
    ESP_LOGW(TAG, "Missing source or destination parameter");
    request->send(400, "application/json", "{\"error\":\"Missing source or destination\"}");
    return;
  }

  // Convert URI paths to filesystem paths (strips URL prefix)
  std::string source = this->uri_to_filepath(source_param->value().c_str());
  std::string destination = this->uri_to_filepath(dest_param->value().c_str());

  ESP_LOGI(TAG, "API COPY: %s -> %s", source.c_str(), destination.c_str());

  // Check if source exists
  struct stat src_stat;
  if (stat(source.c_str(), &src_stat) != 0) {
    request->send(404, "application/json", "{\"error\":\"Source file not found\"}");
    return;
  }

  if (S_ISDIR(src_stat.st_mode)) {
    request->send(400, "application/json", "{\"error\":\"Directory copy not supported\"}");
    return;
  }

  // Perform copy with progress tracking for files > 100KB
  bool track_progress = (src_stat.st_size > 102400);
  if (this->perform_file_copy(source, destination, src_stat.st_size, track_progress)) {
    request->send(200, "application/json", "{\"success\":true}");
  } else {
    request->send(500, "application/json", "{\"error\":\"Copy operation failed\"}");
  }
}

void HttpFileServer::handle_api_move(AsyncWebServerRequest *request) {
  // Get parameters from POST body
  auto *source_param = request->getParam("source");
  auto *dest_param = request->getParam("destination");

  if (!source_param || !dest_param) {
    ESP_LOGW(TAG, "Missing source or destination parameter");
    request->send(400, "application/json", "{\"error\":\"Missing source or destination\"}");
    return;
  }

  // Convert URI paths to filesystem paths (strips URL prefix)
  std::string source = this->uri_to_filepath(source_param->value().c_str());
  std::string destination = this->uri_to_filepath(dest_param->value().c_str());

  ESP_LOGI(TAG, "API MOVE: %s -> %s", source.c_str(), destination.c_str());

  // Check if source exists
  struct stat src_stat;
  if (stat(source.c_str(), &src_stat) != 0) {
    request->send(404, "application/json", "{\"error\":\"Source file not found\"}");
    return;
  }

  if (S_ISDIR(src_stat.st_mode)) {
    request->send(400, "application/json", "{\"error\":\"Directory move not supported\"}");
    return;
  }

  // Perform move with progress tracking for files > 100KB
  bool track_progress = (src_stat.st_size > 102400);
  if (this->perform_file_move(source, destination, src_stat.st_size, track_progress)) {
    request->send(200, "application/json", "{\"success\":true}");
  } else {
    request->send(500, "application/json", "{\"error\":\"Move operation failed\"}");
  }
}

void HttpFileServer::handle_api_rename(AsyncWebServerRequest *request) {
  // Get parameters from POST body
  auto *source_param = request->getParam("source");
  auto *name_param = request->getParam("name");

  if (!source_param || !name_param) {
    ESP_LOGW(TAG, "Missing source or name parameter");
    request->send(400, "application/json", "{\"error\":\"Missing source or name\"}");
    return;
  }

  // Convert URI path to filesystem path (strips URL prefix)
  std::string source = this->uri_to_filepath(source_param->value().c_str());
  std::string new_name = name_param->value().c_str();

  // Build new path (same directory, new name)
  std::string dir_path = source.substr(0, source.find_last_of('/'));
  std::string new_path = Path::join(dir_path, new_name);

  ESP_LOGI(TAG, "API RENAME: %s -> %s", source.c_str(), new_path.c_str());

  // Check if source exists
  struct stat src_stat;
  if (stat(source.c_str(), &src_stat) != 0) {
    request->send(404, "application/json", "{\"error\":\"Source file not found\"}");
    return;
  }

  // Perform rename
  if (rename(source.c_str(), new_path.c_str()) == 0) {
    request->send(200, "application/json", "{\"success\":true}");
  } else {
    ESP_LOGE(TAG, "Rename failed: %s (errno: %d, %s)", source.c_str(), errno, strerror(errno));
    request->send(500, "application/json", "{\"error\":\"Rename operation failed\"}");
  }
}

void HttpFileServer::handle_api_mkdir(AsyncWebServerRequest *request) {
  // Get parameters from POST body
  auto *name_param = request->getParam("name");

  if (!name_param) {
    ESP_LOGW(TAG, "Missing name parameter");
    request->send(400, "application/json", "{\"error\":\"Missing directory name\"}");
    return;
  }

  std::string dir_uri = name_param->value().c_str();

  // Convert URI to filesystem path
  std::string dir_path = this->uri_to_filepath(dir_uri);

  ESP_LOGI(TAG, "API MKDIR: URI=%s -> Path=%s", dir_uri.c_str(), dir_path.c_str());

  // Create directory
  if (mkdir(dir_path.c_str(), 0755) == 0) {
    request->send(200, "application/json", "{\"success\":true}");
  } else {
    ESP_LOGE(TAG, "Mkdir failed: %s (errno: %d, %s)", dir_path.c_str(), errno, strerror(errno));
    request->send(500, "application/json", "{\"error\":\"Mkdir operation failed\"}");
  }
}

void HttpFileServer::handle_api_delete(AsyncWebServerRequest *request) {
  if (!this->deletion_enabled_) {
    ESP_LOGW(TAG, "Deletion is disabled");
    request->send(403, "application/json", "{\"error\":\"File deletion is disabled\"}");
    return;
  }

  // Get parameters from POST body
  auto *path_param = request->getParam("path");

  if (!path_param) {
    ESP_LOGW(TAG, "Missing path parameter");
    request->send(400, "application/json", "{\"error\":\"Missing path parameter\"}");
    return;
  }

  std::string path_uri = path_param->value().c_str();

  // Convert URI to filesystem path
  std::string filepath = this->uri_to_filepath(path_uri);

  ESP_LOGI(TAG, "API DELETE: URI=%s -> Path=%s", path_uri.c_str(), filepath.c_str());

  // Check if path exists
  struct stat file_stat;
  if (stat(filepath.c_str(), &file_stat) != 0) {
    request->send(404, "application/json", "{\"error\":\"Path not found\"}");
    return;
  }

  bool success = false;
  if (S_ISDIR(file_stat.st_mode)) {
    // Recursive directory deletion
    ESP_LOGI(TAG, "Deleting directory recursively: %s", filepath.c_str());
    success = this->recursive_delete_directory(filepath);
  } else {
    // Single file deletion
    ESP_LOGI(TAG, "Deleting file: %s", filepath.c_str());
    success = (remove(filepath.c_str()) == 0);
  }

  if (success) {
    request->send(200, "application/json", "{\"success\":true}");
  } else {
    ESP_LOGE(TAG, "Delete failed: %s (errno: %d, %s)", filepath.c_str(), errno, strerror(errno));
    request->send(500, "application/json", "{\"error\":\"Delete operation failed\"}");
  }
}

void HttpFileServer::handle_api_mount(AsyncWebServerRequest *request) {
  // Mount/unmount operations should be performed via ESPHome automations
  // using storage_host actions (storage_host.mount/storage_host.unmount)
  // This API endpoint is reserved for future functionality
  ESP_LOGW(TAG, "Mount API called - not implemented (use automations)");
  request->send(501, "application/json",
                "{\"error\":\"Mount operations must be performed via ESPHome automations\"}");
}

void HttpFileServer::handle_api_unmount(AsyncWebServerRequest *request) {
  // Mount/unmount operations should be performed via ESPHome automations
  // using storage_host actions (storage_host.mount/storage_host.unmount)
  // This API endpoint is reserved for future functionality
  ESP_LOGW(TAG, "Unmount API called - not implemented (use automations)");
  request->send(501, "application/json",
                "{\"error\":\"Unmount operations must be performed via ESPHome automations\"}");
}

void HttpFileServer::handle_api_progress(AsyncWebServerRequest *request) {
  // Build JSON response with progress information
  std::string json = "{";
  json += "\"in_progress\":" + std::string(this->progress_.in_progress ? "true" : "false");

  if (this->progress_.in_progress) {
    json += ",\"operation\":\"" + this->progress_.operation + "\"";
    json += ",\"source\":\"" + this->progress_.source + "\"";
    json += ",\"destination\":\"" + this->progress_.destination + "\"";
    json += ",\"total_bytes\":" + std::to_string(this->progress_.total_bytes);
    json += ",\"transferred_bytes\":" + std::to_string(this->progress_.transferred_bytes);

    // Calculate progress percentage
    float percentage = 0.0;
    if (this->progress_.total_bytes > 0) {
      percentage = (this->progress_.transferred_bytes * 100.0) / this->progress_.total_bytes;
    }

    // Format percentage with 1 decimal place
    char percent_buf[16];
    snprintf(percent_buf, sizeof(percent_buf), "%.1f", percentage);
    json += ",\"percentage\":" + std::string(percent_buf);

    // Calculate elapsed time
    uint32_t elapsed_ms = millis() - this->progress_.start_time;
    json += ",\"elapsed_ms\":" + std::to_string(elapsed_ms);

    // Estimate remaining time if we have progress
    if (this->progress_.transferred_bytes > 0 && this->progress_.total_bytes > 0) {
      uint32_t total_estimated_ms = (elapsed_ms * this->progress_.total_bytes) / this->progress_.transferred_bytes;
      uint32_t remaining_ms = total_estimated_ms - elapsed_ms;
      json += ",\"remaining_ms\":" + std::to_string(remaining_ms);
    }
  }

  json += "}";

  request->send(200, "application/json", json.c_str());
}

void HttpFileServer::handle_api_exists(AsyncWebServerRequest *request) {
  // Get path parameter from query string
  auto *path_param = request->getParam("path");

  if (!path_param) {
    request->send(400, "application/json", "{\"error\":\"Missing path parameter\"}");
    return;
  }

  // Convert URI path to filesystem path (strips URL prefix)
  std::string filepath = this->uri_to_filepath(path_param->value().c_str());

  // Check if file exists
  struct stat file_stat;
  bool exists = (stat(filepath.c_str(), &file_stat) == 0);

  std::string json = "{\"exists\":" + std::string(exists ? "true" : "false") + "}";
  request->send(200, "application/json", json.c_str());
}

void HttpFileServer::handle_api_dirisempty(AsyncWebServerRequest *request) {
  // Lightweight check - only checks if directory has any entries, doesn't count them
  auto *path_param = request->getParam("path");

  if (!path_param) {
    request->send(400, "application/json", "{\"error\":\"Missing path parameter\"}");
    return;
  }

  std::string dir_uri = path_param->value().c_str();
  std::string dirpath = this->uri_to_filepath(dir_uri);

  // Check if path exists and is a directory
  struct stat dir_stat;
  if (stat(dirpath.c_str(), &dir_stat) != 0) {
    request->send(404, "application/json", "{\"error\":\"Path not found\"}");
    return;
  }

  if (!S_ISDIR(dir_stat.st_mode)) {
    request->send(400, "application/json", "{\"error\":\"Path is not a directory\"}");
    return;
  }

  bool is_empty = this->is_directory_empty(dirpath);
  std::string json = "{\"is_empty\":" + std::string(is_empty ? "true" : "false") + "}";
  request->send(200, "application/json", json.c_str());
}

void HttpFileServer::handle_api_dirinfo(AsyncWebServerRequest *request) {
  // Get path parameter from query string
  auto *path_param = request->getParam("path");

  if (!path_param) {
    request->send(400, "application/json", "{\"error\":\"Missing path parameter\"}");
    return;
  }

  std::string dir_uri = path_param->value().c_str();
  std::string dirpath = this->uri_to_filepath(dir_uri);

  // Check if path exists and is a directory
  struct stat dir_stat;
  if (stat(dirpath.c_str(), &dir_stat) != 0) {
    request->send(404, "application/json", "{\"error\":\"Path not found\"}");
    return;
  }

  if (!S_ISDIR(dir_stat.st_mode)) {
    request->send(400, "application/json", "{\"error\":\"Path is not a directory\"}");
    return;
  }

  // Count directory contents recursively
  int file_count = 0;
  int dir_count = 0;
  this->count_directory_contents(dirpath, file_count, dir_count);

  bool is_empty = (file_count == 0 && dir_count == 0);
  int total_items = file_count + dir_count;

  std::string json = "{";
  json += "\"is_empty\":" + std::string(is_empty ? "true" : "false");
  json += ",\"file_count\":" + std::to_string(file_count);
  json += ",\"dir_count\":" + std::to_string(dir_count);
  json += ",\"total_items\":" + std::to_string(total_items);
  json += "}";

  request->send(200, "application/json", json.c_str());
}

// Form data parsing helper
bool HttpFileServer::parse_json_request(const uint8_t *body, size_t body_len, ApiRequest &req) {
  // Parse URL-encoded form data
  // Format: source=/path/to/file&destination=/path/to/dest&name=newname
  std::string form_data((const char *) body, body_len);

  // Split by '&' to get key=value pairs
  size_t pos = 0;
  while (pos < form_data.length()) {
    size_t amp_pos = form_data.find('&', pos);
    if (amp_pos == std::string::npos) {
      amp_pos = form_data.length();
    }

    std::string pair = form_data.substr(pos, amp_pos - pos);
    size_t eq_pos = pair.find('=');
    if (eq_pos != std::string::npos) {
      std::string key = pair.substr(0, eq_pos);
      std::string value = pair.substr(eq_pos + 1);

      // URL decode the value
      value = this->url_decode(value);

      if (key == "source") {
        req.source = value;
      } else if (key == "destination") {
        req.destination = value;
      } else if (key == "name") {
        req.name = value;
      }
    }

    pos = amp_pos + 1;
  }

  return !req.source.empty() || !req.destination.empty() || !req.name.empty();
}

// RAII wrapper for FILE* to ensure files are always closed
struct FileCloser {
  FILE *fp;
  FileCloser(FILE *f) : fp(f) {}
  ~FileCloser() {
    if (fp) {
      fclose(fp);
    }
  }
  // Delete copy/move to prevent double-close
  FileCloser(const FileCloser &) = delete;
  FileCloser &operator=(const FileCloser &) = delete;
};

// File operation helpers (reused from WebDAV logic)
bool HttpFileServer::perform_file_copy(const std::string &src_path, const std::string &dst_path, off_t file_size,
                                       bool track_progress) {
  // Initialize progress tracking if requested (only if not already in progress)
  // This allows perform_file_move to set up progress as "move" before calling this function
  if (track_progress && !this->progress_.in_progress) {
    this->progress_.operation = "copy";
    this->progress_.source = src_path;
    this->progress_.destination = dst_path;
    this->progress_.total_bytes = file_size;
    this->progress_.transferred_bytes = 0;
    this->progress_.in_progress = true;
    this->progress_.start_time = millis();
  }

  FILE *src = fopen(src_path.c_str(), "rb");
  if (!src) {
    ESP_LOGE(TAG, "Failed to open source file: %s (errno: %d, %s)", src_path.c_str(), errno, strerror(errno));
    if (track_progress)
      this->progress_.in_progress = false;
    return false;
  }
  FileCloser src_closer(src);  // RAII: will close src on scope exit

  FILE *dst = fopen(dst_path.c_str(), "wb");
  if (!dst) {
    ESP_LOGE(TAG, "Failed to open destination file: %s (errno: %d, %s)", dst_path.c_str(), errno, strerror(errno));
    if (track_progress)
      this->progress_.in_progress = false;
    return false;
  }
  FileCloser dst_closer(dst);  // RAII: will close dst on scope exit

  auto buffer = std::make_unique<char[]>(FILE_BUFFER_SIZE);
  size_t bytes_read;
  size_t total_copied = 0;
  bool copy_success = true;

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

    // Update progress
    if (track_progress) {
      this->progress_.transferred_bytes = total_copied;
    }

    // Log progress for large files
    if (total_copied % (FILE_BUFFER_SIZE * 64) == 0 && file_size > 0) {
      ESP_LOGD(TAG, "Copy progress: %zu / %lld bytes (%.1f%%)", total_copied, (long long) file_size,
               (total_copied * 100.0) / file_size);
    }
  }

  // Check for read errors
  if (ferror(src)) {
    ESP_LOGE(TAG, "Read error at offset %zu (errno: %d, %s)", total_copied, errno, strerror(errno));
    copy_success = false;
  }

  // Flush destination file before closing
  if (fflush(dst) != 0) {
    ESP_LOGE(TAG, "Flush failed (errno: %d, %s)", errno, strerror(errno));
    copy_success = false;
  }

  // Files will be automatically closed by RAII wrappers (FileCloser destructors)
  // This happens even if HTTP connection times out or handler is interrupted

  if (copy_success && total_copied == static_cast<size_t>(file_size)) {
    ESP_LOGI(TAG, "File copy completed successfully: %zu bytes", total_copied);
    // Clear progress tracking on success (unless perform_file_move will handle it)
    // Note: perform_file_move sets operation to "move" and will clear progress itself
    if (track_progress && this->progress_.operation == "copy") {
      this->progress_.in_progress = false;
    }
    return true;
  } else {
    ESP_LOGE(TAG, "File copy failed: %s to %s (copied %zu of %lld bytes)", src_path.c_str(), dst_path.c_str(),
             total_copied, (long long) file_size);

    // Clean up partial file
    if (remove(dst_path.c_str()) != 0) {
      ESP_LOGE(TAG, "Failed to remove partial destination file (errno: %d, %s)", errno, strerror(errno));
    }

    // Clear progress tracking on failure (unless perform_file_move will handle it)
    if (track_progress && this->progress_.operation == "copy") {
      this->progress_.in_progress = false;
    }
    return false;
  }
}

bool HttpFileServer::perform_file_move(const std::string &src_path, const std::string &dst_path, off_t file_size,
                                       bool track_progress) {
  // Initialize progress tracking if requested
  if (track_progress) {
    this->progress_.operation = "move";
    this->progress_.source = src_path;
    this->progress_.destination = dst_path;
    this->progress_.total_bytes = file_size;
    this->progress_.transferred_bytes = 0;
    this->progress_.in_progress = true;
    this->progress_.start_time = millis();
  }

  // Try atomic rename first (works within same filesystem)
  if (rename(src_path.c_str(), dst_path.c_str()) == 0) {
    ESP_LOGI(TAG, "File moved successfully (atomic rename)");
    if (track_progress) {
      this->progress_.transferred_bytes = file_size;
      this->progress_.in_progress = false;
    }
    return true;
  }

  // If rename fails with EXDEV (cross-device), fall back to copy+delete
  if (errno == EXDEV) {
    ESP_LOGI(TAG, "Cross-mount move detected, using copy+delete fallback");
    // Note: perform_file_copy will handle progress tracking if track_progress is true
    if (perform_file_copy(src_path, dst_path, file_size, track_progress)) {
      if (remove(src_path.c_str()) == 0) {
        ESP_LOGI(TAG, "File move completed successfully");
        if (track_progress)
          this->progress_.in_progress = false;
        return true;
      } else {
        ESP_LOGE(TAG, "Failed to delete source after copy: %s (errno: %d, %s)", src_path.c_str(), errno,
                 strerror(errno));
        // File was copied but source remains - still consider success
        if (track_progress)
          this->progress_.in_progress = false;
        return true;
      }
    } else {
      if (track_progress)
        this->progress_.in_progress = false;
    }
  } else {
    ESP_LOGE(TAG, "Failed to move %s to %s (errno: %d, %s)", src_path.c_str(), dst_path.c_str(), errno,
             strerror(errno));
    if (track_progress)
      this->progress_.in_progress = false;
  }

  return false;
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
      {"mp3", "Audio (MP3)"},   {"wav", "Audio (WAV)"},   {"flac", "Audio (FLAC)"}, {"png", "Image (PNG)"},
      {"jpg", "Image (JPG)"},   {"jpeg", "Image (JPEG)"}, {"gif", "Image (GIF)"},   {"bmp", "Image (BMP)"},
      {"txt", "Text (TXT)"},    {"log", "Text (LOG)"},    {"csv", "Text (CSV)"},    {"html", "Web (HTML)"},
      {"css", "Web (CSS)"},     {"js", "Web (JS)"},       {"json", "Data (JSON)"},  {"xml", "Data (XML)"},
      {"zip", "Archive (ZIP)"}, {"gz", "Archive (GZ)"},   {"tar", "Archive (TAR)"}, {"mp4", "Video (MP4)"},
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
      {"mp3", "audio/mpeg"},        {"wav", "audio/wav"},       {"flac", "audio/flac"},
      {"png", "image/png"},         {"jpg", "image/jpeg"},      {"jpeg", "image/jpeg"},
      {"gif", "image/gif"},         {"bmp", "image/bmp"},       {"txt", "text/plain"},
      {"log", "text/plain"},        {"csv", "text/csv"},        {"html", "text/html"},
      {"htm", "text/html"},         {"css", "text/css"},        {"js", "application/javascript"},
      {"json", "application/json"}, {"xml", "application/xml"}, {"pdf", "application/pdf"},
      {"zip", "application/zip"},   {"gz", "application/gzip"}, {"tar", "application/x-tar"},
      {"mp4", "video/mp4"},         {"avi", "video/x-msvideo"}, {"webm", "video/webm"},
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
