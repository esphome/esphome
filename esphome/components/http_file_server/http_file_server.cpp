#include "http_file_server.h"
#include "esphome/components/storage_host/storage_host.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esphome/core/application.h"

// Storage device headers for mount/unmount
#ifdef USE_USB_MSC_HOST
#include "esphome/components/usb_msc_host/usb_msc_host.h"
#endif
#ifdef USE_SD_MMC_CARD
#include "esphome/components/sd_mmc_card/sd_mmc_card.h"
#endif

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

void HttpFileServer::loop() {
  // Check for deferred mount operations
  if (this->deferred_mount_op_.type != DeferredMountOp::NONE) {
    // Check if it's time to execute the deferred operation
    if (millis() >= this->deferred_mount_op_.schedule_time) {
      std::string mount_point = this->deferred_mount_op_.mount_point;
      DeferredMountOp::Type op_type = this->deferred_mount_op_.type;

      // Clear the deferred operation first (prevents re-execution)
      this->deferred_mount_op_.type = DeferredMountOp::NONE;
      this->deferred_mount_op_.mount_point.clear();

      ESP_LOGI(TAG, "Executing deferred %s operation for mount point: %s",
               op_type == DeferredMountOp::MOUNT     ? "MOUNT"
               : op_type == DeferredMountOp::UNMOUNT ? "UNMOUNT"
                                                     : "REMOUNT",
               mount_point.c_str());

      // Execute the deferred operation
      bool success = false;
      std::string error_msg;

      if (op_type == DeferredMountOp::UNMOUNT) {
        // Try to find matching USB MSC device and unmount
#ifdef USE_USB_MSC_HOST
        for (void *dev_ptr : this->usb_msc_devices_) {
          auto *device = static_cast<usb_msc_host::USBMscDevice *>(dev_ptr);
          if (device->get_mount_path() == mount_point) {
            device->unmount_device();
            ESP_LOGI(TAG, "Successfully unmounted USB MSC device at %s", mount_point.c_str());
            success = true;
            break;
          }
        }
#endif

        // Try SD MMC devices if not found in USB devices
        if (!success && !this->sd_mmc_devices_.empty()) {
#ifdef USE_SD_MMC_CARD
          for (void *dev_ptr : this->sd_mmc_devices_) {
            auto *device = static_cast<sd_mmc_card::SdMmc *>(dev_ptr);
            if (device->get_mount_path() == mount_point) {
              device->unmount_card();
              ESP_LOGI(TAG, "Successfully unmounted SD MMC device at %s", mount_point.c_str());
              success = true;
              break;
            }
          }
#endif
        }

        if (!success) {
          ESP_LOGW(TAG, "Failed to unmount: device not found for mount point %s", mount_point.c_str());
        }

      } else if (op_type == DeferredMountOp::MOUNT) {
        // Try to find matching USB MSC device and mount
#ifdef USE_USB_MSC_HOST
        for (void *dev_ptr : this->usb_msc_devices_) {
          auto *device = static_cast<usb_msc_host::USBMscDevice *>(dev_ptr);
          if (device->get_mount_path() == mount_point) {
            if (device->remount_device()) {
              ESP_LOGI(TAG, "Successfully mounted USB MSC device at %s", mount_point.c_str());
              success = true;
            } else {
              ESP_LOGE(TAG, "Failed to mount USB MSC device at %s", mount_point.c_str());
              error_msg = "Mount failed";
            }
            break;
          }
        }
#endif

        // Try SD MMC devices if not found in USB devices
        if (!success && !this->sd_mmc_devices_.empty()) {
#ifdef USE_SD_MMC_CARD
          for (void *dev_ptr : this->sd_mmc_devices_) {
            auto *device = static_cast<sd_mmc_card::SdMmc *>(dev_ptr);
            if (device->get_mount_path() == mount_point) {
              if (device->mount_card()) {
                ESP_LOGI(TAG, "Successfully mounted SD MMC device at %s", mount_point.c_str());
                success = true;
              } else {
                ESP_LOGE(TAG, "Failed to mount SD MMC device at %s", mount_point.c_str());
                error_msg = "Mount failed";
              }
              break;
            }
          }
#endif
        }

        if (!success && error_msg.empty()) {
          ESP_LOGW(TAG, "Failed to mount: device not found for mount point %s", mount_point.c_str());
        }

      } else if (op_type == DeferredMountOp::REMOUNT) {
        // Try to find matching USB MSC device and remount
#ifdef USE_USB_MSC_HOST
        for (void *dev_ptr : this->usb_msc_devices_) {
          auto *device = static_cast<usb_msc_host::USBMscDevice *>(dev_ptr);
          if (device->get_mount_path() == mount_point) {
            if (device->remount_device()) {
              ESP_LOGI(TAG, "Successfully remounted USB MSC device at %s", mount_point.c_str());
              success = true;
            } else {
              ESP_LOGE(TAG, "Failed to remount USB MSC device at %s", mount_point.c_str());
              error_msg = "Remount failed";
            }
            break;
          }
        }
#endif

        // Try SD MMC devices if not found in USB devices
        if (!success && !this->sd_mmc_devices_.empty()) {
#ifdef USE_SD_MMC_CARD
          for (void *dev_ptr : this->sd_mmc_devices_) {
            auto *device = static_cast<sd_mmc_card::SdMmc *>(dev_ptr);
            if (device->get_mount_path() == mount_point) {
              // Remount = unmount then mount
              device->unmount_card();
              if (device->mount_card()) {
                ESP_LOGI(TAG, "Successfully remounted SD MMC device at %s", mount_point.c_str());
                success = true;
              } else {
                ESP_LOGE(TAG, "Failed to remount SD MMC device at %s", mount_point.c_str());
                error_msg = "Remount failed";
              }
              break;
            }
          }
#endif
        }

        if (!success && error_msg.empty()) {
          ESP_LOGW(TAG, "Failed to remount: device not found for mount point %s", mount_point.c_str());
        }
      }
    }
  }
}

// AsyncWebHandler interface implementation
bool HttpFileServer::canHandle(AsyncWebServerRequest *request) const {
  // Handle requests that start with our URL prefix
  std::string uri = request->url().c_str();
  const char *method_name = (request->method() == HTTP_GET)      ? "GET"
                            : (request->method() == HTTP_POST)   ? "POST"
                            : (request->method() == HTTP_DELETE) ? "DELETE"
                                                                 : "OTHER";

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
  ESP_LOGI(TAG, "handleRequest: uri='%s', url_prefix_='%s', method=%d", uri.c_str(), this->url_prefix_.c_str(),
           request->method());

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
  } else if (uri.find(this->url_prefix_ + "/api/remount") == 0 && request->method() == HTTP_POST) {
    ESP_LOGD(TAG, "API REMOUNT endpoint hit, body_buffer size: %zu", this->body_buffer_.size());
    this->handle_api_remount(request);
  } else if (uri.find(this->url_prefix_ + "/api/exists") == 0 && request->method() == HTTP_GET) {
    this->handle_api_exists(request);
  } else if (uri.find(this->url_prefix_ + "/api/dirisempty") == 0 && request->method() == HTTP_GET) {
    this->handle_api_dirisempty(request);
  } else if (uri.find(this->url_prefix_ + "/api/dirinfo") == 0 && request->method() == HTTP_GET) {
    this->handle_api_dirinfo(request);
  } else if (uri.find(this->url_prefix_ + "/api/progress") == 0 && request->method() == HTTP_GET) {
    this->handle_api_progress(request);
  } else if (uri.find(this->url_prefix_ + "/api/cancel") == 0 && request->method() == HTTP_POST) {
    ESP_LOGD(TAG, "API CANCEL endpoint hit");
    this->handle_api_cancel(request);
  } else if (uri.find(this->url_prefix_ + "/api/upload_chunk") == 0 && request->method() == HTTP_POST) {
    ESP_LOGD(TAG, "API UPLOAD_CHUNK endpoint hit, body_buffer size: %zu", this->body_buffer_.size());
    this->handle_api_upload_chunk(request);
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

    // Check Content-Type to determine upload method
    auto content_type = request->get_header("Content-Type");
    if (content_type.has_value()) {
      ESP_LOGD(TAG, "Content-Type: %s", content_type.value().c_str());
    }

    // If multipart/form-data, it was already handled by handleUpload() callback
    // The web server calls handleRequest() after processing multipart, so just send success
    if (content_type.has_value() && content_type.value().find("multipart/form-data") != std::string::npos) {
      ESP_LOGD(TAG, "Multipart upload was handled by handleUpload() callback");
      // Don't send another response - handleUpload() already sent it
      return;
    } else {
      // Old synchronous upload handler for raw binary uploads
      auto *filename_param = request->getParam("filename");
      if (filename_param) {
        this->handle_file_upload(request, filename_param->value().c_str());
      } else {
        request->send(400, "text/plain", "Missing filename parameter");
      }
    }
  } else {
    request->send(405, "application/json", "{\"error\":\"Method not allowed\"}");
  }
}

