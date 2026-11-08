#include "fendt_caravan.h"

#ifdef USE_ESP32

namespace esphome::fendt_caravan {

namespace espbt = esphome::esp32_ble_tracker;

using namespace std;
const uint8_t WAIT_COMMAND = 200;
ESPHOME_LOG_TAG(TAG, "fendt_caravan");
static const char *const SERVICE_UUID = "C7841029-FE7C-4894-8532-F97908EF1AE4";
static constexpr uint16_t CHARACTERISTIC_UUID = 0x0001;
static constexpr char BT_VARS_COMMAND[] = "net-BT_VARS";

void FendtCaravan::loop() {
  if (this->parent()->state() != espbt::ClientState::ESTABLISHED)
    return;

  uint32_t time_stamp = App.get_loop_component_start_time();
  if (this->commands_.empty() || this->wait_handshake_)
    return;

  if ((time_stamp - this->last_command_time_) >= WAIT_COMMAND) {
    Chunk &chunk = this->commands_.front();
    auto status = esp_ble_gattc_write_char(
        this->parent()->get_gattc_if(), this->parent()->get_conn_id(), this->char_handle_, chunk.len,
        reinterpret_cast<uint8_t *>(chunk.data.data()), ESP_GATT_WRITE_TYPE_RSP, ESP_GATT_AUTH_REQ_NONE);
    this->last_command_time_ = time_stamp;
    if (status != ESP_OK) {
      // Keep the chunk queued and retry; a disconnect clears the queue, so this cannot retry forever
      ESP_LOGW(TAG, "Writing command failed (%d), will retry", status);
      this->status_set_warning();
      return;
    }
    this->status_clear_warning();

    ESP_LOGD(TAG, "Command sent: %.*s", chunk.len, chunk.data.data());
    this->commands_.pop();
  }
}

void FendtCaravan::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                       esp_ble_gattc_cb_param_t *param) {
  switch (event) {
    case ESP_GATTC_SEARCH_CMPL_EVT: {
      ESP_LOGV(TAG, "Service discovery complete");
      auto *service = this->parent()->get_service(espbt::ESPBTUUID::from_raw(SERVICE_UUID));
      if (service == nullptr) {
        ESP_LOGW(TAG, "control service not found at device, not a Fendt Caravan..?");
        break;
      }
      auto *chr = service->get_characteristic(CHARACTERISTIC_UUID);
      if (chr == nullptr) {
        ESP_LOGW(TAG, "Control characteristic not found");
        break;
      }
      this->char_handle_ = chr->handle;
      auto status = esp_ble_gattc_register_for_notify(this->parent()->get_gattc_if(), this->parent()->get_remote_bda(),
                                                      chr->handle);
      if (status) {
        ESP_LOGW(TAG, "esp_ble_gattc_register_for_notify failed, status=%d", status);
      }
      auto *descr = chr->get_descriptor(0x2902);
      if (descr == nullptr) {
        ESP_LOGW(TAG, "No CCC descriptor found at service");
        break;
      }
      uint8_t cccd_value[2] = {1, 0};
      status =
          esp_ble_gattc_write_char_descr(this->parent()->get_gattc_if(), this->parent()->get_conn_id(), descr->handle,
                                         2, cccd_value, ESP_GATT_WRITE_TYPE_RSP, ESP_GATT_AUTH_REQ_NONE);
      if (status) {
        ESP_LOGW(TAG, "Error sending CCC descriptor write request, status=%d", status);
        break;
      }
      this->add_command_(this->handshake_, strlen(this->handshake_));
      this->add_command_(BT_VARS_COMMAND, sizeof(BT_VARS_COMMAND) - 1);
      break;
    }
    case ESP_GATTC_DISCONNECT_EVT: {
      this->reset_session_();
      break;
    }
    case ESP_GATTC_NOTIFY_EVT: {
      if (param->notify.handle != this->char_handle_)
        break;
      const FrameResult result = this->rx_.push(param->notify.value, param->notify.value_len);
      if (result == FrameResult::FRAME_RESULT_OVERFLOW) {
        ESP_LOGW(TAG, "Report too long, dropped");
      } else if (result == FrameResult::FRAME_RESULT_COMPLETE) {
        ESP_LOGD(TAG, "Notified value: %s", this->rx_.data());
        this->on_data_received_(this->rx_.data());
        this->rx_.reset();
      }
      break;
    }
    default:
      break;
  }
}

void FendtCaravan::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Fendt Caravan\n"
                "  Fendt Address: %s",
                this->parent()->address_str());
}

void FendtCaravan::send_command(const std::string &command) {
  ESP_LOGV(TAG, "on_command_send called. Command: %s", command.c_str());
  if (this->wait_handshake_)
    return;
  this->add_command_(command.data(), command.size());
}

bool FendtCaravan::add_command_(const char *cmd, size_t len) {
  if (len == 0)
    return false;
  // All chunks of a command are queued or none: a partial command would leave the panel waiting for the rest
  const size_t needed = (len + CHUNK_SIZE - 1) / CHUNK_SIZE;
  if (needed > QUEUE_SIZE - this->commands_.size()) {
    ESP_LOGW(TAG, "Command queue full, dropped: %.*s", static_cast<int>(len), cmd);
    return false;
  }
  for (size_t pos = 0; pos < len; pos += CHUNK_SIZE) {
    Chunk chunk;
    chunk.len = next_chunk(cmd, len, pos, chunk.data.data());
    this->commands_.push(chunk);
  }
  return true;
}

// A link lost mid-session must not leave the next session waiting for a BT_STOP that never comes
void FendtCaravan::reset_session_() {
  this->wait_handshake_ = false;
  this->char_handle_ = 0;
  this->commands_.clear();
  this->rx_.reset();
}

void FendtCaravan::on_data_received_(const char *data) {
  char *value = strchr(data, ':');
  if (value == nullptr) {
    ESP_LOGW(TAG, "Report without a key: %s", data);
    return;
  }
  *value++ = '\0';
  ESP_LOGD(TAG, "Data: '%s', Value: '%s'", data, value);

  if (!strncmp(data, "BT_START", 8)) {
    this->wait_handshake_ = true;
    return;
  }
  if (!strncmp(data, "BT_STOP", 7)) {
    this->wait_handshake_ = false;
    return;
  }

  if (this->mcu_hub_) {
    bool result = this->mcu_hub_->decode(data, value);
    if (result)
      return;
  }
}

}  // namespace esphome::fendt_caravan
#endif
