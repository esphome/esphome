#include "http_request_update.h"

#include "esphome/core/application.h"
#include "esphome/core/version.h"

#include "esphome/components/json/json_util.h"
#include "esphome/components/network/util.h"

namespace esphome {
namespace http_request {

// The update function runs in a task only on ESP32s.
#ifdef USE_ESP32
#define UPDATE_RETURN vTaskDelete(nullptr)  // Delete the current update task
#else
#define UPDATE_RETURN return
#endif

static const char *const TAG = "http_request.update";

static const size_t MAX_READ_SIZE = 256;

void HttpRequestUpdate::setup() {
  this->ota_parent_->add_on_state_callback([this](ota::OTAState state, float progress, uint8_t err) {
    if (state == ota::OTAState::OTA_IN_PROGRESS) {
      this->state_ = update::UPDATE_STATE_INSTALLING;
      this->update_info_.has_progress = true;
      this->update_info_.progress = progress;
      this->publish_state();
    } else if (state == ota::OTAState::OTA_ABORT || state == ota::OTAState::OTA_ERROR) {
      this->state_ = update::UPDATE_STATE_AVAILABLE;
      this->status_set_error("Failed to install firmware");
      this->publish_state();
    }
  });
}

void HttpRequestUpdate::update() {
#ifdef USE_ESP32
  xTaskCreate(HttpRequestUpdate::update_task, "update_task", 8192, (void *) this, 1, &this->update_task_handle_);
#else
  this->update_task(this);
#endif
}

void HttpRequestUpdate::update_task(void *params) {
  HttpRequestUpdate *this_update = (HttpRequestUpdate *) params;

  auto container = this_update->request_parent_->get(this_update->source_url_);

  if (container == nullptr || container->status_code != HTTP_STATUS_OK) {
    std::string msg = str_sprintf("Failed to fetch manifest from %s", this_update->source_url_.c_str());
    this_update->status_set_error(msg.c_str());
    UPDATE_RETURN;
  }

  ExternalRAMAllocator<uint8_t> allocator(ExternalRAMAllocator<uint8_t>::ALLOW_FAILURE);
  uint8_t *data = allocator.allocate(container->content_length);
  if (data == nullptr) {
    std::string msg = str_sprintf("Failed to allocate %d bytes for manifest", container->content_length);
    this_update->status_set_error(msg.c_str());
    container->end();
    UPDATE_RETURN;
  }

  size_t read_index = 0;
  while (container->get_bytes_read() < container->content_length) {
    int read_bytes = container->read(data + read_index, MAX_READ_SIZE);

    yield();

    read_index += read_bytes;
  }

  bool valid = false;
  {  // Ensures the response string falls out of scope and deallocates before the task ends
    std::string response((char *) data, read_index);
    allocator.deallocate(data, container->content_length);

    container->end();
    container.reset();  // Release ownership of the container's shared_ptr

    valid = json::parse_json(response, [this_update](JsonObject root) -> bool {
      if (!root.containsKey("name") || !root.containsKey("version") || !root.containsKey("builds")) {
        ESP_LOGE(TAG, "Manifest does not contain required fields");
        return false;
      }
      this_update->update_info_.title = root["name"].as<std::string>();
      this_update->update_info_.latest_version = root["version"].as<std::string>();

      for (auto build : root["builds"].as<JsonArray>()) {
        if (!build.containsKey("chipFamily")) {
          ESP_LOGE(TAG, "Manifest does not contain required fields");
          return false;
        }
        if (build["chipFamily"] == ESPHOME_VARIANT) {
          if (!build.containsKey("ota")) {
            ESP_LOGE(TAG, "Manifest does not contain required fields");
            return false;
          }
          auto ota = build["ota"];
          if (!ota.containsKey("path") || !ota.containsKey("md5")) {
            ESP_LOGE(TAG, "Manifest does not contain required fields");
            return false;
          }
          this_update->update_info_.firmware_url = ota["path"].as<std::string>();
          this_update->update_info_.md5 = ota["md5"].as<std::string>();

          if (ota.containsKey("summary"))
            this_update->update_info_.summary = ota["summary"].as<std::string>();
          if (ota.containsKey("release_url"))
            this_update->update_info_.release_url = ota["release_url"].as<std::string>();

          return true;
        }
      }
      return false;
    });
  }

  if (!valid) {
    std::string msg = str_sprintf("Failed to parse JSON from %s", this_update->source_url_.c_str());
    this_update->status_set_error(msg.c_str());
    UPDATE_RETURN;
  }

  // Merge source_url_ and this_update->update_info_.firmware_url
  if (this_update->update_info_.firmware_url.find("http") == std::string::npos) {
    std::string path = this_update->update_info_.firmware_url;
    if (path[0] == '/') {
      std::string domain = this_update->source_url_.substr(0, this_update->source_url_.find('/', 8));
      this_update->update_info_.firmware_url = domain + path;
    } else {
      std::string domain = this_update->source_url_.substr(0, this_update->source_url_.rfind('/') + 1);
      this_update->update_info_.firmware_url = domain + path;
    }
  }

  {  // Ensures the current version string falls out of scope and deallocates before the task ends
    std::string current_version;
#ifdef ESPHOME_PROJECT_VERSION
    current_version = ESPHOME_PROJECT_VERSION;
#else
    current_version = ESPHOME_VERSION;
#endif

    this_update->update_info_.current_version = current_version;
  }

  if (this_update->update_info_.latest_version.empty() ||
      this_update->update_info_.latest_version == this_update->update_info_.current_version) {
    this_update->state_ = update::UPDATE_STATE_NO_UPDATE;
  } else {
    this_update->state_ = update::UPDATE_STATE_AVAILABLE;
  }

  this_update->update_info_.has_progress = false;
  this_update->update_info_.progress = 0.0f;

  this_update->status_clear_error();
  this_update->publish_state();

  UPDATE_RETURN;
}

void HttpRequestUpdate::perform(bool force) {
  if (this->state_ != update::UPDATE_STATE_AVAILABLE && !force) {
    return;
  }

  this->state_ = update::UPDATE_STATE_INSTALLING;
  this->publish_state();

  this->ota_parent_->set_md5(this->update_info.md5);
  this->ota_parent_->set_url(this->update_info.firmware_url);
  // Flash in the next loop
  this->defer([this]() { this->ota_parent_->flash(); });
}

}  // namespace http_request
}  // namespace esphome
