#include "esp32_hosted.h"

#ifdef USE_ESP32

#include "esphome/core/log.h"

#include <cerrno>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <esp_hosted.h>

namespace esphome::esp32_hosted {

ESPHOME_LOG_TAG(TAG, "esp32_hosted");

// The initial state task only waits on esp_hosted, unless it ends up running the bring-up itself
static constexpr uint32_t INITIAL_STATE_TASK_STACK_SIZE = 4096;
static constexpr UBaseType_t INITIAL_STATE_TASK_PRIORITY = 1;
// How long the initial state task keeps asking before it leaves the state to the events
static constexpr uint32_t INITIAL_STATE_WAIT_MS = 60000;
static constexpr uint32_t INITIAL_STATE_POLL_MS = 100;

void ESP32Hosted::setup() {
  // esp_hosted creates the default event loop too, possibly not yet
  esp_err_t err = esp_event_loop_create_default();
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    ESP_LOGE(TAG, "Event loop: %s", esp_err_to_name(err));
    this->mark_failed(LOG_STR("Could not create the event loop"));
    return;
  }
  err = esp_event_handler_register(ESP_HOSTED_EVENT, ESP_EVENT_ANY_ID, ESP32Hosted::event_handler_, this);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Event handler: %s", esp_err_to_name(err));
    this->mark_failed(LOG_STR("Could not register for link events"));
    return;
  }
  // The link usually comes up before setup(), so its event is gone. A task finds out without blocking the loop.
  if (xTaskCreate(ESP32Hosted::initial_state_task_, "hosted_link", INITIAL_STATE_TASK_STACK_SIZE, this,
                  INITIAL_STATE_TASK_PRIORITY, nullptr) != pdPASS) {
    ESP_LOGW(TAG, "Could not start the initial state task, waiting for link events");
  }
}

void ESP32Hosted::loop() {
  // Only a link change enables the loop
  this->disable_loop();
  const bool linked = this->link_state_.load(std::memory_order_acquire);
  if (linked == this->linked_)
    return;
  this->linked_ = linked;
  if (linked) {
    ESP_LOGI(TAG, "Link up");
  } else {
    ESP_LOGW(TAG, "Link down");
  }
  this->link_callback_.call(linked);
}

void ESP32Hosted::dump_config() { ESP_LOGCONFIG(TAG, "ESP32 Hosted:\n  Linked: %s", YESNO(this->linked_)); }

void ESP32Hosted::set_link_state_(bool linked) {
  this->link_state_.store(linked, std::memory_order_release);
  this->enable_loop_soon_any_context();
}

void ESP32Hosted::event_handler_(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
  auto *hosted = static_cast<ESP32Hosted *>(arg);
  switch (event_id) {
    case ESP_HOSTED_EVENT_TRANSPORT_UP:
      hosted->set_link_state_(true);
      break;
    // esp_hosted restarts the device after a failure unless that is turned off in its configuration
    case ESP_HOSTED_EVENT_TRANSPORT_FAILURE:
    case ESP_HOSTED_EVENT_TRANSPORT_DOWN:
      hosted->set_link_state_(false);
      break;
    default:
      break;
  }
}

void ESP32Hosted::initial_state_task_(void *arg) {
  auto *hosted = static_cast<ESP32Hosted *>(arg);
  const TickType_t start = xTaskGetTickCount();
  // Returns at once when the link is up and waits while the bring-up runs
  while (true) {
    const int err = esp_hosted_connect_to_slave();  // NOLINT
    if (err == 0) {
      hosted->set_link_state_(true);
      break;
    }
    // esp_hosted is still initialising, or the bring-up took longer than its own wait. Anything else is a failure.
    if ((err != -EINVAL && err != -ETIMEDOUT) || xTaskGetTickCount() - start >= pdMS_TO_TICKS(INITIAL_STATE_WAIT_MS))
      break;
    vTaskDelay(pdMS_TO_TICKS(INITIAL_STATE_POLL_MS));
  }
  vTaskDelete(nullptr);
}

}  // namespace esphome::esp32_hosted

#endif  // USE_ESP32
