#if defined(USE_ESP32_VARIANT_ESP32H2) || defined(USE_ESP32_VARIANT_ESP32P4)
#include "esp32_hosted_update.h"
#include "esphome/components/watchdog/watchdog.h"
#include "esphome/components/sha256/sha256.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include <esp_image_format.h>
#include <esp_app_desc.h>
#include <esp_hosted.h>
#include <esp_hosted_host_fw_ver.h>

#ifdef USE_ESP32_HOSTED_HTTP_UPDATE
#include "esphome/components/json/json_util.h"
#endif

extern "C" {
#include <esp_hosted_ota.h>
}

namespace esphome::esp32_hosted {

static const char *const TAG = "esp32_hosted.update";

// older coprocessor firmware versions have a 1500-byte limit per RPC call
constexpr size_t CHUNK_SIZE = 1500;

// Compile-time version string from esp_hosted_host_fw_ver.h macros
#define STRINGIFY_(x) #x
#define STRINGIFY(x) STRINGIFY_(x)
static const char *const ESP_HOSTED_VERSION_STR = STRINGIFY(ESP_HOSTED_VERSION_MAJOR_1) "." STRINGIFY(
    ESP_HOSTED_VERSION_MINOR_1) "." STRINGIFY(ESP_HOSTED_VERSION_PATCH_1);

#ifdef USE_ESP32_HOSTED_HTTP_UPDATE
// Parse version string "major.minor.patch" into components
// Returns true if parsing succeeded
static bool parse_version(const std::string &version_str, int &major, int &minor, int &patch) {
  major = minor = patch = 0;
  if (sscanf(version_str.c_str(), "%d.%d.%d", &major, &minor, &patch) >= 2) {
    return true;
  }
  return false;
}

// Compare two versions, returns:
//  -1 if v1 < v2
//   0 if v1 == v2
//   1 if v1 > v2
static int compare_versions(int major1, int minor1, int patch1, int major2, int minor2, int patch2) {
  if (major1 != major2)
    return major1 < major2 ? -1 : 1;
  if (minor1 != minor2)
    return minor1 < minor2 ? -1 : 1;
  if (patch1 != patch2)
    return patch1 < patch2 ? -1 : 1;
  return 0;
}
#endif

void Esp32HostedUpdate::setup() {
  this->update_info_.title = "ESP32 Hosted Coprocessor";

  // if wifi is not present, connect to the coprocessor
#ifndef USE_WIFI
  esp_hosted_connect_to_slave();  // NOLINT
#endif

  // get coprocessor version
  esp_hosted_coprocessor_fwver_t ver_info;
  if (esp_hosted_get_coprocessor_fwversion(&ver_info) == ESP_OK) {
    this->update_info_.current_version = str_sprintf("%d.%d.%d", ver_info.major1, ver_info.minor1, ver_info.patch1);
  } else {
    this->update_info_.current_version = "unknown";
  }
  ESP_LOGD(TAG, "Coprocessor version: %s", this->update_info_.current_version.c_str());

#ifndef USE_ESP32_HOSTED_HTTP_UPDATE
  // Embedded mode: get image version from embedded firmware
  const int app_desc_offset = sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t);
  if (this->firmware_size_ >= app_desc_offset + sizeof(esp_app_desc_t)) {
    esp_app_desc_t *app_desc = (esp_app_desc_t *) (this->firmware_data_ + app_desc_offset);
    if (app_desc->magic_word == ESP_APP_DESC_MAGIC_WORD) {
      ESP_LOGD(TAG,
               "Firmware version: %s\n"
               "Project name: %s\n"
               "Build date: %s\n"
               "Build time: %s\n"
               "IDF version: %s",
               app_desc->version, app_desc->project_name, app_desc->date, app_desc->time, app_desc->idf_ver);
      this->update_info_.latest_version = app_desc->version;
      if (this->update_info_.latest_version != this->update_info_.current_version) {
        this->state_ = update::UPDATE_STATE_AVAILABLE;
      } else {
        this->state_ = update::UPDATE_STATE_NO_UPDATE;
      }
    } else {
      ESP_LOGW(TAG, "Invalid app description magic word: 0x%08x (expected 0x%08x)", app_desc->magic_word,
               ESP_APP_DESC_MAGIC_WORD);
      this->state_ = update::UPDATE_STATE_NO_UPDATE;
    }
  } else {
    ESP_LOGW(TAG, "Firmware too small to contain app description");
    this->state_ = update::UPDATE_STATE_NO_UPDATE;
  }