void HttpFileServer::handleUpload(AsyncWebServerRequest *request, const PlatformString &filename, size_t index,
                                  uint8_t *data, size_t len, bool final) {
  // Only log at milestones to avoid flooding logs with hundreds of tiny chunk calls
  if (index == 0 || final || (index % 1048576 == 0)) {  // Log at start, end, and every 1MB
    ESP_LOGD(TAG, "handleUpload: filename='%s', index=%zu, len=%zu, final=%d", filename.c_str(), index, len, final);
  }

  // Check authentication if enabled
  if (this->auth_enabled_) {
    if (!request->authenticate(this->username_.c_str(), this->password_.c_str())) {
      ESP_LOGW(TAG, "Upload authentication failed");
      return;
    }
  }

  if (!this->upload_enabled_) {
    ESP_LOGW(TAG, "Upload disabled in configuration");
    if (final) {
      request->send(403, "text/plain", "File upload is disabled");
    }
    return;
  }

  // Get upload directory from request URL
  if (index == 0) {
    // Clean up any stale upload state from previous failed upload
    if (this->upload_file_) {
      ESP_LOGW(TAG, "Closing stale upload file from previous upload");
      fclose(this->upload_file_);
      this->upload_file_ = nullptr;
    }

    std::string uri = request->url().c_str();
    ESP_LOGI(TAG, "Upload started: index=0, uri='%s', filename='%s'", uri.c_str(), filename.c_str());

    this->upload_directory_ = this->uri_to_filepath(uri);
    this->upload_filename_ = filename.c_str();

    ESP_LOGD(TAG, "Upload directory: %s", this->upload_directory_.c_str());

    // Check if target is a directory
    struct stat file_stat;
    if (stat(this->upload_directory_.c_str(), &file_stat) != 0 || !S_ISDIR(file_stat.st_mode)) {
      ESP_LOGE(TAG, "Upload target is not a directory: %s (errno: %d)", this->upload_directory_.c_str(), errno);
      if (final) {
        request->send(400, "text/plain", "Upload target must be a directory");
      }
      return;
    }

    // Open file for writing
    std::string upload_path = Path::join(this->upload_directory_, this->upload_filename_);
    ESP_LOGI(TAG, "Starting async upload: %s", upload_path.c_str());

    // Get expected file size from query parameter (if provided by JavaScript)
    size_t expected_size = 0;
    auto *filesize_param = request->getParam("filesize");
    if (filesize_param) {
      expected_size = std::stoul(filesize_param->value().c_str());
      ESP_LOGI(TAG, "Expected upload size from query param: %zu bytes", expected_size);
    } else {
      ESP_LOGW(TAG, "No filesize query parameter found in upload request");
    }

    // Initialize progress tracking (thread-safe)
    portENTER_CRITICAL(&this->progress_mutex_);
    this->progress_.operation = "upload";
    this->progress_.source = filename.c_str();
    this->progress_.destination = upload_path;
    this->progress_.total_bytes = expected_size;  // Use expected size from JavaScript
    this->progress_.transferred_bytes = 0;
    this->progress_.in_progress = true;
    this->progress_.cancelled = false;
    this->progress_.start_time = millis();
    portEXIT_CRITICAL(&this->progress_mutex_);

    // Reset yield tracking for upload
    this->upload_bytes_since_yield_ = 0;

    this->upload_file_ = fopen(upload_path.c_str(), "wb");
    if (!this->upload_file_) {
      ESP_LOGE(TAG, "Failed to open file for async upload: %s", upload_path.c_str());
      portENTER_CRITICAL(&this->progress_mutex_);
      this->progress_.in_progress = false;
      this->progress_.cancelled = false;  // Reset cancelled flag after handling
      portEXIT_CRITICAL(&this->progress_mutex_);
      if (final) {
        request->send(500, "text/plain", "Failed to create file");
      }
      return;
    }
  }

  // Check if upload_file is null (happens if index != 0 on first call, meaning we missed initialization)
  if (!this->upload_file_ && index != 0) {
    ESP_LOGE(TAG, "Upload file not initialized (index=%zu, expected to start at 0). Upload state corrupted.", index);
    if (final) {
      request->send(500, "text/plain", "Upload state corrupted - please retry upload");
    }
    return;
  }

  // Check for cancellation (thread-safe)
  portENTER_CRITICAL(&this->progress_mutex_);
  bool cancelled = this->progress_.cancelled;
  portEXIT_CRITICAL(&this->progress_mutex_);

  if (cancelled && this->upload_file_) {
    ESP_LOGI(TAG, "Upload cancelled by user");
    fclose(this->upload_file_);
    this->upload_file_ = nullptr;

    // Delete partial file
    std::string upload_path = Path::join(this->upload_directory_, this->upload_filename_);
    if (remove(upload_path.c_str()) == 0) {
      ESP_LOGI(TAG, "Deleted partial upload file: %s", upload_path.c_str());
    } else {
      ESP_LOGE(TAG, "Failed to delete partial upload file: %s (errno: %d)", upload_path.c_str(), errno);
    }

    portENTER_CRITICAL(&this->progress_mutex_);
    this->progress_.in_progress = false;
    this->progress_.cancelled = false;  // Reset cancelled flag after handling
    portEXIT_CRITICAL(&this->progress_mutex_);

    if (final) {
      request->send(499, "text/plain", "Upload cancelled");
    }
    return;
  }

  // Write data chunk
  if (this->upload_file_ && len > 0) {
    size_t written = fwrite(data, 1, len, this->upload_file_);
    if (written != len) {
      ESP_LOGE(TAG, "Failed to write async upload data");
      fclose(this->upload_file_);
      this->upload_file_ = nullptr;

      // Delete partial file on write failure
      std::string upload_path = Path::join(this->upload_directory_, this->upload_filename_);
      if (remove(upload_path.c_str()) == 0) {
        ESP_LOGI(TAG, "Deleted partial upload file after write failure: %s", upload_path.c_str());
      } else {
        ESP_LOGE(TAG, "Failed to delete partial upload file: %s (errno: %d)", upload_path.c_str(), errno);
      }

      portENTER_CRITICAL(&this->progress_mutex_);
      this->progress_.in_progress = false;
      this->progress_.cancelled = false;  // Reset cancelled flag after handling
      portEXIT_CRITICAL(&this->progress_mutex_);
      if (final) {
        request->send(500, "text/plain", "Failed to write file");
      }
      return;
    }

    // Update progress (thread-safe)
    portENTER_CRITICAL(&this->progress_mutex_);
    if (this->progress_.in_progress) {
      this->progress_.transferred_bytes += written;
    }
    portEXIT_CRITICAL(&this->progress_mutex_);

    // Yield CPU periodically to allow web server to handle other requests (like progress API)
    // Longer yield time (100ms) but less frequent (every 128KB instead of 16KB)
    this->upload_bytes_since_yield_ += written;
    static constexpr size_t YIELD_INTERVAL_BYTES = 128 * 1024;  // 128KB between yields
    if (this->upload_bytes_since_yield_ >= YIELD_INTERVAL_BYTES) {
      vTaskDelay(10);  // Yield for ~100ms to give httpd time to send progress responses
      this->upload_bytes_since_yield_ = 0;
    }
  }

  // Finalize upload
  if (final) {
    if (this->upload_file_) {
      fclose(this->upload_file_);
      this->upload_file_ = nullptr;
      ESP_LOGI(TAG, "Async upload completed: %s", this->upload_filename_.c_str());

      // Mark progress as complete (thread-safe)
      portENTER_CRITICAL(&this->progress_mutex_);
      this->progress_.in_progress = false;
      this->progress_.cancelled = false;  // Reset cancelled flag after handling
      portEXIT_CRITICAL(&this->progress_mutex_);

      request->send(201, "text/plain", "File uploaded successfully");
    } else {
      portENTER_CRITICAL(&this->progress_mutex_);
      this->progress_.in_progress = false;
      this->progress_.cancelled = false;  // Reset cancelled flag after handling
      portEXIT_CRITICAL(&this->progress_mutex_);
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
    .progress-speed {
      margin-top: 0.5rem;
      font-size: 0.9rem;
      color: #666;
    }
    #cancelBtn {
      margin-top: 1rem;
      width: 100%;
    }
  </style>
</head>
<body>
<div class="container">
)";
  return html;
}

