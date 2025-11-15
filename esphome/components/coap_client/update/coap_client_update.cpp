#include "coap_client_update.h"

#include "esphome/core/application.h"
#include "esphome/core/version.h"

#include "esphome/components/json/json_util.h"
#include "esphome/components/network/util.h"

namespace esphome::coap {

// The update function runs in a task only on ESP32s.
#ifdef USE_ESP32
#define UPDATE_RETURN vTaskDelete(nullptr)  // Delete the current update task
#else
#define UPDATE_RETURN return
#endif

static const char *const TAG = "coap_client.update";

static const size_t MAX_READ_SIZE = 256;

void CoapClientUpdate::setup() {
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

void CoapClientUpdate::update() {
#ifdef USE_ESP32
  xTaskCreate(CoapClientUpdate::update_task, "update_task", 8192, (void *) this, 1, &this->update_task_handle_);
#else
  this->update_task(this);
#endif
}

void CoapClientUpdate::update_task(void *params) {
  CoapClientUpdate *this_update = (CoapClientUpdate *) params;

  this_update->set_update_response_ready(false);
  ESP_LOGI(TAG, "Get: %s", this_update->source_url_.c_str());
  this_update->request_parent_->get(this_update->source_url_, CoapClientUpdate::update_callback, this_update);
  while (!this_update->is_update_response_ready()) {
    App.feed_wdt();
    delay(10);  // NOLINT
  }

  valid = json::parse_json(this->update_response_, [this_update](JsonObject root) -> bool {
    if (!root["name"].is<const char *>() || !root["version"].is<const char *>() || !root["builds"].is<JsonArray>()) {
      ESP_LOGE(TAG, "Manifest does not contain required fields");
      return false;
    }
    this_update->update_info_.title = root["name"].as<std::string>();
    this_update->update_info_.latest_version = root["version"].as<std::string>();

    for (auto build : root["builds"].as<JsonArray>()) {
      if (!build["chipFamily"].is<const char *>()) {
        ESP_LOGE(TAG, "Manifest does not contain required fields");
        return false;
      }
      if (build["chipFamily"] == ESPHOME_VARIANT) {
        if (!build["ota"].is<JsonObject>()) {
          ESP_LOGE(TAG, "Manifest does not contain required fields");
          return false;
        }
        JsonObject ota = build["ota"].as<JsonObject>();
        if (!ota["path"].is<const char *>() || !ota["md5"].is<const char *>()) {
          ESP_LOGE(TAG, "Manifest does not contain required fields");
          return false;
        }
        this_update->update_info_.firmware_url = ota["path"].as<std::string>();
        this_update->update_info_.md5 = ota["md5"].as<std::string>();

        if (ota["summary"].is<const char *>())
          this_update->update_info_.summary = ota["summary"].as<std::string>();
        if (ota["release_url"].is<const char *>())
          this_update->update_info_.release_url = ota["release_url"].as<std::string>();

        return true;
      }
    }
    return false;
  });

  if (!valid) {
    std::string msg = str_sprintf("Failed to parse JSON from %s", this_update->source_url_.c_str());
    // Defer to main loop to avoid race condition on component_state_ read-modify-write
    this_update->defer([this_update, msg]() { this_update->status_set_error(msg.c_str()); });
    UPDATE_RETURN;
  }

  // Merge source_url_ and this_update->update_info_.firmware_url
  if (this_update->update_info_.firmware_url.find("coap") == std::string::npos) {
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

  bool trigger_update_available = false;

  if (this_update->update_info_.latest_version.empty() ||
      this_update->update_info_.latest_version == this_update->update_info_.current_version) {
    this_update->state_ = update::UPDATE_STATE_NO_UPDATE;
  } else {
    if (this_update->state_ != update::UPDATE_STATE_AVAILABLE) {
      trigger_update_available = true;
    }
    this_update->state_ = update::UPDATE_STATE_AVAILABLE;
  }

  // Defer to main loop to ensure thread-safe execution of:
  // - status_clear_error() performs non-atomic read-modify-write on component_state_
  // - publish_state() triggers API callbacks that write to the shared protobuf buffer
  //   which can be corrupted if accessed concurrently from task and main loop threads
  // - update_available trigger to ensure consistent state when the trigger fires
  this_update->defer([this_update, trigger_update_available]() {
    this_update->update_info_.has_progress = false;
    this_update->update_info_.progress = 0.0f;

    this_update->status_clear_error();
    this_update->publish_state();

    if (trigger_update_available) {
      this_update->get_update_available_trigger()->trigger(this_update->update_info_);
    }
  });

  UPDATE_RETURN;
}

void CoapClientUpdate::perform(bool force) {
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

void CoapClientUpdate::update_callback(uint16_t response_code, const unsigned char *data, size_t len, size_t offset,
                                       size_t total, void *context) {
  ESP_LOGD(TAG, "update_callback %d, %d, %d", len, offset, total);
  CoapClientUpdate *obj = (CoapClientUpdate *) context;
  if (!obj->is_update_response_ready()) {
    if (len == 0 && offset == 0 && total == 0) {
      // Error
      obj->set_update_response_ready(true);
      return;
    } else if (len > 0) {
      obj->append_update_response(data, len);
    }
    if (total > 0 && len + offset == total) {
      obj->set_update_response_ready(true);
    }
  }
}

}  // namespace esphome::coap