  // publish state
  this->status_clear_error();
  this->publish_state();
#endif
}

void Esp32HostedUpdate::dump_config() {
  ESP_LOGCONFIG(TAG, "ESP32 Hosted Update:");
  ESP_LOGCONFIG(TAG, "  Host Library Version: %s", ESP_HOSTED_VERSION_STR);
  ESP_LOGCONFIG(TAG, "  Coprocessor Version: %s", this->update_info_.current_version.c_str());
  ESP_LOGCONFIG(TAG, "  Latest Version: %s", this->update_info_.latest_version.c_str());
#ifdef USE_ESP32_HOSTED_HTTP_UPDATE
  ESP_LOGCONFIG(TAG, "  Mode: HTTP");
  ESP_LOGCONFIG(TAG, "  Source URL: %s", this->source_url_.c_str());
#else
  ESP_LOGCONFIG(TAG, "  Mode: Embedded");
  ESP_LOGCONFIG(TAG, "  Firmware Size: %zu bytes", this->firmware_size_);
#endif
}

void Esp32HostedUpdate::check() {
#ifdef USE_ESP32_HOSTED_HTTP_UPDATE
  if (!this->fetch_manifest_()) {
    return;
  }

  // Compare versions
  if (this->update_info_.latest_version.empty() ||
      this->update_info_.latest_version == this->update_info_.current_version) {
    this->state_ = update::UPDATE_STATE_NO_UPDATE;
  } else {
    this->state_ = update::UPDATE_STATE_AVAILABLE;
  }

  this->update_info_.has_progress = false;
  this->update_info_.progress = 0.0f;
  this->status_clear_error();
  this->publish_state();
#endif
}

#ifdef USE_ESP32_HOSTED_HTTP_UPDATE
bool Esp32HostedUpdate::fetch_manifest_() {
  ESP_LOGD(TAG, "Fetching manifest from %s", this->source_url_.c_str());

  auto container = this->http_request_parent_->get(this->source_url_);
  if (container == nullptr || container->status_code != 200) {
    ESP_LOGE(TAG, "Failed to fetch manifest from %s", this->source_url_.c_str());
    this->status_set_error(LOG_STR("Failed to fetch manifest"));
    return false;
  }

  // Read manifest JSON into string (manifest is small, ~1KB max)
  std::string json_str;
  json_str.reserve(container->content_length);
  uint8_t buf[256];
  while (container->get_bytes_read() < container->content_length) {
    int read = container->read(buf, sizeof(buf));
    if (read > 0) {
      json_str.append(reinterpret_cast<char *>(buf), read);
    }
    yield();
  }
  container->end();

  // Parse JSON manifest
  // Format: {"versions": [{"version": "2.7.0", "url": "...", "sha256": "..."}]}
  // Only consider versions <= host library version to avoid compatibility issues
  bool valid = json::parse_json(json_str, [this](JsonObject root) -> bool {
    if (!root["versions"].is<JsonArray>()) {
      ESP_LOGE(TAG, "Manifest does not contain 'versions' array");
      return false;
    }

    JsonArray versions = root["versions"].as<JsonArray>();
    if (versions.size() == 0) {
      ESP_LOGE(TAG, "Manifest 'versions' array is empty");
      return false;
    }

    // Find the highest version that is compatible with the host library
    // (version <= host version to avoid upgrading coprocessor ahead of host)
    int best_major = -1, best_minor = -1, best_patch = -1;
    std::string best_version, best_url, best_sha256;

    for (JsonObject entry : versions) {
      if (!entry["version"].is<const char *>() || !entry["url"].is<const char *>() ||
          !entry["sha256"].is<const char *>()) {
        continue;  // Skip malformed entries
      }

      std::string ver_str = entry["version"].as<std::string>();
      int major, minor, patch;
      if (!parse_version(ver_str, major, minor, patch)) {
        ESP_LOGW(TAG, "Failed to parse version: %s", ver_str.c_str());
        continue;
      }

      // Check if this version is compatible (not newer than host)
      if (compare_versions(major, minor, patch, ESP_HOSTED_VERSION_MAJOR_1, ESP_HOSTED_VERSION_MINOR_1,
                           ESP_HOSTED_VERSION_PATCH_1) > 0) {
        ESP_LOGD(TAG, "Skipping version %s (newer than host %s)", ver_str.c_str(), ESP_HOSTED_VERSION_STR);
        continue;
      }

      // Check if this is better than our current best
      if (best_major < 0 || compare_versions(major, minor, patch, best_major, best_minor, best_patch) > 0) {
        best_major = major;
        best_minor = minor;
        best_patch = patch;
        best_version = ver_str;
        best_url = entry["url"].as<std::string>();
        best_sha256 = entry["sha256"].as<std::string>();
      }
    }

    if (best_major < 0) {
      ESP_LOGW(TAG, "No compatible firmware version found (host is %s)", ESP_HOSTED_VERSION_STR);
      return false;
    }

    this->update_info_.latest_version = best_version;
    this->firmware_url_ = best_url;

    // Parse SHA256 hex string to bytes
    if (!parse_hex(best_sha256, this->firmware_sha256_.data(), 32)) {
      ESP_LOGE(TAG, "Invalid SHA256: %s", best_sha256.c_str());
      return false;
    }

    ESP_LOGD(TAG, "Best compatible version: %s", this->update_info_.latest_version.c_str());
    ESP_LOGD(TAG, "Firmware URL: %s", this->firmware_url_.c_str());

    return true;
  });

  if (!valid) {
    ESP_LOGE(TAG, "Failed to parse manifest JSON");
    this->status_set_error(LOG_STR("Failed to parse manifest"));
    return false;
  }

  return true;
}

