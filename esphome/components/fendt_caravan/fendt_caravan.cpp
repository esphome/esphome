#include "fendt_caravan.h"

#ifdef USE_ESP32
#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include "main_control_unit_hub.h"

#include <cstring>

namespace esphome::fendt_caravan {

namespace espbt = esphome::esp32_ble_tracker;

ESPHOME_LOG_TAG(TAG, "fendt_caravan");
static const char *const SERVICE_UUID = "C7841029-FE7C-4894-8532-F97908EF1AE4";
static constexpr uint16_t CHARACTERISTIC_UUID = 0x0001;
static constexpr uint32_t WRITE_GAP_MS = 200;        // the panel wants a pause between chunks
static constexpr uint32_t REPORT_TIMEOUT_MS = 1000;  // a report whose next fragment never comes is dropped
static constexpr uint32_t WRITE_TIMEOUT_MS = 2000;   // the BLE event pool drops the response when it is full
static constexpr uint8_t WRITE_RETRIES = 3;

bool FendtCaravan::send_command(const char *command) {
  if (this->node_state == espbt::ClientState::ESTABLISHED && this->queue_.push(command))
    return true;
  ESP_LOGW(TAG, "Command dropped: %s", command);
  return false;
}

void FendtCaravan::loop() {
  if (this->node_state != espbt::ClientState::ESTABLISHED) {
    // A stack restart drops the node to INIT without a disconnect event; the handle set at SEARCH_CMPL is kept
    // while the notification registration is still pending
    if (this->node_state < espbt::ClientState::CONNECTED && this->char_handle_ != 0)
      this->reset_link_();
    return;
  }
  const uint32_t now = App.get_loop_component_start_time();
  if (this->rx_.pending() && now - this->last_fragment_ms_ > REPORT_TIMEOUT_MS) {
    ESP_LOGW(TAG, "Incomplete report dropped");
    this->rx_.reset();
  }
  if (this->write_pending_ && now - this->last_write_ms_ > WRITE_TIMEOUT_MS) {
    // The panel may have taken the chunk, so the command is not sent again
    ESP_LOGW(TAG, "No write response for %s", this->queue_.front());
    this->write_pending_ = false;
    this->pop_command_(true);
  }
  // The write response moves the queue on; a report still arriving is left alone, and the panel needs a
  // pause after each chunk
  if (this->queue_.empty() || this->write_pending_ || this->rx_.pending() || now - this->last_write_ms_ < WRITE_GAP_MS)
    return;
  this->write_chunk_();
}

void FendtCaravan::write_chunk_() {
  const char *cmd = this->queue_.front();
  char chunk[CHUNK_SIZE + 1];
  const size_t n = next_chunk(cmd, strlen(cmd), this->chunk_pos_, chunk);
  this->last_write_ms_ = App.get_loop_component_start_time();
  auto status =
      esp_ble_gattc_write_char(this->parent()->get_gattc_if(), this->parent()->get_conn_id(), this->char_handle_, n,
                               reinterpret_cast<uint8_t *>(chunk), ESP_GATT_WRITE_TYPE_RSP, ESP_GATT_AUTH_REQ_NONE);
  if (status != ESP_OK) {
    ESP_LOGW(TAG, "Write failed: %d", status);
    this->finish_write_(false);
    return;
  }
  ESP_LOGV(TAG, "Wrote %.*s", static_cast<int>(n), chunk);
  this->write_pending_ = true;
}

void FendtCaravan::finish_write_(bool ok) {
  this->write_pending_ = false;
  const char *cmd = this->queue_.front();
  if (ok) {
    this->retries_ = 0;
    this->chunk_pos_ += CHUNK_SIZE;
    if (this->chunk_pos_ >= strlen(cmd)) {
      ESP_LOGD(TAG, "Sent %s", cmd);
      this->pop_command_(true);
    }
    return;
  }
  if (++this->retries_ <= WRITE_RETRIES)
    return;  // the same chunk goes again after the pause
  // Half a command is noise to the panel, so the whole command goes
  ESP_LOGW(TAG, "Giving up on %s", cmd);
  this->pop_command_(false);
}

void FendtCaravan::pop_command_(bool sent) {
  const char *cmd = this->queue_.front();
  this->queue_.pop();
  this->chunk_pos_ = 0;
  this->retries_ = 0;
  this->hub_->on_command_done(cmd, sent);
}

void FendtCaravan::reset_link_() {
  this->write_pending_ = false;
  this->char_handle_ = 0;
  this->queue_.clear();
  this->chunk_pos_ = 0;
  this->retries_ = 0;
  this->rx_.reset();
  this->hub_->on_link_lost();
}

void FendtCaravan::dispatch_(char *report) {
  char *value = strchr(report, ':');
  if (value == nullptr) {
    ESP_LOGW(TAG, "Report without a key: %s", report);
    return;
  }
  *value++ = '\0';
  ESP_LOGD(TAG, "Report %s: %s", report, value);
  this->hub_->handle_report(report, value);
}

void FendtCaravan::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                       esp_ble_gattc_cb_param_t *param) {
  switch (event) {
    case ESP_GATTC_SEARCH_CMPL_EVT: {
      this->reset_link_();  // a stack restart brings a new session without a disconnect event
      auto *service = this->parent()->get_service(espbt::ESPBTUUID::from_raw(SERVICE_UUID));
      if (service == nullptr) {
        ESP_LOGW(TAG, "Control service not found; not a Fendt panel?");
        break;
      }
      auto *chr = service->get_characteristic(CHARACTERISTIC_UUID);
      if (chr == nullptr) {
        ESP_LOGW(TAG, "Control characteristic not found");
        break;
      }
      this->char_handle_ = chr->handle;
      // The client enables notifications on the characteristic once the registration completes
      auto status = this->parent()->register_for_notify(chr->handle);
      if (status != ESP_OK) {
        ESP_LOGW(TAG, "Registering for notifications failed: %d", status);
      }
      break;
    }
    case ESP_GATTC_REG_FOR_NOTIFY_EVT:
      if (param->reg_for_notify.handle != this->char_handle_)
        break;
      if (param->reg_for_notify.status != ESP_GATT_OK) {
        ESP_LOGW(TAG, "Notifications refused: %d", param->reg_for_notify.status);
        break;
      }
      this->node_state = espbt::ClientState::ESTABLISHED;
      // The panel answers the handshake with every variable it has
      this->send_command(this->handshake_);
      this->send_command("net-BT_VARS");
      break;
    case ESP_GATTC_WRITE_CHAR_EVT:
      if (param->write.handle == this->char_handle_ && this->write_pending_)
        this->finish_write_(param->write.status == ESP_GATT_OK);
      break;
    case ESP_GATTC_NOTIFY_EVT: {
      if (param->notify.handle != this->char_handle_)
        break;
      this->last_fragment_ms_ = App.get_loop_component_start_time();
      const FrameResult result = this->rx_.push(param->notify.value, param->notify.value_len);
      if (result == FrameResult::FRAME_RESULT_OVERFLOW) {
        ESP_LOGW(TAG, "Report too long, dropped");
      } else if (result == FrameResult::FRAME_RESULT_COMPLETE) {
        this->dispatch_(this->rx_.data());
        this->rx_.reset();
      }
      break;
    }
    case ESP_GATTC_DISCONNECT_EVT:
    case ESP_GATTC_CLOSE_EVT:
      this->reset_link_();
      break;
    default:
      break;
  }
}

}  // namespace esphome::fendt_caravan
#endif
