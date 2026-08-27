#include "http_request_update.h"

#include "esphome/core/application.h"
#include "esphome/core/version.h"

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

static const char *const TAG = "http_request.update";

// Wraps UpdateInfo + error for the task→main-loop handoff.
struct TaskResult {
  update::UpdateInfo info;
  const LogString *error_str{nullptr};
};

static const size_t MAX_READ_SIZE = 256;
static constexpr uint32_t INITIAL_CHECK_INTERVAL_ID = 0;
static constexpr uint32_t INITIAL_CHECK_INTERVAL_MS = 10000;
static constexpr uint8_t INITIAL_CHECK_MAX_ATTEMPTS = 6;

void HttpRequestUpdate::setup() {
  this->ota_parent_->add_state_listener(this);

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

void HttpRequestUpdate::on_ota_state(ota::OTAState state, float progress, uint8_t error) {
  if (state == ota::OTAState::OTA_IN_PROGRESS) {
    this->state_ = update::UPDATE_STATE_INSTALLING;
    this->update_info_.has_progress = true;
    this->update_info_.progress = progress;
    this->publish_state();
  } else if (state == ota::OTAState::OTA_ABORT || state == ota::OTAState::OTA_ERROR) {
    this->state_ = update::UPDATE_STATE_AVAILABLE;
    this->status_set_error(LOG_STR("Failed to install firmware"));
    this->publish_state();
  }
}

void HttpRequestUpdate::update() {
  if (!network::is_connected()) {
    ESP_LOGD(TAG, "Network not connected, skipping update check");
    return;
  }
  this->cancel_interval(INITIAL_CHECK_INTERVAL_ID);
#ifdef USE_ESP32
  if (this->update_task_handle_ != nullptr) {
    ESP_LOGW(TAG, "Update check already in progress");
    return;
  }
  xTaskCreate(HttpRequestUpdate::update_task, "update_task", 8192, (void *) this, 1, &this->update_task_handle_);
#else
  this->update_task(this);
#endif
}

void HttpRequestUpdate::update_task(void *params) {
  HttpRequestUpdate *this_update = (HttpRequestUpdate *) params;

  // Allocate once — every path below returns via the single defer at the end.
  // On failure, error_str is set; on success it is nullptr.
  auto *result = new TaskResult();
  auto *info = &result->info;

  auto container = this_update->request_parent_->get(this_update->source_url_);

  if (container == nullptr || container->status_code != HTTP_STATUS_OK) {
    ESP_LOGE(TAG, "Failed to fetch manifest from %s", this_update->source_url_.c_str());
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
      valid = json::parse_json(data, read_index, [info](JsonObject root) -> bool {
        if (!root[ESPHOME_F("name")].is<const char *>() || !root[ESPHOME_F("version")].is<const char *>() ||
            !root[ESPHOME_F("builds")].is<JsonArray>()) {
          ESP_LOGE(TAG, "Manifest does not contain required fields");
          return false;
        }
        info->title = root[ESPHOME_F("name")].as<std::string>();
        info->latest_version = root[ESPHOME_F("version")].as<std::string>();

        auto builds_array = root[ESPHOME_F("builds")].as<JsonArray>();
        for (auto build : builds_array) {
          if (!build[ESPHOME_F("chipFamily")].is<const char *>()) {
            ESP_LOGE(TAG, "Manifest does not contain required fields");
            return false;
          }
          if (build[ESPHOME_F("chipFamily")] == ESPHOME_VARIANT) {
            if (!build[ESPHOME_F("ota")].is<JsonObject>()) {
              ESP_LOGE(TAG, "Manifest does not contain required fields");
              return false;
            }
            JsonObject ota = build[ESPHOME_F("ota")].as<JsonObject>();
            if (!ota[ESPHOME_F("path")].is<const char *>() || !ota[ESPHOME_F("md5")].is<const char *>()) {
              ESP_LOGE(TAG, "Manifest does not contain required fields");
              return false;
            }
            info->firmware_url = ota[ESPHOME_F("path")].as<std::string>();
            info->md5 = ota[ESPHOME_F("md5")].as<std::string>();

            if (ota[ESPHOME_F("summary")].is<const char *>())
              info->summary = ota[ESPHOME_F("summary")].as<std::string>();
            if (ota[ESPHOME_F("release_url")].is<const char *>())
              info->release_url = ota[ESPHOME_F("release_url")].as<std::string>();

            return true;
          }
        }
        return false;
      });
    }
    allocator.deallocate(data, content_length);

    if (!valid) {
      ESP_LOGE(TAG, "Failed to parse JSON from %s", this_update->source_url_.c_str());
      result->error_str = LOG_STR("Failed to parse manifest JSON");
      goto defer;  // NOLINT(cppcoreguidelines-avoid-goto)
    }

    // Merge source_url_ and firmware_url
    if (!info->firmware_url.empty() && info->firmware_url.find("http") == std::string::npos) {
      std::string path = info->firmware_url;
      if (path[0] == '/') {
        std::string domain = this_update->source_url_.substr(0, this_update->source_url_.find('/', 8));
        info->firmware_url = domain + path;
      } else {
        std::string domain = this_update->source_url_.substr(0, this_update->source_url_.rfind('/') + 1);
        info->firmware_url = domain + path;
      }
    }

#ifdef ESPHOME_PROJECT_VERSION
    info->current_version = ESPHOME_PROJECT_VERSION;
#else
    info->current_version = ESPHOME_VERSION;
#endif
  }

defer:
  // Release container before vTaskDelete (which doesn't call destructors)
  container.reset();

  // Defer to the main loop so all update_info_ and state_ writes happen on the
  // same thread as readers (API, MQTT, web server). This is a single defer for
  // both success and error paths to avoid multiple std::function instantiations.
  // Lambda captures only 2 pointers (8 bytes) — fits in std::function SBO on supported toolchains.
  this_update->defer([this_update, result]() {
#ifdef USE_ESP32
    this_update->update_task_handle_ = nullptr;
#endif
    if (result->error_str != nullptr) {
      this_update->status_set_error(result->error_str);
      delete result;
      return;
    }

    // Determine new state on main loop (avoids extra lambda captures from task)
    bool trigger_update_available = false;
    update::UpdateState new_state;
    if (result->info.latest_version.empty() || result->info.latest_version == result->info.current_version) {
      new_state = update::UPDATE_STATE_NO_UPDATE;
    } else {
      new_state = update::UPDATE_STATE_AVAILABLE;
      if (this_update->state_ != update::UPDATE_STATE_AVAILABLE) {
        trigger_update_available = true;
      }
    }

    this_update->update_info_ = std::move(result->info);
    this_update->state_ = new_state;
    delete result;  // Safe: moved-from state is valid for destruction

    this_update->status_clear_error();
    this_update->publish_state();

    if (trigger_update_available) {
      this_update->get_update_available_trigger()->trigger(this_update->update_info_);
    }
  });

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

}  // namespace esphome::http_request