bool Esp32HostedUpdate::stream_firmware_to_coprocessor_() {
  ESP_LOGI(TAG, "Downloading firmware from %s", this->firmware_url_.c_str());

  auto container = this->http_request_parent_->get(this->firmware_url_);
  if (container == nullptr || container->status_code != 200) {
    ESP_LOGE(TAG, "Failed to fetch firmware");
    this->status_set_error(LOG_STR("Failed to fetch firmware"));
    return false;
  }

  size_t total_size = container->content_length;
  ESP_LOGI(TAG, "Firmware size: %zu bytes", total_size);

  // Begin OTA on coprocessor
  esp_err_t err = esp_hosted_slave_ota_begin();  // NOLINT
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to begin OTA: %s", esp_err_to_name(err));
    container->end();
    this->status_set_error(LOG_STR("Failed to begin OTA"));
    return false;
  }

  // Stream firmware to coprocessor while computing SHA256
  sha256::SHA256 hasher;
  hasher.init();

  uint8_t buffer[CHUNK_SIZE];
  while (container->get_bytes_read() < total_size) {
    int read = container->read(buffer, sizeof(buffer));
    if (read < 0) {
      ESP_LOGE(TAG, "Download failed");
      esp_hosted_slave_ota_end();  // NOLINT
      container->end();
      this->status_set_error(LOG_STR("Download failed"));
      return false;
    }
    if (read > 0) {
      hasher.add(buffer, read);
      err = esp_hosted_slave_ota_write(buffer, read);  // NOLINT
      if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write OTA data: %s", esp_err_to_name(err));
        esp_hosted_slave_ota_end();  // NOLINT
        container->end();
        this->status_set_error(LOG_STR("Failed to write OTA data"));
        return false;
      }
    }
    App.feed_wdt();
    yield();
  }
  container->end();

  // Verify SHA256
  hasher.calculate();
  if (!hasher.equals_bytes(this->firmware_sha256_.data())) {
    ESP_LOGE(TAG, "SHA256 mismatch");
    esp_hosted_slave_ota_end();  // NOLINT
    this->status_set_error(LOG_STR("SHA256 verification failed"));
    return false;
  }

  ESP_LOGI(TAG, "SHA256 verified successfully");
  return true;
}
#endif

