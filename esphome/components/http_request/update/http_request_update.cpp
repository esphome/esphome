#include "http_request_update.h"

#include "esphome/core/application.h"
#include "esphome/core/version.h"

#include "esphome/components/json/json_util.h"
#include "esphome/components/network/util.h"

namespace esphome {
namespace http_request {

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
  auto container = this->request_parent_->get(this->source_url_);

  if (container == nullptr) {
    std::string msg = str_sprintf("Failed to fetch manifest from %s", this->source_url_.c_str());
    this->status_set_error(msg.c_str());
    return;
  }

  ExternalRAMAllocator<uint8_t> allocator(ExternalRAMAllocator<uint8_t>::ALLOW_FAILURE);
  uint8_t *data = allocator.allocate(container->content_length);
  if (data == nullptr) {
    std::string msg = str_sprintf("Failed to allocate %d bytes for manifest", container->content_length);
    this->status_set_error(msg.c_str());
    container->end();
    return;
  }

  size_t read_index = 0;
  while (container->get_bytes_read() < container->content_length) {
    int read_bytes = container->read(data + read_index, MAX_READ_SIZE);

    App.feed_wdt();
    yield();

    read_index += read_bytes;
  }

  std::string response((char *) data, read_index);
  allocator.deallocate(data, container->content_length);

  container->end();

  bool valid = json::parse_json(response, [this](JsonObject root) -> bool {
    if (!root.containsKey("name") || !root.containsKey("version") || !root.containsKey("builds")) {
      ESP_LOGE(TAG, "Manifest does not contain required fields");
      return false;
    }
    this->update_info_.title = root["name"].as<std::string>();
    this->update_info_.latest_version = root["version"].as<std::string>();

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
        this->update_info_.firmware_url = ota["path"].as<std::string>();
        this->update_info_.md5 = ota["md5"].as<std::string>();

        if (ota.containsKey("summary"))
          this->update_info_.summary = ota["summary"].as<std::string>();
        if (ota.containsKey("release_url"))
          this->update_info_.release_url = ota["release_url"].as<std::string>();

        return true;
      }
    }
    return false;
  });

  if (!valid) {
    std::string msg = str_sprintf("Failed to parse JSON from %s", this->source_url_.c_str());
    this->status_set_error(msg.c_str());
    return;
  }

  // Merge source_url_ and this->update_info_.firmware_url
  if (this->update_info_.firmware_url.find("http") == std::string::npos) {
    std::string path = this->update_info_.firmware_url;
    if (path[0] == '/') {
      std::string domain = this->source_url_.substr(0, this->source_url_.find('/', 8));
      this->update_info_.firmware_url = domain + path;
    } else {
      std::string domain = this->source_url_.substr(0, this->source_url_.rfind('/') + 1);
      this->update_info_.firmware_url = domain + path;
    }
  }

  std::string current_version = this->current_version_;
  if (current_version.empty()) {
#ifdef ESPHOME_PROJECT_VERSION
    current_version = ESPHOME_PROJECT_VERSION;
#else
    current_version = ESPHOME_VERSION;
#endif
  }
  this->update_info_.current_version = current_version;

  if (this->update_info_.latest_version.empty()) {
    this->state_ = update::UPDATE_STATE_NO_UPDATE;
  } else if (this->update_info_.latest_version != this->current_version_) {
    this->state_ = update::UPDATE_STATE_AVAILABLE;
  }

  this->update_info_.has_progress = false;
  this->update_info_.progress = 0.0f;

  this->status_clear_error();
  this->publish_state();
}

void HttpRequestUpdate::perform() {
  if (this->state_ != update::UPDATE_STATE_AVAILABLE) {
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
