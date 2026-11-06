#include "http_request_update.h"

#include <cstring>

#include "esphome/core/application.h"

#include "esphome/components/json/json_util.h"
#include "esphome/components/network/util.h"

namespace esphome::http_request {

// The update function runs in a task only on ESP32s.
#ifdef USE_ESP32
// vTaskDelete doesn't return, but clang-tidy doesn't know that
#define UPDATE_RETURN \
  do { \
    vTaskDelete(nullptr); \
    __builtin_unreachable(); \
  } while (0)
#else
#define UPDATE_RETURN return
#endif

ESPHOME_LOG_TAG(TAG, "http_request.update");

static const size_t MAX_READ_SIZE = 256;
static constexpr uint32_t INITIAL_CHECK_INTERVAL_ID = 0;
static constexpr uint32_t INITIAL_CHECK_INTERVAL_MS = 10000;
static constexpr uint8_t INITIAL_CHECK_MAX_ATTEMPTS = 6;

void HttpRequestUpdate::setup() {
  this->target_->set_update_listener(this);

  // Check periodically until network is ready
  // Only if update interval is > total retry window to avoid redundant checks
  if (this->get_update_interval() != SCHEDULER_DONT_RUN &&
      this->get_update_interval() > INITIAL_CHECK_INTERVAL_MS * INITIAL_CHECK_MAX_ATTEMPTS) {
    this->initial_check_remaining_ = INITIAL_CHECK_MAX_ATTEMPTS;
    this->set_interval(INITIAL_CHECK_INTERVAL_ID, INITIAL_CHECK_INTERVAL_MS, [this]() {
      bool connected = network::is_connected();
      if (--this->initial_check_remaining_ == 0 || connected) {
        this->cancel_interval(INITIAL_CHECK_INTERVAL_ID);
        if (connected) {
          this->update();
        }
      }
    });
  }
}

void HttpRequestUpdate::on_target_state(UpdateTargetState state, float progress, const LogString *message) {
  switch (state) {
    case UpdateTargetState::UPDATE_TARGET_STATE_PROGRESS:
      this->state_ = update::UPDATE_STATE_INSTALLING;
      this->update_info_.has_progress = true;
      this->update_info_.progress = progress;
      break;
    case UpdateTargetState::UPDATE_TARGET_STATE_COMPLETE:
      this->state_ = update::UPDATE_STATE_NO_UPDATE;
      this->update_info_.has_progress = false;
      this->update_info_.current_version = this->update_info_.latest_version;
      this->status_clear_error();
      break;
    case UpdateTargetState::UPDATE_TARGET_STATE_FAILED:
      this->state_ = update::UPDATE_STATE_AVAILABLE;
      this->update_info_.has_progress = false;
      this->status_set_error(message);
      break;
  }
  this->publish_state();
}

void HttpRequestUpdate::set_source_url(const char *source_url, size_t len) {
  // Copy before releasing, so a caller passing the current URL back in stays valid.
  char *copy = new char[len + 1];
  memcpy(copy, source_url, len);
  copy[len] = '\0';
  this->release_source_url_();
  this->source_url_ = copy;
  this->source_url_owned_ = true;
}

void HttpRequestUpdate::release_source_url_() {
  if (this->source_url_owned_)
    delete[] this->source_url_;
  this->source_url_owned_ = false;
}

void HttpRequestUpdate::begin_check_(TaskResult *result) {
  result->update = this;
  result->source_url = this->source_url_;
  result->owns_source_url = this->source_url_owned_;
  this->source_url_owned_ = false;
}

void HttpRequestUpdate::end_check_(TaskResult *result) {
  if (this->source_url_ == result->source_url) {
    this->source_url_owned_ = result->owns_source_url;
  } else if (result->owns_source_url) {
    delete[] result->source_url;  // replaced while the check ran
  }
}

void HttpRequestUpdate::update() {
  // A target that installs asynchronously is still working, and its state and metadata describe
  // the image being installed; a check would replace both.
  if (this->state_ == update::UPDATE_STATE_INSTALLING) {
    ESP_LOGD(TAG, "Install in progress, skipping update check");
    return;
  }
  if (!network::is_connected()) {
    ESP_LOGD(TAG, "Network not connected, skipping update check");
    return;
  }
  this->cancel_interval(INITIAL_CHECK_INTERVAL_ID);
#ifdef USE_ESP32
  if (this->task_result_ != nullptr) {
    ESP_LOGW(TAG, "Update check already in progress");
    return;
  }
#endif
  // Allocated once per check; every path in the task returns via its single defer, which frees it.
  auto *result = new TaskResult();
  this->begin_check_(result);
#ifdef USE_ESP32
  this->task_result_ = result;
  if (xTaskCreate(HttpRequestUpdate::update_task, "update_task", 8192, result, 1, nullptr) != pdPASS) {
    this->task_result_ = nullptr;
    this->end_check_(result);
    delete result;
  }
#else
  this->update_task(result);
  this->end_check_(result);
#endif
}

void HttpRequestUpdate::update_task(void *params) {
  auto *result = static_cast<TaskResult *>(params);
  HttpRequestUpdate *this_update = result->update;
  auto *info = &result->info;
  const char *source_url = result->source_url;

  auto container = this_update->request_parent_->get(source_url);

  if (container == nullptr || container->status_code != HTTP_STATUS_OK) {
    ESP_LOGE(TAG, "Failed to fetch manifest from %s", source_url);
    if (container != nullptr)
      container->end();
    result->error_str = LOG_STR("Failed to fetch manifest");
    goto defer;  // NOLINT(cppcoreguidelines-avoid-goto)
  }

  {
    RAMAllocator<uint8_t> allocator;
    uint8_t *data = allocator.allocate(container->content_length);
    if (data == nullptr) {
      ESP_LOGE(TAG, "Failed to allocate %zu bytes for manifest", container->content_length);
      container->end();
      result->error_str = LOG_STR("Failed to allocate memory for manifest");
      goto defer;  // NOLINT(cppcoreguidelines-avoid-goto)
    }

    auto read_result = http_read_fully(container.get(), data, container->content_length, MAX_READ_SIZE,
                                       this_update->request_parent_->get_timeout());
    if (read_result.status != HttpReadStatus::OK) {
      if (read_result.status == HttpReadStatus::TIMEOUT) {
        ESP_LOGE(TAG, "Timeout reading manifest");
      } else {
        ESP_LOGE(TAG, "Error reading manifest: %d", read_result.error_code);
      }
      allocator.deallocate(data, container->content_length);
      container->end();
      result->error_str = LOG_STR("Failed to read manifest");
      goto defer;  // NOLINT(cppcoreguidelines-avoid-goto)
    }
    size_t read_index = container->get_bytes_read();
    size_t content_length = container->content_length;

    container->end();
    container.reset();  // Release ownership of the container's shared_ptr

    bool valid = false;
    {  // Scope to ensure JsonDocument is destroyed before deallocating buffer
      UpdateTarget *target = this_update->target_;
      valid = json::parse_json(data, read_index, [info, target](JsonObject root) -> bool {
        if (!root[ESPHOME_F("name")].is<const char *>() || !root[ESPHOME_F("version")].is<const char *>() ||
            !root[ESPHOME_F("builds")].is<JsonArray>()) {
          ESP_LOGE(TAG, "Manifest does not contain required fields");
          return false;
        }
        info->title = root[ESPHOME_F("name")].as<std::string>();
        info->latest_version = root[ESPHOME_F("version")].as<std::string>();

        auto builds_array = root[ESPHOME_F("builds")].as<JsonArray>();
        for (auto build : builds_array) {
          if (!build.is<JsonObject>() || !target->matches_build(build.as<JsonObjectConst>()))
            continue;

          JsonVariantConst image = build[target->get_manifest_key()];
          if (!image.is<JsonObjectConst>() || !image[ESPHOME_F("path")].is<const char *>() ||
              !image[ESPHOME_F("md5")].is<const char *>()) {
            ESP_LOGE(TAG, "Manifest does not contain required fields");
            return false;
          }
          info->firmware_url = image[ESPHOME_F("path")].as<std::string>();
          info->md5 = image[ESPHOME_F("md5")].as<std::string>();

          if (image[ESPHOME_F("summary")].is<const char *>())
            info->summary = image[ESPHOME_F("summary")].as<std::string>();
          if (image[ESPHOME_F("release_url")].is<const char *>())
            info->release_url = image[ESPHOME_F("release_url")].as<std::string>();

          return true;
        }
        ESP_LOGE(TAG, "Manifest does not contain a build for this device");
        return false;
      });
    }
    allocator.deallocate(data, content_length);

    if (!valid) {
      ESP_LOGE(TAG, "Failed to parse JSON from %s", source_url);
      result->error_str = LOG_STR("Failed to parse manifest JSON");
      goto defer;  // NOLINT(cppcoreguidelines-avoid-goto)
    }

    // Merge source_url_ and firmware_url
    if (!info->firmware_url.empty() && info->firmware_url.find("http") == std::string::npos) {
      const char *source = source_url;
      const size_t source_len = strlen(source);
      size_t prefix_len;
      if (info->firmware_url[0] == '/') {
        // scheme and host, up to the first slash after "https://"
        const char *host_end = source_len > 8 ? strchr(source + 8, '/') : nullptr;
        prefix_len = host_end != nullptr ? host_end - source : source_len;
      } else {
        // directory of the manifest, up to and including its last slash
        const char *dir_end = strrchr(source, '/');
        prefix_len = dir_end != nullptr ? dir_end - source + 1 : 0;
      }
      info->firmware_url.insert(0, source, prefix_len);
    }

    info->current_version = this_update->target_->get_current_version();
  }

defer:
  // Release container before vTaskDelete (which doesn't call destructors)
  container.reset();

  // Defer to the main loop so all update_info_ and state_ writes happen on the
  // same thread as readers (API, MQTT, web server). This is a single defer for
  // both success and error paths to avoid multiple std::function instantiations.
  // Lambda captures only 2 pointers (8 bytes) — fits in std::function SBO on supported toolchains.
  this_update->defer([this_update, result]() { this_update->finish_check_(result); });

  UPDATE_RETURN;
}

void HttpRequestUpdate::finish_check_(TaskResult *result) {
#ifdef USE_ESP32
  this->task_result_ = nullptr;
  this->end_check_(result);
#endif
  // An install started after this check did, so its state and metadata win.
  if (this->state_ == update::UPDATE_STATE_INSTALLING) {
    ESP_LOGD(TAG, "Install in progress, discarding update check result");
    delete result;
    return;
  }

  if (result->error_str != nullptr) {
    this->status_set_error(result->error_str);
    delete result;
    return;
  }

  bool trigger_update_available = false;
  update::UpdateState new_state;
  if (result->info.latest_version.empty() || result->info.latest_version == result->info.current_version) {
    new_state = update::UPDATE_STATE_NO_UPDATE;
  } else {
    new_state = update::UPDATE_STATE_AVAILABLE;
    if (this->state_ != update::UPDATE_STATE_AVAILABLE) {
      trigger_update_available = true;
    }
  }

  this->update_info_ = std::move(result->info);
  this->state_ = new_state;
  delete result;  // Safe: moved-from state is valid for destruction

  this->status_clear_error();
  this->publish_state();

  if (trigger_update_available) {
    this->get_update_available_trigger()->trigger(this->update_info_);
  }
}

void HttpRequestUpdate::perform(bool force) {
  if (this->state_ != update::UPDATE_STATE_AVAILABLE && !force) {
    return;
  }

  this->state_ = update::UPDATE_STATE_INSTALLING;
  this->publish_state();

  // Install in the next loop
  this->defer([this]() { this->target_->install(this->update_info_.firmware_url, this->update_info_.md5); });
}

}  // namespace esphome::http_request