void Esp32HostedUpdate::perform(bool force) {
  if (this->state_ != update::UPDATE_STATE_AVAILABLE && !force) {
    ESP_LOGW(TAG, "Update not available");
    return;
  }

  update::UpdateState prev_state = this->state_;
  this->state_ = update::UPDATE_STATE_INSTALLING;
  this->update_info_.has_progress = false;
  this->publish_state();

  watchdog::WatchdogManager watchdog(60000);

#ifdef USE_ESP32_HOSTED_HTTP_UPDATE
  // HTTP mode: stream firmware from URL
  if (!this->stream_firmware_to_coprocessor_()) {
    this->state_ = prev_state;
    this->publish_state();
    return;
  }
#else
  // Embedded mode: use firmware embedded in binary
  if (this->firmware_data_ == nullptr || this->firmware_size_ == 0) {
    ESP_LOGE(TAG, "No firmware data available");
    this->state_ = prev_state;
    this->status_set_error(LOG_STR("No firmware data available"));
    this->publish_state();
    return;
  }

  // ESP32-S3 hardware SHA acceleration requires 32-byte DMA alignment (IDF 5.5.x+)
  alignas(32) sha256::SHA256 hasher;
  hasher.init();
  hasher.add(this->firmware_data_, this->firmware_size_);
  hasher.calculate();
  if (!hasher.equals_bytes(this->firmware_sha256_.data())) {
    this->state_ = prev_state;
    this->status_set_error(LOG_STR("SHA256 verification failed"));
    this->publish_state();
    return;
  }

  ESP_LOGI(TAG, "Starting OTA update (%zu bytes)", this->firmware_size_);

  esp_err_t begin_err = esp_hosted_slave_ota_begin();  // NOLINT
  if (begin_err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to begin OTA: %s", esp_err_to_name(begin_err));
    this->state_ = prev_state;
    this->status_set_error(LOG_STR("Failed to begin OTA"));
    this->publish_state();
    return;
  }

  uint8_t chunk[CHUNK_SIZE];
  const uint8_t *data_ptr = this->firmware_data_;
  size_t remaining = this->firmware_size_;
  while (remaining > 0) {
    size_t chunk_size = std::min(remaining, static_cast<size_t>(CHUNK_SIZE));
    memcpy(chunk, data_ptr, chunk_size);
    esp_err_t write_err = esp_hosted_slave_ota_write(chunk, chunk_size);  // NOLINT
    if (write_err != ESP_OK) {
      ESP_LOGE(TAG, "Failed to write OTA data: %s", esp_err_to_name(write_err));
      esp_hosted_slave_ota_end();  // NOLINT
      this->state_ = prev_state;
      this->status_set_error(LOG_STR("Failed to write OTA data"));
      this->publish_state();
      return;
    }
    data_ptr += chunk_size;
    remaining -= chunk_size;
    App.feed_wdt();
  }
#endif

  // End OTA and activate new firmware
  esp_err_t end_err = esp_hosted_slave_ota_end();  // NOLINT
  if (end_err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to end OTA: %s", esp_err_to_name(end_err));
    this->state_ = prev_state;
    this->status_set_error(LOG_STR("Failed to end OTA"));
    this->publish_state();
    return;
  }

  esp_err_t activate_err = esp_hosted_slave_ota_activate();  // NOLINT
  if (activate_err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to activate OTA: %s", esp_err_to_name(activate_err));
    this->state_ = prev_state;
    this->status_set_error(LOG_STR("Failed to activate OTA"));
    this->publish_state();
    return;
  }

  // Update state
  ESP_LOGI(TAG, "OTA update successful");
  this->state_ = update::UPDATE_STATE_NO_UPDATE;
  this->status_clear_error();
  this->publish_state();

  // Schedule a restart to ensure everything is in sync
  ESP_LOGI(TAG, "Restarting in 1 second");
  this->set_timeout(1000, []() { App.safe_reboot(); });
}

}  // namespace esphome::esp32_hosted
#endif