std::string HttpFileServer::generate_html_footer() {
  return R"HTML(
</div>
<div id="progressModal" class="progress-modal">
  <div class="progress-content">
    <div class="progress-title" id="progressTitle">Processing...</div>
    <div class="progress-bar-container">
      <div class="progress-bar" id="progressBar">0%</div>
    </div>
    <div class="progress-details" id="progressDetails">Initializing...</div>
    <div class="progress-speed" id="progressSpeed"></div>
    <div class="progress-file-info">
      <div><strong>From:</strong> <span id="progressSource">-</span></div>
      <div><strong>To:</strong> <span id="progressDest">-</span></div>
    </div>
    <button id="cancelBtn" class="delete" onclick="cancelOperation()" > Cancel</ button></ div></ div><script>
             // API base path for this file server instance
             const API_BASE = ')HTML" +
         this->url_prefix_ + R"HTML(';
  console.log('[FileServer] Script loaded, API_BASE:', API_BASE);
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
    if (bytes === 0)
      return '0 B';
    const k = 1024;
    const sizes = ['B', 'KB', 'MB', 'GB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    return (bytes / Math.pow(k, i)).toFixed(1) + ' ' + sizes[i];
  }

  function formatTime(ms) {
    if (ms < 1000)
      return ms + 'ms';
    const seconds = Math.floor(ms / 1000);
    if (seconds < 60)
      return seconds + 's';
    const minutes = Math.floor(seconds / 60);
    const remainingSeconds = seconds % 60;
    return minutes + 'm ' + remainingSeconds + 's';
  }

  let progressPollCount = 0;
  let hasSeenProgress = false;
  let consecutiveTimeouts = 0;
  let lastTransferredBytes = 0;
  let lastProcessedItems = 0;
  let stallCount = 0;  // Count polls with no progress change

  function pollProgress() {
    console.log('[FileServer] pollProgress() START, count=' + progressPollCount);
    progressPollCount++;

    const url = API_BASE + '/api/progress';
    console.log('[FileServer] Polling:', url);

    // Use XMLHttpRequest instead of fetch for better compatibility with ESP32 web server
    const xhr = new XMLHttpRequest();
    xhr.open('GET', url, true);
    xhr.withCredentials = true;  // Include authentication credentials
    xhr.timeout = 5000;          // 5 second timeout

    xhr.onload = function() {
      console.log('[FileServer] XHR onload, status:', xhr.status, 'response:', xhr.responseText);

      // Reset timeout counter on successful response
      consecutiveTimeouts = 0;

      if (xhr.status !== 200) {
        console.error('[FileServer] Bad status:', xhr.status, 'text:', xhr.responseText);
        consecutiveTimeouts++;
        checkTimeoutLimit();
        return;
      }

      let data;
      try {
        data = JSON.parse(xhr.responseText);
      } catch (e) {
        console.error('[FileServer] JSON parse error:', e, 'text:', xhr.responseText);
        return;
      }

      console.log('[FileServer] Progress update #' + progressPollCount + ':', data);

      if (!data.in_progress) {
        // Only close if we've seen progress before, or after 10 polls
        if (hasSeenProgress || progressPollCount > 10) {
          console.log('[FileServer] Operation complete, closing modal');
          hideProgressModal();
          location.reload();
        } else {
          console.log('[FileServer] Waiting for operation to start...');
        }
        return;
      }

      // Mark that we've seen progress
      hasSeenProgress = true;

      // Check if progress is actually advancing (not stalled)
      let progressMade = false;
      if (data.operation === 'delete') {
        progressMade = (data.processed_items > lastProcessedItems);
        lastProcessedItems = data.processed_items;
      } else {
        progressMade = (data.transferred_bytes > lastTransferredBytes);
        lastTransferredBytes = data.transferred_bytes;
      }

      if (progressMade) {
        stallCount = 0;  // Reset stall counter when progress is made
      } else {
        stallCount++;
        console.log('[FileServer] No progress detected, stall count:', stallCount);
        // Only abort if stalled for 5 consecutive polls (10 seconds with no progress at 2s interval)
        if (stallCount >= 5) {
          console.warn('[FileServer] Operation appears stalled, calling cancel and closing modal');
          // Call cancel API to clean up partial files and reset backend state
          fetch(API_BASE + '/api/cancel', {method: 'POST'})
              .then(() => {
                console.log('[FileServer] Cancel request sent');
              })
              .catch(error => {
                console.error('[FileServer] Cancel request failed:', error);
              });
          hideProgressModal();
          alert('Operation appears to have stalled and has been cancelled. Please refresh the page.');
          return;
        }
      }

      const bar = document.getElementById('progressBar');
      const details = document.getElementById('progressDetails');
      const speed = document.getElementById('progressSpeed');

      const percentage = data.percentage || 0;
      bar.style.width = percentage + '%';
      bar.textContent = percentage.toFixed(1) + '%';

      let detailText = '';
      if (data.operation === 'delete') {
        // For delete operations, show item count and current file
        detailText = data.processed_items + ' / ' + data.total_items + ' items';
        if (data.elapsed_ms) {
          detailText += ' • Elapsed: ' + formatTime(data.elapsed_ms);
        }
        // Show current file being deleted
        if (data.current_file) {
          speed.textContent = 'Deleting: ' + data.current_file;
        } else {
          speed.textContent = '';
        }
      } else {
        // For byte-based operations (copy, move, upload)
        detailText = formatBytes(data.transferred_bytes) + ' / ' + formatBytes(data.total_bytes);
        if (data.elapsed_ms) {
          detailText += ' • Elapsed: ' + formatTime(data.elapsed_ms);
        }
        if (data.remaining_ms) {
          detailText += ' • Remaining: ' + formatTime(data.remaining_ms);
        }
        // Display average speed
        if (data.avg_speed && data.avg_speed > 0) {
          speed.textContent = 'Average speed: ' + formatBytes(data.avg_speed) + '/s';
        } else {
          speed.textContent = '';
        }
      }

      details.textContent = detailText;

      console.log('[FileServer] Updated progress bar:', percentage.toFixed(1) + '%');
    };

    xhr.onerror = function() {
      console.error('[FileServer] XHR network error, status:', xhr.status, 'readyState:', xhr.readyState);
      consecutiveTimeouts++;
      checkTimeoutLimit();
    };

    xhr.ontimeout = function() {
      console.error('[FileServer] XHR timeout after 5s (consecutive: ' + (consecutiveTimeouts + 1) + ')');
      consecutiveTimeouts++;
      checkTimeoutLimit();
    };

    xhr.onreadystatechange = function() {
      console.log('[FileServer] XHR readyState changed to:', xhr.readyState, 'status:', xhr.status);
    };

    try {
      xhr.send();
      console.log('[FileServer] XHR sent successfully');
    } catch (e) {
      console.error('[FileServer] XHR send failed:', e);
    }
  }

  function checkTimeoutLimit() {
    // Close modal after 5 consecutive timeouts (25 seconds of no responses)
    // This handles the case where upload is blocking the server from responding
    if (consecutiveTimeouts >= 5) {
      console.warn('[FileServer] Too many consecutive timeouts (' + consecutiveTimeouts + '), closing modal');
      alert(
          'Progress tracking unavailable - the operation may still be running in the background. Please refresh the page in a few moments.');
      hideProgressModal();
    }
  }

  function startProgressPolling() {
    console.log('[FileServer] startProgressPolling called');
    // Reset progress tracking
    progressPollCount = 0;
    hasSeenProgress = false;
    consecutiveTimeouts = 0;
    lastTransferredBytes = 0;
    lastProcessedItems = 0;
    stallCount = 0;

    if (progressPollInterval) {
      clearInterval(progressPollInterval);
    }
    // Poll every 2 seconds (reduced from 500ms to minimize httpd socket/connection load)
    progressPollInterval = setInterval(pollProgress, 2000);
    console.log('[FileServer] Set interval ID:', progressPollInterval);
    // First poll after 500ms delay to give backend time to start tracking
    setTimeout(pollProgress, 500);
    console.log('[FileServer] Scheduled first poll in 500ms');
  }

  function cancelOperation() {
    if (!confirm('Are you sure you want to cancel the current operation?')) {
      return;
    }

    fetch(API_BASE + '/api/cancel', {method: 'POST', headers: {'Content-Type': 'application/json'}})
        .then(response => response.json())
        .then(data =>
                     {
                       if (data.success) {
                         alert('Operation cancelled');
                         hideProgressModal();
                         location.reload();
                       } else {
                         alert(data.message || 'No operation in progress');
                       }
                     })
        .catch(error => { alert('Error cancelling operation: ' + error); });
  }

  function delete_file(path) {
    if (confirm('Are you sure you want to delete this file?')) {
      fetch(API_BASE + '/api/delete', {
        method: 'POST',
        headers: {'Content-Type': 'application/x-www-form-urlencoded'},
        body: 'path=' + encodeURIComponent(path)
      })
          .then(response =>
                           {
                             if (!response.ok) {
                               return response.text().then(
                                   text => { throw new Error('HTTP ' + response.status + ': ' + text); });
                             }
                             return response.json();
                           })
          .then(data =>
                       {
                         if (data.success) {
                           alert('File deleted successfully!');
                           location.reload();
                         } else {
                           alert('Delete failed: ' + (data.error || 'Unknown error'));
                         }
                       })
          .catch(error => { alert('Error: ' + error); });
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
          .then(response =>
                           {
                             if (!response.ok) {
                               return response.text().then(
                                   text => { throw new Error('HTTP ' + response.status + ': ' + text); });
                             }
                             return response.json();
                           })
          .then(data =>
                       {
                         if (data.success) {
                           alert('Directory deleted successfully!');
                           location.reload();
                         } else {
                           alert('Delete failed: ' + (data.error || 'Unknown error'));
                         }
                       })
          .catch(error => { alert('Error: ' + error); });
    } catch (error) {
      alert('Error: ' + error);
    }
  }

  function download_file(path, filename) {
    // Direct download - just navigate to the URL
    // The Content-Disposition: attachment header will trigger the browser's save dialog
    window.location.href = path;
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
    console.log('[FileServer] performCopy called:', source, '->', destination);
    showProgressModal('copy', source, destination);
    console.log('[FileServer] Starting progress polling');
    startProgressPolling();

    fetch(API_BASE + '/api/copy', {
      method: 'POST',
      headers: {'Content-Type': 'application/x-www-form-urlencoded'},
      body: 'source=' + encodeURIComponent(source) + '&destination=' + encodeURIComponent(destination)
    })
        .then(response => response.json())
        .then(data =>
                     {
                       if (data.success) {
                         // Task started successfully - polling will handle closing modal when done
                         console.log('[FileServer] Copy task started, polling will track progress');
                       } else {
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
    let destination = prompt('Enter destination path:', source);
    if (!destination)
      return;

    // Check if destination exists
    const exists = await fileExists(destination);
    if (exists) {
      const choice = confirm('File already exists. Click OK to overwrite, or Cancel to change the filename.');
      if (!choice) {
        // User wants to change filename - suggest new name with (1) appended
        const suggested = suggestNewFilename(destination);
        destination = prompt('Enter new destination path:', suggested);
        if (!destination)
          return;
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
        .then(data =>
                     {
                       if (data.success) {
                         // Task started successfully - polling will handle closing modal when done
                         console.log('[FileServer] Move task started, polling will track progress');
                       } else {
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
    if (!destination)
      return;

    // Check if destination exists
    const exists = await fileExists(destination);
    if (exists) {
      const choice = confirm('File already exists. Click OK to overwrite, or Cancel to change the filename.');
      if (!choice) {
        // User wants to change filename - suggest new name with (1) appended
        const suggested = suggestNewFilename(destination);
        destination = prompt('Enter new destination path:', suggested);
        if (!destination)
          return;
      }
    }

    performMove(source, destination);
  }
  function rename_file(source) {
    const currentName = source.split('/').pop();
    const newName = prompt('Enter new name:', currentName);
    if (!newName || newName === currentName)
      return;

    fetch(API_BASE + '/api/rename', {
      method: 'POST',
      headers: {'Content-Type': 'application/x-www-form-urlencoded'},
      body: 'source=' + encodeURIComponent(source) + '&name=' + encodeURIComponent(newName)
    })
        .then(response => response.json())
        .then(data =>
                     {
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
    if (!name || !name.trim())
      return;

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
        .then(data =>
                     {
                       if (data.success) {
                         alert('Directory created successfully!');
                         location.reload();
                       } else {
                         alert('Create directory failed: ' + (data.error || 'Unknown error'));
                       }
                     })
        .catch(error => alert('Error: ' + error));
  }

  function mount_device(mount_point) {
    if (!confirm('Mount device at ' + mount_point + '?'))
      return;

    fetch(API_BASE + '/api/mount', {
      method: 'POST',
      headers: {'Content-Type': 'application/x-www-form-urlencoded'},
      body: 'mount_point=' + encodeURIComponent(mount_point)
    })
        .then(response => response.json())
        .then(data =>
                     {
                       if (data.success) {
                         alert('Device mounted successfully!');
                         location.reload();
                       } else {
                         alert('Mount failed: ' + (data.error || 'Unknown error'));
                       }
                     })
        .catch(error => alert('Error: ' + error));
  }

  function unmount_device(mount_point) {
    if (!confirm('Unmount device at ' + mount_point + '?'))
      return;

    fetch(API_BASE + '/api/unmount', {
      method: 'POST',
      headers: {'Content-Type': 'application/x-www-form-urlencoded'},
      body: 'mount_point=' + encodeURIComponent(mount_point)
    })
        .then(response => response.json())
        .then(data =>
                     {
                       if (data.success) {
                         alert('Device unmounted successfully!');
                         location.reload();
                       } else {
                         alert('Unmount failed: ' + (data.error || 'Unknown error'));
                       }
                     })
        .catch(error => alert('Error: ' + error));
  }

  function remount_device(mount_point) {
    if (!confirm('Remount device at ' + mount_point + '?'))
      return;

    fetch(API_BASE + '/api/mount', {
      method: 'POST',
      headers: {'Content-Type': 'application/x-www-form-urlencoded'},
      body: 'mount_point=' + encodeURIComponent(mount_point)
    })
        .then(response => response.json())
        .then(data =>
                     {
                       if (data.success) {
                         alert('Device remounted successfully!');
                         location.reload();
                       } else {
                         alert('Remount failed: ' + (data.error || 'Unknown error'));
                       }
                     })
        .catch(error => alert('Error: ' + error));
  }

  async function handleUpload(event) {
    console.log('[FileServer] handleUpload called, event:', event);
    event.preventDefault();

    const fileInput = document.getElementById('uploadFile');
    const file = fileInput.files[0];

    if (!file) {
      alert('Please select a file');
      return false;
    }

    console.log('[FileServer] File selected:', file.name, 'size:', file.size);

    // Show progress modal
    showProgressModal('upload', file.name, window.location.pathname);
    startProgressPolling();

    // Prevent default form submission
    event.stopPropagation();

    // Chunked upload configuration
    const CHUNK_SIZE = 256 * 1024; // 256KB chunks for good balance of speed vs responsiveness
    const totalChunks = Math.ceil(file.size / CHUNK_SIZE);

    console.log('[FileServer] Starting chunked upload: ' + totalChunks + ' chunks of ' + CHUNK_SIZE + ' bytes');

    try {
      // Upload each chunk sequentially
      for (let chunkIndex = 0; chunkIndex < totalChunks; chunkIndex++) {
        const start = chunkIndex * CHUNK_SIZE;
        const end = Math.min(start + CHUNK_SIZE, file.size);
        const chunk = file.slice(start, end);

        // Only log every 50th chunk, first chunk, and last chunk to reduce overhead
        if (chunkIndex % 50 === 0 || chunkIndex === 0 || chunkIndex === totalChunks - 1) {
          console.log('[FileServer] Uploading chunk ' + (chunkIndex + 1) + '/' + totalChunks +
                      ' (bytes ' + start + '-' + end + ')');
        }

        // Build API URL with query parameters
        const apiUrl = API_BASE + '/api/upload_chunk' +
                       '?filename=' + encodeURIComponent(file.name) +
                       '&chunkIndex=' + chunkIndex +
                       '&totalChunks=' + totalChunks +
                       '&path=' + encodeURIComponent(window.location.pathname) +
                       '&fileSize=' + file.size;

        // Add timeout (60 seconds per chunk)
        const controller = new AbortController();
        const timeoutId = setTimeout(() => controller.abort(), 60000);

        try {
          const response = await fetch(apiUrl, {
            method: 'POST',
            body: chunk,
            credentials: 'include',
            headers: {
              'Content-Type': 'application/octet-stream',
              'Connection': 'close'  // Force fresh connection to avoid socket exhaustion
            },
            signal: controller.signal
          });

          clearTimeout(timeoutId);

          if (!response.ok) {
            const errorText = await response.text();
            throw new Error('Chunk ' + (chunkIndex + 1) + ' failed: ' + errorText);
          }

          const result = await response.json();

          // Check if upload was cancelled
          if (result.cancelled) {
            console.log('[FileServer] Upload cancelled by user');
            hideProgressModal();
            alert('Upload cancelled');
            return false;
          }

          if (!result.success) {
            throw new Error('Chunk ' + (chunkIndex + 1) + ' upload failed');
          }

          // Check if server thinks upload is complete
          if (result.complete) {
            if (chunkIndex < totalChunks - 1) {
              // Server says complete but we have more chunks - something went wrong
              throw new Error('Server completed upload early at chunk ' + (chunkIndex + 1) + '/' + totalChunks);
            }
            console.log('[FileServer] Upload completed by server at final chunk');
          }

          // Log progress sparingly to reduce overhead (every 50th chunk, first, and last)
          if (chunkIndex % 50 === 0 || chunkIndex === 0 || chunkIndex === totalChunks - 1) {
            console.log('[FileServer] Chunk ' + (chunkIndex + 1) + '/' + totalChunks + ' uploaded');
          }
        } catch (fetchError) {
          clearTimeout(timeoutId);
          if (fetchError.name === 'AbortError') {
            throw new Error('Chunk ' + (chunkIndex + 1) + ' timed out after 60 seconds');
          }
          throw fetchError;
        }
      }

      console.log('[FileServer] All chunks uploaded successfully');

      // Upload complete - give progress modal a moment to show 100%, then reload
      setTimeout(() => {
        hideProgressModal();
        alert('File uploaded successfully!');
        location.reload();
      }, 500);

    } catch (error) {
      console.error('[FileServer] Upload error:', error);
      hideProgressModal();
      alert('Error: ' + error.message);
    }

    return false;
  }
</script>
</body>
</html>
)HTML";
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
    if (info.is_mount_point && !info.mounted) {
      // Unmounted mount point - show as disabled (not clickable)
      row += "<span class=\"folder unmounted\" style=\"color: #999; cursor: not-allowed;\">" + info.name +
             " (unmounted)</span>";
    } else {
      // Normal directory or mounted mount point
      row += "<a href=\"" + file_uri + "\" class=\"folder\">" + info.name + "</a>";
    }
  } else {
    row += info.name;
  }

  row += "</td><td>";

  if (info.is_directory) {
    if (info.is_mount_point && !info.mounted) {
      row += "<span style=\"color: #999;\">Mount Point</span>";
    } else {
      row += "Folder";
    }
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
    } else {
      // Mount point actions
      if (info.mounted) {
        // Show Remount and Unmount for mounted devices
        row += "<button onclick=\"remount_device('" + info.path + "')\">Remount</button>";
        row += "<button onclick=\"unmount_device('" + info.path + "')\">Unmount</button>";
      } else {
        // Show only Mount for unmounted devices
        row += "<button onclick=\"mount_device('" + info.path + "')\">Mount</button>";
      }
    }
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

  // Upload form (hidden for root directory)
  if (this->upload_enabled_ && !is_virtual_root) {
    html += R"HTML(<div class="upload-form">
      <form id="uploadForm" onsubmit="return handleUpload(event);">
        <input type="file" name="file" id="uploadFile" required>
        <button type="submit">Upload</button>
      </form>
    </div>)HTML";
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
      info.mounted = this->is_mount_point_mounted(mount.path);  // Check mount status
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

  // Get file size
  struct stat file_stat;
  if (stat(filepath.c_str(), &file_stat) != 0) {
    request->send(404, "text/plain", "File not found");
    return;
  }
  size_t file_size = file_stat.st_size;

  // Set content type based on file extension
  std::string mime_type = Path::mime_type(filepath);
  std::string filename = Path::file_name(filepath);
  std::string content_disposition = "attachment; filename=\"" + filename + "\"";

  ESP_LOGI(TAG, "Starting file download: %s (size: %zu bytes)", filename.c_str(), file_size);

  // Use raw httpd API for streaming (AsyncWebServer wraps httpd but doesn't expose streaming)
  // Get the underlying httpd_req_t
  httpd_req_t *req = static_cast<httpd_req_t *>(*request);

  // Open file
  FILE *file = fopen(filepath.c_str(), "rb");
  if (!file) {
    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File not found");
    return;
  }

  // Set response headers using raw httpd API
  httpd_resp_set_type(req, mime_type.c_str());
  httpd_resp_set_hdr(req, "Content-Disposition", content_disposition.c_str());
  httpd_resp_set_hdr(req, "Content-Length", std::to_string(file_size).c_str());
  httpd_resp_set_hdr(req, "Cache-Control", "no-cache");

  // Stream file in chunks
  auto buffer = std::make_unique<uint8_t[]>(FILE_BUFFER_SIZE);
  size_t total_sent = 0;
  bool success = true;

  while (total_sent < file_size) {
    App.feed_wdt();  // Feed watchdog for large files

    size_t to_read = std::min(FILE_BUFFER_SIZE, file_size - total_sent);
    size_t bytes_read = fread(buffer.get(), 1, to_read, file);

    if (bytes_read == 0) {
      ESP_LOGE(TAG, "Failed to read chunk at offset %zu", total_sent);
      success = false;
      break;
    }

    esp_err_t err = httpd_resp_send_chunk(req, reinterpret_cast<const char *>(buffer.get()), bytes_read);
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "Download cancelled or connection closed at %zu / %zu bytes", total_sent, file_size);
      success = false;
      break;
    }

    total_sent += bytes_read;

    // Log progress every ~3MB
    if (file_size > 3 * 1024 * 1024 && (total_sent % (3 * 1024 * 1024) < FILE_BUFFER_SIZE)) {
      ESP_LOGI(TAG, "Download progress: %zu / %zu MB (%.1f%%)", total_sent / (1024 * 1024), file_size / (1024 * 1024),
               (float) total_sent / file_size * 100.0f);
    }
  }

  fclose(file);

  // Send final empty chunk to signal completion
  if (success) {
    httpd_resp_send_chunk(req, nullptr, 0);
    ESP_LOGI(TAG, "Download completed: %zu bytes", total_sent);
  } else {
    ESP_LOGW(TAG, "Download incomplete: %zu / %zu bytes", total_sent, file_size);
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

  // Atomically check and set in_progress flag to prevent concurrent uploads
  portENTER_CRITICAL(&this->progress_mutex_);
  if (this->progress_.in_progress) {
    portEXIT_CRITICAL(&this->progress_mutex_);
    ESP_LOGW(TAG, "Upload rejected: another operation is already in progress (%s)", upload_path.c_str());
    request->send(409, "text/plain", "Another upload/copy/move is already in progress. Please wait.");
    return;
  }
  // Set progress tracking immediately while still in critical section to prevent race
  this->progress_.operation = "upload";
  this->progress_.source = filename.c_str();
  this->progress_.destination = upload_path;
  this->progress_.total_bytes = request->contentLength();
  this->progress_.transferred_bytes = 0;
  this->progress_.in_progress = true;
  this->progress_.cancelled = false;
  this->progress_.start_time = millis();
  portEXIT_CRITICAL(&this->progress_mutex_);

  ESP_LOGI(TAG, "Starting upload: %s (%zu bytes, handler address: %p)", upload_path.c_str(), request->contentLength(),
           (void *) this);

  // Open file for writing
  FILE *file = fopen(upload_path.c_str(), "wb");
  if (!file) {
    ESP_LOGE(TAG, "Failed to open file for writing: %s", upload_path.c_str());
    portENTER_CRITICAL(&this->progress_mutex_);
    this->progress_.in_progress = false;
    portEXIT_CRITICAL(&this->progress_mutex_);
    request->send(500, "text/plain", "Failed to open file for writing");
    return;
  }

  // Get raw httpd_req_t to read POST body
  httpd_req_t *req = static_cast<httpd_req_t *>(*request);
  size_t remaining = request->contentLength();
  // Use architecture-specific buffer size (4KB/8KB/16KB based on ESP32 variant)
  auto buffer = std::make_unique<uint8_t[]>(FILE_BUFFER_SIZE);  // Heap allocation to avoid stack overflow
  bool success = true;

  ESP_LOGI(TAG, "Reading upload data: %zu bytes total", remaining);

  // Read and write in chunks
  size_t chunks_since_yield = 0;
  while (remaining > 0) {
    // Feed the watchdog to prevent timeout on large uploads
    App.feed_wdt();

    size_t to_read = (remaining > FILE_BUFFER_SIZE) ? FILE_BUFFER_SIZE : remaining;
    int received = httpd_req_recv(req, reinterpret_cast<char *>(buffer.get()), to_read);

    if (received <= 0) {
      ESP_LOGE(TAG, "Failed to receive data: %d (errno: %d)", received, errno);
      success = false;
      break;
    }

    size_t written = fwrite(buffer.get(), 1, received, file);
    fflush(file);  // Ensure data is written immediately

    if (written != static_cast<size_t>(received)) {
      ESP_LOGE(TAG, "Failed to write to file: wrote %zu of %d bytes", written, received);
      success = false;
      break;
    }

    // Update progress (thread-safe)
    portENTER_CRITICAL(&this->progress_mutex_);
    this->progress_.transferred_bytes += written;
    size_t current_transferred = this->progress_.transferred_bytes;
    size_t current_total = this->progress_.total_bytes;
    portEXIT_CRITICAL(&this->progress_mutex_);

    remaining -= received;
    chunks_since_yield++;

    // Yield to other tasks every 64 chunks (256KB - 1MB) to allow progress polling
    if (chunks_since_yield >= 64) {
      vTaskDelay(1);  // Brief yield to let other tasks run (including /api/progress requests)
      chunks_since_yield = 0;
    }

    // Log progress every ~50KB
    if (current_transferred % (50 * 1024) < FILE_BUFFER_SIZE) {
      ESP_LOGD(TAG, "Upload progress: %zu / %zu bytes (%.1f%%)", current_transferred, current_total,
               (float) current_transferred / current_total * 100.0f);
    }
  }

  fclose(file);

  // Mark progress as complete (thread-safe)
  portENTER_CRITICAL(&this->progress_mutex_);
  size_t final_transferred = this->progress_.transferred_bytes;
  this->progress_.in_progress = false;
  portEXIT_CRITICAL(&this->progress_mutex_);

  if (success) {
    ESP_LOGI(TAG, "Upload complete: %zu bytes", final_transferred);
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

bool HttpFileServer::recursive_delete_directory(const std::string &path, bool track_progress) {
  DIR *dir = opendir(path.c_str());
  if (!dir) {
    ESP_LOGE(TAG, "Cannot open directory for deletion: %s (errno: %d)", path.c_str(), errno);
    return false;
  }

  // First pass: count the number of files in the directory
  size_t file_count = 0;
  struct dirent *entry;
  while ((entry = readdir(dir)) != nullptr) {
    if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
      file_count++;
    }
  }
  closedir(dir);

  // Determine if we should log each file deletion
  bool log_each_file = (file_count > 5) || track_progress;
  if (log_each_file && !track_progress) {
    ESP_LOGI(TAG, "Deleting directory with %zu files: %s (will log each file)", file_count, path.c_str());
  }

  // Second pass: actually delete the files
  dir = opendir(path.c_str());
  if (!dir) {
    ESP_LOGE(TAG, "Cannot reopen directory for deletion: %s (errno: %d)", path.c_str(), errno);
    return false;
  }

  bool success = true;
  while ((entry = readdir(dir)) != nullptr) {
    // Check for cancellation
    if (track_progress) {
      portENTER_CRITICAL(&this->progress_mutex_);
      bool cancelled = this->progress_.cancelled;
      portEXIT_CRITICAL(&this->progress_mutex_);

      if (cancelled) {
        ESP_LOGI(TAG, "Delete operation cancelled by user");
        closedir(dir);
        return false;
      }
    }

    // Skip . and ..
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
      continue;
    }

    std::string entry_path = Path::join(path, entry->d_name);
    struct stat entry_stat;
    if (stat(entry_path.c_str(), &entry_stat) == 0) {
      if (S_ISDIR(entry_stat.st_mode)) {
        // Recursively delete subdirectory
        if (!this->recursive_delete_directory(entry_path, track_progress)) {
          success = false;
          break;
        }
      } else {
        // Update progress with current file
        if (track_progress) {
          portENTER_CRITICAL(&this->progress_mutex_);
          this->progress_.current_file = entry_path;
          portEXIT_CRITICAL(&this->progress_mutex_);
        }

        // Delete file
        if (log_each_file) {
          ESP_LOGI(TAG, "Deleting file: %s", entry_path.c_str());
        }
        if (remove(entry_path.c_str()) != 0) {
          ESP_LOGE(TAG, "Failed to delete file: %s (errno: %d, %s)", entry_path.c_str(), errno, strerror(errno));
          success = false;
          break;
        }

        // Update progress counter
        if (track_progress) {
          portENTER_CRITICAL(&this->progress_mutex_);
          this->progress_.processed_items++;
          portEXIT_CRITICAL(&this->progress_mutex_);
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

    // Update progress for the directory
    if (track_progress) {
      portENTER_CRITICAL(&this->progress_mutex_);
      this->progress_.processed_items++;
      portEXIT_CRITICAL(&this->progress_mutex_);
    }
  }

  return success;
}

// Mount status helper
bool HttpFileServer::is_mount_point_mounted(const std::string &mount_path) {
  // Check USB MSC devices
#ifdef USE_USB_MSC_HOST
  for (void *dev_ptr : this->usb_msc_devices_) {
    auto *device = static_cast<usb_msc_host::USBMscDevice *>(dev_ptr);
    if (device->get_mount_path() == mount_path) {
      return device->is_mounted();
    }
  }
#endif

  // Check SD MMC devices
#ifdef USE_SD_MMC_CARD
  for (void *dev_ptr : this->sd_mmc_devices_) {
    auto *device = static_cast<sd_mmc_card::SdMmc *>(dev_ptr);
    if (device->get_mount_path() == mount_path) {
      return device->is_mounted();
    }
  }
#endif

  // If no matching device found, assume it's mounted (could be static mount)
  return true;
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

  // Check if another operation is already in progress
  portENTER_CRITICAL(&this->progress_mutex_);
  bool already_in_progress = this->progress_.in_progress;
  std::string current_operation = this->progress_.operation;
  portEXIT_CRITICAL(&this->progress_mutex_);

  if (already_in_progress) {
    ESP_LOGW(TAG, "Cannot start copy: %s operation already in progress", current_operation.c_str());
    request->send(409, "application/json", "{\"error\":\"Another operation is already in progress\"}");
    return;
  }

  // Create task parameters
  // Always track progress for consistent modal behavior (overhead is minimal)
  bool track_progress = true;
  ESP_LOGI(TAG, "Copy: file size %lld bytes, free_heap=%zu", (long long) src_stat.st_size, esp_get_free_heap_size());

  auto *task_params = new CopyTaskParams{this, source, destination, src_stat.st_size, track_progress};

  // Create FreeRTOS task for background copy (4KB stack, priority 1)
  BaseType_t result = xTaskCreate(copy_task, "http_copy", 4096, task_params, 1, nullptr);

  if (result == pdPASS) {
    ESP_LOGI(TAG, "Copy task created successfully");
    request->send(200, "application/json", "{\"success\":true}");
  } else {
    ESP_LOGE(TAG, "Failed to create copy task (heap: %zu)", esp_get_free_heap_size());
    delete task_params;
    request->send(500, "application/json", "{\"error\":\"Failed to start copy operation\"}");
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

  // Check if another operation is already in progress
  portENTER_CRITICAL(&this->progress_mutex_);
  bool already_in_progress = this->progress_.in_progress;
  std::string current_operation = this->progress_.operation;
  portEXIT_CRITICAL(&this->progress_mutex_);

  if (already_in_progress) {
    ESP_LOGW(TAG, "Cannot start move: %s operation already in progress", current_operation.c_str());
    request->send(409, "application/json", "{\"error\":\"Another operation is already in progress\"}");
    return;
  }

  // Create task parameters
  // Always track progress for consistent modal behavior (overhead is minimal)
  bool track_progress = true;

  auto *task_params = new MoveTaskParams{this, source, destination, src_stat.st_size, track_progress};

  // Create FreeRTOS task for background move (4KB stack, priority 1)
  BaseType_t result = xTaskCreate(move_task, "http_move", 4096, task_params, 1, nullptr);

  if (result == pdPASS) {
    ESP_LOGI(TAG, "Move task created successfully");
    request->send(200, "application/json", "{\"success\":true}");
  } else {
    ESP_LOGE(TAG, "Failed to create move task (heap: %zu)", esp_get_free_heap_size());
    delete task_params;
    request->send(500, "application/json", "{\"error\":\"Failed to start move operation\"}");
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
    // Count files/directories first
    int file_count = 0;
    int dir_count = 0;
    this->count_directory_contents(filepath, file_count, dir_count);
    int total_items = file_count + dir_count;

    // Initialize progress tracking for large deletions (>5 items)
    bool track_progress = (total_items > 5);
    if (track_progress) {
      portENTER_CRITICAL(&this->progress_mutex_);
      this->progress_.operation = "delete";
      this->progress_.source = filepath;
      this->progress_.destination = "";
      this->progress_.current_file = "";
      this->progress_.total_items = total_items;
      this->progress_.processed_items = 0;
      this->progress_.in_progress = true;
      this->progress_.cancelled = false;
      this->progress_.start_time = millis();
      portEXIT_CRITICAL(&this->progress_mutex_);
      ESP_LOGI(TAG, "Deleting directory recursively: %s (%d files, %d dirs)", filepath.c_str(), file_count, dir_count);
    } else {
      ESP_LOGI(TAG, "Deleting directory recursively: %s", filepath.c_str());
    }

    // Recursive directory deletion
    success = this->recursive_delete_directory(filepath, track_progress);

    // Clear progress tracking
    if (track_progress) {
      portENTER_CRITICAL(&this->progress_mutex_);
      this->progress_.in_progress = false;
      this->progress_.cancelled = false;  // Reset cancelled flag after handling
      portEXIT_CRITICAL(&this->progress_mutex_);
    }
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
  // Get parameters from POST body
  auto *mount_point_param = request->getParam("mount_point");

  if (!mount_point_param) {
    ESP_LOGW(TAG, "Missing mount_point parameter");
    request->send(400, "application/json", "{\"error\":\"Missing mount_point parameter\"}");
    return;
  }

  std::string mount_point = mount_point_param->value().c_str();
  ESP_LOGI(TAG, "API MOUNT: mount_point=%s (scheduling deferred mount)", mount_point.c_str());

  // Schedule deferred mount to happen in loop() after HTTP response completes
  // This prevents potential memory issues and maintains consistency with unmount behavior
  this->deferred_mount_op_.type = DeferredMountOp::MOUNT;
  this->deferred_mount_op_.mount_point = mount_point;
  this->deferred_mount_op_.schedule_time = millis() + 100;  // 100ms delay

  request->send(200, "application/json", "{\"success\":true}");
  return;
}

void HttpFileServer::handle_api_unmount(AsyncWebServerRequest *request) {
  // Get parameters from POST body
  auto *mount_point_param = request->getParam("mount_point");

  if (!mount_point_param) {
    ESP_LOGW(TAG, "Missing mount_point parameter");
    request->send(400, "application/json", "{\"error\":\"Missing mount_point parameter\"}");
    return;
  }

  std::string mount_point = mount_point_param->value().c_str();
  ESP_LOGI(TAG, "API UNMOUNT: mount_point=%s (scheduling deferred unmount)", mount_point.c_str());

  // Schedule deferred unmount to happen in loop() after HTTP response completes
  // This prevents double-free crashes from unmounting while HTTP request is active
  this->deferred_mount_op_.type = DeferredMountOp::UNMOUNT;
  this->deferred_mount_op_.mount_point = mount_point;
  this->deferred_mount_op_.schedule_time = millis() + 100;  // 100ms delay

  request->send(200, "application/json", "{\"success\":true}");
  // NOTE: The actual unmount happens in loop() - see the loop() method
}

void HttpFileServer::handle_api_remount(AsyncWebServerRequest *request) {
  // Get parameters from POST body
  auto *mount_point_param = request->getParam("mount_point");

  if (!mount_point_param) {
    ESP_LOGW(TAG, "Missing mount_point parameter");
    request->send(400, "application/json", "{\"error\":\"Missing mount_point parameter\"}");
    return;
  }

  std::string mount_point = mount_point_param->value().c_str();
  ESP_LOGI(TAG, "API REMOUNT: mount_point=%s (scheduling deferred remount)", mount_point.c_str());

  // Schedule deferred remount to happen in loop() after HTTP response completes
  // This prevents double-free crashes from unmounting while HTTP request is active
  this->deferred_mount_op_.type = DeferredMountOp::REMOUNT;
  this->deferred_mount_op_.mount_point = mount_point;
  this->deferred_mount_op_.schedule_time = millis() + 100;  // 100ms delay

  request->send(200, "application/json", "{\"success\":true}");
  return;
}

void HttpFileServer::handle_api_progress(AsyncWebServerRequest *request) {
  // Thread-safe snapshot of progress data
  portENTER_CRITICAL(&this->progress_mutex_);
  bool in_progress = this->progress_.in_progress;
  std::string operation = this->progress_.operation;
  std::string source = this->progress_.source;
  std::string destination = this->progress_.destination;
  std::string current_file = this->progress_.current_file;
  size_t total_bytes = this->progress_.total_bytes;
  size_t transferred_bytes = this->progress_.transferred_bytes;
  int total_items = this->progress_.total_items;
  int processed_items = this->progress_.processed_items;
  uint32_t start_time = this->progress_.start_time;

  // Auto-clear stuck operations (timeout after 5 minutes with no progress updates)
  if (in_progress && start_time > 0) {
    uint32_t elapsed_ms = millis() - start_time;
    if (elapsed_ms > 300000) {  // 5 minutes
      ESP_LOGW(TAG, "Auto-clearing stuck operation: %s (elapsed: %u ms)", operation.c_str(), elapsed_ms);
      this->progress_.in_progress = false;
      in_progress = false;
    }
  }
  portEXIT_CRITICAL(&this->progress_mutex_);

  // Build JSON response with progress information
  std::string json = "{";
  json += "\"in_progress\":" + std::string(in_progress ? "true" : "false");

  if (in_progress) {
    json += ",\"operation\":\"" + operation + "\"";
    json += ",\"source\":\"" + source + "\"";
    json += ",\"destination\":\"" + destination + "\"";

    // For delete operations, include item-based progress
    if (operation == "delete") {
      json += ",\"total_items\":" + std::to_string(total_items);
      json += ",\"processed_items\":" + std::to_string(processed_items);
      if (!current_file.empty()) {
        json += ",\"current_file\":\"" + current_file + "\"";
      }

      // Calculate percentage based on items
      float percentage = 0.0;
      if (total_items > 0) {
        percentage = (processed_items * 100.0) / total_items;
      }

      // Format percentage with 1 decimal place
      char percent_buf[16];
      snprintf(percent_buf, sizeof(percent_buf), "%.1f", percentage);
      json += ",\"percentage\":" + std::string(percent_buf);
    } else {
      // For byte-based operations (copy, move, upload)
      json += ",\"total_bytes\":" + std::to_string(total_bytes);
      json += ",\"transferred_bytes\":" + std::to_string(transferred_bytes);

      // Calculate progress percentage
      float percentage = 0.0;
      if (total_bytes > 0) {
        percentage = (transferred_bytes * 100.0) / total_bytes;
      }

      // Format percentage with 1 decimal place
      char percent_buf[16];
      snprintf(percent_buf, sizeof(percent_buf), "%.1f", percentage);
      json += ",\"percentage\":" + std::string(percent_buf);
    }

    // Calculate elapsed time
    uint32_t elapsed_ms = millis() - start_time;
    json += ",\"elapsed_ms\":" + std::to_string(elapsed_ms);

    // Calculate average speed (bytes per second)
    if (elapsed_ms > 0 && transferred_bytes > 0) {
      // Convert to bytes/sec: (transferred_bytes * 1000) / elapsed_ms
      // Use 64-bit arithmetic to avoid overflow
      uint64_t avg_speed = ((uint64_t) transferred_bytes * 1000) / elapsed_ms;
      json += ",\"avg_speed\":" + std::to_string(avg_speed);
    }

    // Estimate remaining time if we have progress (only for byte-based operations)
    if (operation != "delete" && transferred_bytes > 0 && total_bytes > 0) {
      // Use 64-bit arithmetic to avoid overflow when multiplying elapsed_ms * total_bytes
      uint64_t total_estimated_ms = ((uint64_t) elapsed_ms * total_bytes) / transferred_bytes;
      uint64_t remaining_ms = total_estimated_ms > elapsed_ms ? total_estimated_ms - elapsed_ms : 0;
      json += ",\"remaining_ms\":" + std::to_string(remaining_ms);
    }

    // Log progress
    if (operation == "delete") {
      ESP_LOGD(TAG, "Progress poll: delete operation, %d/%d items", processed_items, total_items);
    } else {
      ESP_LOGD(TAG, "Progress poll: %s operation, %zu/%zu bytes", operation.c_str(), transferred_bytes, total_bytes);
    }
  } else {
    ESP_LOGD(TAG, "Progress poll: no operation in progress");
  }

  json += "}";

  ESP_LOGD(TAG, "Sending progress JSON response (%zu bytes): %s", json.length(), json.c_str());
  AsyncWebServerResponse *response = request->beginResponse(200, "application/json", json);

  // CORS headers for XHR with credentials - required even for same-origin requests
  response->addHeader("Access-Control-Allow-Credentials", "true");
  // Get origin from Host header (same-origin case) or reflect Origin header
  auto origin_header = request->get_header("Origin");
  if (origin_header.has_value()) {
    // Reflect the Origin header back for CORS
    response->addHeader("Access-Control-Allow-Origin", origin_header.value().c_str());
  } else {
    // Construct origin from Host header for same-origin requests
    auto host_header = request->get_header("Host");
    if (host_header.has_value()) {
      std::string origin = "http://" + host_header.value();
      response->addHeader("Access-Control-Allow-Origin", origin.c_str());
    }
  }

  request->send(response);
  ESP_LOGD(TAG, "Progress response sent");
}

void HttpFileServer::handle_api_cancel(AsyncWebServerRequest *request) {
  ESP_LOGI(TAG, "Cancel request received");

  // Set cancelled flag (thread-safe)
  portENTER_CRITICAL(&this->progress_mutex_);
  if (this->progress_.in_progress) {
    this->progress_.cancelled = true;
    ESP_LOGI(TAG, "Cancelled %s operation", this->progress_.operation.c_str());
    portEXIT_CRITICAL(&this->progress_mutex_);
    request->send(200, "application/json", "{\"success\":true,\"message\":\"Operation cancelled\"}");
  } else {
    portEXIT_CRITICAL(&this->progress_mutex_);
    request->send(200, "application/json", "{\"success\":false,\"message\":\"No operation in progress\"}");
  }
}

void HttpFileServer::handle_api_upload_chunk(AsyncWebServerRequest *request) {
  if (!this->upload_enabled_) {
    request->send(403, "application/json", "{\"error\":\"Upload is disabled\"}");
    return;
  }

  // Get parameters from query string
  auto *filename_param = request->getParam("filename");
  auto *chunk_index_param = request->getParam("chunkIndex");
  auto *total_chunks_param = request->getParam("totalChunks");
  auto *path_param = request->getParam("path");
  auto *file_size_param = request->getParam("fileSize");

  if (!filename_param || !chunk_index_param || !total_chunks_param || !path_param || !file_size_param) {
    request->send(400, "application/json", "{\"error\":\"Missing required parameters\"}");
    return;
  }

  std::string filename = filename_param->value().c_str();
  int chunk_index = std::stoi(chunk_index_param->value().c_str());
  int total_chunks = std::stoi(total_chunks_param->value().c_str());
  std::string path = path_param->value().c_str();
  size_t file_size = std::stoull(file_size_param->value().c_str());

  // Log only every 50th chunk, first, and last to reduce overhead
  if (chunk_index % 50 == 0 || chunk_index == 0 || chunk_index == total_chunks - 1) {
    ESP_LOGD(TAG, "Upload chunk: file=%s, chunk=%d/%d, path=%s, size=%zu", filename.c_str(), chunk_index, total_chunks,
             path.c_str(), file_size);
  }

  // Convert path to filesystem path
  std::string dir_path = this->uri_to_filepath(path);
  std::string upload_path = Path::join(dir_path, filename);

  // Check if cancelled
  portENTER_CRITICAL(&this->progress_mutex_);
  bool cancelled = this->progress_.cancelled;
  portEXIT_CRITICAL(&this->progress_mutex_);

  if (cancelled) {
    // Clean up
    if (this->upload_file_) {
      fclose(this->upload_file_);
      this->upload_file_ = nullptr;
      remove(upload_path.c_str());
      ESP_LOGI(TAG, "Cleaned up cancelled chunked upload: %s", upload_path.c_str());
    }

    portENTER_CRITICAL(&this->progress_mutex_);
    this->progress_.in_progress = false;
    this->progress_.cancelled = false;
    portEXIT_CRITICAL(&this->progress_mutex_);

    request->send(200, "application/json", "{\"success\":false,\"cancelled\":true}");
    return;
  }

  // First chunk - initialize
  if (chunk_index == 0) {
    // Clean up any stale upload state
    if (this->upload_file_) {
      ESP_LOGW(TAG, "Closing stale upload file from previous chunked upload");
      fclose(this->upload_file_);
      this->upload_file_ = nullptr;
    }

    // Check if target directory exists and is a directory
    struct stat file_stat;
    if (stat(dir_path.c_str(), &file_stat) != 0 || !S_ISDIR(file_stat.st_mode)) {
      ESP_LOGE(TAG, "Upload target is not a directory: %s", dir_path.c_str());
      request->send(400, "application/json", "{\"error\":\"Target is not a directory\"}");
      return;
    }

    // Open file for writing
    this->upload_file_ = fopen(upload_path.c_str(), "wb");
    if (!this->upload_file_) {
      ESP_LOGE(TAG, "Failed to open file for chunked upload: %s", upload_path.c_str());
      request->send(500, "application/json", "{\"error\":\"Failed to open file\"}");
      return;
    }

    // Initialize progress tracking
    portENTER_CRITICAL(&this->progress_mutex_);
    this->progress_.operation = "upload";
    this->progress_.source = filename;
    this->progress_.destination = upload_path;
    this->progress_.total_bytes = file_size;
    this->progress_.transferred_bytes = 0;
    this->progress_.in_progress = true;
    this->progress_.cancelled = false;
    this->progress_.start_time = millis();
    portEXIT_CRITICAL(&this->progress_mutex_);

    this->upload_directory_ = dir_path;
    this->upload_filename_ = filename;

    ESP_LOGI(TAG, "Started chunked upload: %s (%zu bytes, %d chunks)", upload_path.c_str(), file_size, total_chunks);
  }

  // Write chunk data
  if (!this->upload_file_) {
    ESP_LOGE(TAG, "Upload file not open for chunk %d", chunk_index);

    // Clean up progress state (file handle was lost/corrupted)
    portENTER_CRITICAL(&this->progress_mutex_);
    this->progress_.in_progress = false;
    this->progress_.cancelled = false;  // Reset cancelled flag after handling
    portEXIT_CRITICAL(&this->progress_mutex_);

    request->send(500, "application/json", "{\"error\":\"Upload not initialized\"}");
    return;
  }

  size_t bytes_to_write = 0;
  const uint8_t *data_ptr = nullptr;
  std::unique_ptr<uint8_t[]> chunk_buffer;

  // Check if data is in body_buffer_ (form-urlencoded) or needs to be read from request (octet-stream)
  if (!this->body_buffer_.empty()) {
    // Data already in body_buffer_
    data_ptr = reinterpret_cast<const uint8_t *>(this->body_buffer_.data());
    bytes_to_write = this->body_buffer_.size();
    ESP_LOGD(TAG, "Reading chunk %d from body_buffer_: %zu bytes", chunk_index, bytes_to_write);
  } else {
    // body_buffer_ is empty - read directly from httpd_req_t (octet-stream case)
    httpd_req_t *req = *request;  // Convert AsyncWebServerRequest to httpd_req_t
    size_t content_len = req->content_len;

    if (content_len == 0) {
      ESP_LOGW(TAG, "Chunk %d has zero content length", chunk_index);
      // Empty chunk is valid (could be last chunk of a file that's exactly divisible by chunk size)
      bytes_to_write = 0;
    } else {
      ESP_LOGD(TAG, "Reading chunk %d directly from httpd request: %zu bytes", chunk_index, content_len);

      // Allocate buffer for chunk data
      chunk_buffer = std::make_unique<uint8_t[]>(content_len);

      // Read chunk data from HTTP request in a loop (httpd_req_recv may return partial reads)
      size_t total_received = 0;
      while (total_received < content_len) {
        int ret = httpd_req_recv(req, reinterpret_cast<char *>(chunk_buffer.get()) + total_received,
                                 content_len - total_received);
        if (ret <= 0) {
          ESP_LOGE(TAG, "Failed to receive chunk %d data: httpd_req_recv returned %d after %zu/%zu bytes", chunk_index,
                   ret, total_received, content_len);
          fclose(this->upload_file_);
          this->upload_file_ = nullptr;

          if (remove(upload_path.c_str()) == 0) {
            ESP_LOGI(TAG, "Deleted partial upload file after receive failure: %s", upload_path.c_str());
          } else {
            ESP_LOGE(TAG, "Failed to delete partial upload file: %s (errno: %d)", upload_path.c_str(), errno);
          }

          portENTER_CRITICAL(&this->progress_mutex_);
          this->progress_.in_progress = false;
          this->progress_.cancelled = false;
          portEXIT_CRITICAL(&this->progress_mutex_);

          request->send(500, "application/json", "{\"error\":\"Failed to receive chunk data\"}");
          return;
        }
        total_received += ret;
      }

      data_ptr = chunk_buffer.get();
      bytes_to_write = total_received;
    }
  }

  // Write data to file
  if (bytes_to_write > 0) {
    size_t written = fwrite(data_ptr, 1, bytes_to_write, this->upload_file_);
    if (written != bytes_to_write) {
      ESP_LOGE(TAG, "Failed to write chunk %d data: wrote %zu of %zu bytes", chunk_index, written, bytes_to_write);
      fclose(this->upload_file_);
      this->upload_file_ = nullptr;

      // Delete partial file on write failure (analogous to perform_file_copy)
      if (remove(upload_path.c_str()) == 0) {
        ESP_LOGI(TAG, "Deleted partial upload file after write failure: %s", upload_path.c_str());
      } else {
        ESP_LOGE(TAG, "Failed to delete partial upload file: %s (errno: %d)", upload_path.c_str(), errno);
      }

      // Clear progress tracking on failure (analogous to perform_file_copy)
      portENTER_CRITICAL(&this->progress_mutex_);
      this->progress_.in_progress = false;
      this->progress_.cancelled = false;  // Reset cancelled flag after handling
      portEXIT_CRITICAL(&this->progress_mutex_);

      request->send(500, "application/json", "{\"error\":\"Write failed\"}");
      return;
    }

    // Update progress
    portENTER_CRITICAL(&this->progress_mutex_);
    this->progress_.transferred_bytes += written;
    portEXIT_CRITICAL(&this->progress_mutex_);

    // Log only every 50th chunk, first, and last to reduce overhead
    if (chunk_index % 50 == 0 || chunk_index == 0 || chunk_index == total_chunks - 1) {
      ESP_LOGD(TAG, "Wrote chunk %d: %zu bytes (total: %zu/%zu)", chunk_index, written,
               this->progress_.transferred_bytes, file_size);
    }
  }

  // Clear body buffer for next chunk
  this->body_buffer_.clear();

  // Last chunk - finalize
  if (chunk_index == total_chunks - 1) {
    if (this->upload_file_) {
      fclose(this->upload_file_);
      this->upload_file_ = nullptr;
      ESP_LOGI(TAG, "Completed chunked upload: %s (%zu bytes)", upload_path.c_str(),
               this->progress_.transferred_bytes);
    }

    // Mark progress as complete
    portENTER_CRITICAL(&this->progress_mutex_);
    this->progress_.in_progress = false;
    this->progress_.cancelled = false;
    portEXIT_CRITICAL(&this->progress_mutex_);

    request->send(200, "application/json", "{\"success\":true,\"complete\":true}");
  } else {
    // Not the last chunk
    request->send(200, "application/json", "{\"success\":true,\"complete\":false}");
  }
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
  // Format: source=/path/to/file&destination=/path/to/dest&name=newname&path=/file&mount_point=/mnt&device=/dev/sda1
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
      } else if (key == "path") {
        req.path = value;
      } else if (key == "mount_point") {
        req.mount_point = value;
      } else if (key == "device") {
        req.device = value;
      }
    }

    pos = amp_pos + 1;
  }

  return !req.source.empty() || !req.destination.empty() || !req.name.empty() || !req.path.empty() ||
         !req.mount_point.empty() || !req.device.empty();
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

// FreeRTOS task functions for background operations
void HttpFileServer::copy_task(void *params) {
  auto *task_params = static_cast<CopyTaskParams *>(params);
  ESP_LOGI(TAG, "Copy task started for %s -> %s", task_params->source.c_str(), task_params->destination.c_str());

  // Perform the copy operation
  task_params->server->perform_file_copy(task_params->source, task_params->destination, task_params->file_size,
                                         task_params->track_progress);

  ESP_LOGI(TAG, "Copy task completed");

  // Clean up parameters
  delete task_params;

  // Delete this task
  vTaskDelete(nullptr);
}

void HttpFileServer::move_task(void *params) {
  auto *task_params = static_cast<MoveTaskParams *>(params);
  ESP_LOGI(TAG, "Move task started for %s -> %s", task_params->source.c_str(), task_params->destination.c_str());

  // Perform the move operation
  task_params->server->perform_file_move(task_params->source, task_params->destination, task_params->file_size,
                                         task_params->track_progress);

  ESP_LOGI(TAG, "Move task completed");

  // Clean up parameters
  delete task_params;

  // Delete this task
  vTaskDelete(nullptr);
}

void HttpFileServer::download_task(void *params) {
  auto *task_params = static_cast<DownloadTaskParams *>(params);
  httpd_req_t *req = task_params->req;

  ESP_LOGI(TAG, "Download task started for %s (size: %zu bytes)", task_params->filename.c_str(),
           task_params->file_size);

  // Track start time for performance logging
  uint32_t start_time = millis();

  // Open the file
  FILE *file = fopen(task_params->filepath.c_str(), "rb");
  if (!file) {
    ESP_LOGE(TAG, "Failed to open file for download: %s", task_params->filepath.c_str());
    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File not found");
    // Notify caller task if it's waiting
    if (task_params->caller_task) {
      xTaskNotifyGive(task_params->caller_task);
    }
    delete task_params;
    vTaskDelete(nullptr);
    return;
  }

  // Set response headers
  std::string content_disposition = "attachment; filename=\"" + task_params->filename + "\"";
  std::string content_length = std::to_string(task_params->file_size);

  httpd_resp_set_type(req, task_params->mime_type.c_str());
  httpd_resp_set_hdr(req, "Content-Disposition", content_disposition.c_str());
  httpd_resp_set_hdr(req, "Content-Length", content_length.c_str());
  httpd_resp_set_hdr(req, "Cache-Control", "no-cache");

  // Use architecture-specific buffer size (4KB/8KB/16KB based on ESP32 variant)
  auto buffer = std::make_unique<uint8_t[]>(FILE_BUFFER_SIZE);
  size_t total_sent = 0;
  bool success = true;

  while (total_sent < task_params->file_size) {
    App.feed_wdt();  // Feed watchdog for large files

    size_t to_read = std::min(FILE_BUFFER_SIZE, task_params->file_size - total_sent);
    size_t bytes_read = fread(buffer.get(), 1, to_read, file);

    if (bytes_read == 0) {
      ESP_LOGE(TAG, "Failed to read chunk at offset %zu", total_sent);
      success = false;
      break;
    }

    esp_err_t err = httpd_resp_send_chunk(req, reinterpret_cast<const char *>(buffer.get()), bytes_read);
    if (err != ESP_OK) {
      ESP_LOGI(TAG, "Download cancelled or connection closed at %zu / %zu bytes (%s)", total_sent,
               task_params->file_size, esp_err_to_name(err));
      success = false;
      break;
    }

    total_sent += bytes_read;

    // Log progress every ~5MB for less verbose logging
    if (total_sent % (5 * 1024 * 1024) < FILE_BUFFER_SIZE) {
      ESP_LOGI(TAG, "Download progress: %zu / %zu MB (%.1f%%)", total_sent / (1024 * 1024),
               task_params->file_size / (1024 * 1024), (float) total_sent / task_params->file_size * 100.0f);
    }

    // Yield every 4 chunks to balance responsiveness and throughput
    if ((total_sent / FILE_BUFFER_SIZE) % 4 == 0) {
      vTaskDelay(1);  // 1 tick = ~10ms
    }
  }

  fclose(file);

  // Send final empty chunk to signal completion
  if (success) {
    httpd_resp_send_chunk(req, nullptr, 0);
    uint32_t elapsed_ms = millis() - start_time;
    float elapsed_sec = elapsed_ms / 1000.0f;
    float speed_mbps = (total_sent / (1024.0f * 1024.0f)) / (elapsed_sec > 0 ? elapsed_sec : 1.0f);
    ESP_LOGI(TAG, "Download completed: %zu MB in %.1f seconds (%.2f MB/s)", total_sent / (1024 * 1024), elapsed_sec,
             speed_mbps);
  } else {
    ESP_LOGW(TAG, "Download incomplete: %zu / %zu bytes (%.1f%%)", total_sent, task_params->file_size,
             (float) total_sent / task_params->file_size * 100.0f);
  }

  // Notify caller task if it's waiting
  if (task_params->caller_task) {
    xTaskNotifyGive(task_params->caller_task);
  }

  // Clean up parameters
  delete task_params;

  // Delete this task
  vTaskDelete(nullptr);
}

// File operation helpers (reused from WebDAV logic)
bool HttpFileServer::perform_file_copy(const std::string &src_path, const std::string &dst_path, off_t file_size,
                                       bool track_progress) {
  // Initialize progress tracking if requested (only if not already in progress)
  // This allows perform_file_move to set up progress as "move" before calling this function
  if (track_progress) {
    portENTER_CRITICAL(&this->progress_mutex_);
    if (!this->progress_.in_progress) {
      this->progress_.operation = "copy";
      this->progress_.source = src_path;
      this->progress_.destination = dst_path;
      this->progress_.total_bytes = file_size;
      this->progress_.transferred_bytes = 0;
      this->progress_.in_progress = true;
      this->progress_.cancelled = false;
      this->progress_.start_time = millis();
    }
    portEXIT_CRITICAL(&this->progress_mutex_);
  }

  FILE *src = fopen(src_path.c_str(), "rb");
  if (!src) {
    ESP_LOGE(TAG, "Failed to open source file: %s (errno: %d, %s)", src_path.c_str(), errno, strerror(errno));
    if (track_progress) {
      portENTER_CRITICAL(&this->progress_mutex_);
      this->progress_.in_progress = false;
      portEXIT_CRITICAL(&this->progress_mutex_);
    }
    return false;
  }
  FileCloser src_closer(src);  // RAII: will close src on scope exit

  FILE *dst = fopen(dst_path.c_str(), "wb");
  if (!dst) {
    ESP_LOGE(TAG, "Failed to open destination file: %s (errno: %d, %s)", dst_path.c_str(), errno, strerror(errno));
    if (track_progress) {
      portENTER_CRITICAL(&this->progress_mutex_);
      this->progress_.in_progress = false;
      portEXIT_CRITICAL(&this->progress_mutex_);
    }
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
    // Check for cancellation (thread-safe)
    if (track_progress) {
      portENTER_CRITICAL(&this->progress_mutex_);
      bool cancelled = this->progress_.cancelled;
      portEXIT_CRITICAL(&this->progress_mutex_);

      if (cancelled) {
        ESP_LOGI(TAG, "Copy operation cancelled by user at %zu bytes", total_copied);
        copy_success = false;
        break;
      }
    }

    size_t bytes_written = fwrite(buffer.get(), 1, bytes_read, dst);
    if (bytes_written != bytes_read) {
      ESP_LOGE(TAG, "Write failed at offset %zu (errno: %d, %s)", total_copied, errno, strerror(errno));
      copy_success = false;
      break;
    }
    total_copied += bytes_written;

    // Update progress (thread-safe)
    if (track_progress) {
      portENTER_CRITICAL(&this->progress_mutex_);
      this->progress_.transferred_bytes = total_copied;
      portEXIT_CRITICAL(&this->progress_mutex_);
    }

    // Yield to other tasks periodically to allow web server to respond to progress polls
    // Yield every 64 buffers (256KB - 1MB depending on platform)
    if (total_copied % (FILE_BUFFER_SIZE * 64) == 0) {
      vTaskDelay(1);  // Brief yield to let other tasks run
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
    if (track_progress) {
      portENTER_CRITICAL(&this->progress_mutex_);
      if (this->progress_.operation == "copy") {
        this->progress_.in_progress = false;
        this->progress_.cancelled = false;  // Reset cancelled flag after handling
      }
      portEXIT_CRITICAL(&this->progress_mutex_);
    }
    return true;
  } else {
    ESP_LOGE(TAG, "File copy failed: %s to %s (copied %zu of %lld bytes)", src_path.c_str(), dst_path.c_str(),
             total_copied, (long long) file_size);

    // Clean up partial file
    if (remove(dst_path.c_str()) == 0) {
      ESP_LOGI(TAG, "Deleted partial destination file: %s", dst_path.c_str());
    } else {
      ESP_LOGE(TAG, "Failed to remove partial destination file: %s (errno: %d, %s)", dst_path.c_str(), errno,
               strerror(errno));
    }

    // Clear progress tracking on failure (unless perform_file_move will handle it)
    if (track_progress) {
      portENTER_CRITICAL(&this->progress_mutex_);
      if (this->progress_.operation == "copy") {
        this->progress_.in_progress = false;
        this->progress_.cancelled = false;  // Reset cancelled flag after handling
      }
      portEXIT_CRITICAL(&this->progress_mutex_);
    }
    return false;
  }
}

bool HttpFileServer::perform_file_move(const std::string &src_path, const std::string &dst_path, off_t file_size,
                                       bool track_progress) {
  // Initialize progress tracking if requested
  if (track_progress) {
    portENTER_CRITICAL(&this->progress_mutex_);
    this->progress_.operation = "move";
    this->progress_.source = src_path;
    this->progress_.destination = dst_path;
    this->progress_.total_bytes = file_size;
    this->progress_.transferred_bytes = 0;
    this->progress_.in_progress = true;
    this->progress_.cancelled = false;
    this->progress_.start_time = millis();
    portEXIT_CRITICAL(&this->progress_mutex_);
  }

  // Try atomic rename first (works within same filesystem)
  if (rename(src_path.c_str(), dst_path.c_str()) == 0) {
    ESP_LOGI(TAG, "File moved successfully (atomic rename)");
    if (track_progress) {
      portENTER_CRITICAL(&this->progress_mutex_);
      this->progress_.transferred_bytes = file_size;
      this->progress_.in_progress = false;
      this->progress_.cancelled = false;  // Reset cancelled flag after handling
      portEXIT_CRITICAL(&this->progress_mutex_);
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
        if (track_progress) {
          portENTER_CRITICAL(&this->progress_mutex_);
          this->progress_.in_progress = false;
          this->progress_.cancelled = false;  // Reset cancelled flag after handling
          portEXIT_CRITICAL(&this->progress_mutex_);
        }
        return true;
      } else {
        ESP_LOGE(TAG, "Failed to delete source after copy: %s (errno: %d, %s)", src_path.c_str(), errno,
                 strerror(errno));
        // File was copied but source remains - still consider success
        if (track_progress) {
          portENTER_CRITICAL(&this->progress_mutex_);
          this->progress_.in_progress = false;
          this->progress_.cancelled = false;  // Reset cancelled flag after handling
          portEXIT_CRITICAL(&this->progress_mutex_);
        }
        return true;
      }
    } else {
      if (track_progress) {
        portENTER_CRITICAL(&this->progress_mutex_);
        this->progress_.in_progress = false;
        this->progress_.cancelled = false;  // Reset cancelled flag after handling
        portEXIT_CRITICAL(&this->progress_mutex_);
      }
    }
  } else {
    ESP_LOGE(TAG, "Failed to move %s to %s (errno: %d, %s)", src_path.c_str(), dst_path.c_str(), errno,
             strerror(errno));
    if (track_progress) {
      portENTER_CRITICAL(&this->progress_mutex_);
      this->progress_.in_progress = false;
      this->progress_.cancelled = false;  // Reset cancelled flag after handling
      portEXIT_CRITICAL(&this->progress_mutex_);
    }
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
