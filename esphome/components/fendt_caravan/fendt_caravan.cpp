#include "fendt_caravan.h"

#ifdef USE_ESP32

namespace esphome::fendt_caravan {

namespace espbt = esphome::esp32_ble_tracker;

using namespace std;
const uint8_t WAIT_COMMAND = 200;
ESPHOME_LOG_TAG(TAG, "fendt_caravan");
static const char *const SERVICE_UUID = "C7841029-FE7C-4894-8532-F97908EF1AE4";
static constexpr uint16_t CHARACTERISTIC_UUID = 0x0001;

void FendtCaravan::loop() {
  if (this->parent()->state() != espbt::ClientState::ESTABLISHED)
    return;

  uint32_t time_stamp = App.get_loop_component_start_time();
  if (this->commands_.empty())
    return;

  if ((time_stamp - this->last_command_time_) >= WAIT_COMMAND && !this->wait_buffer_) {
    std::string cmd = this->commands_.front();
    auto status = esp_ble_gattc_write_char(this->parent()->get_gattc_if(), this->parent()->get_conn_id(),
                                           this->char_handle_, cmd.size(), reinterpret_cast<uint8_t *>(cmd.data()),
                                           ESP_GATT_WRITE_TYPE_RSP, ESP_GATT_AUTH_REQ_NONE);
    if (status) {
      ESP_LOGE(TAG, "Writing command failed (%d)", status);
    }

    ESP_LOGD(TAG, "Command sent: %s", cmd.c_str());
    this->last_command_time_ = time_stamp;
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

      this->add_command_("net-BT_ID-c0:ee:fb:90:b0:a7");
      this->add_command_("net-BT_VARS");
      break;
    }
    case ESP_GATTC_NOTIFY_EVT: {
      if (param->notify.handle != this->char_handle_)
        break;
      this->wait_buffer_ = true;
      const FrameResult result = this->rx_.push(param->notify.value, param->notify.value_len);
      if (result == FrameResult::FRAME_RESULT_OVERFLOW) {
        ESP_LOGW(TAG, "Report too long, dropped");
      } else if (result == FrameResult::FRAME_RESULT_COMPLETE) {
        ESP_LOGD(TAG, "Notified value: %s", this->rx_.data());
        this->on_data_received_(this->rx_.data());
        this->rx_.reset();
      }
      this->wait_buffer_ = false;
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
  if (command.empty())
    return;
  this->add_command_(command);
}

void FendtCaravan::add_command_(const std::string &cmd) {
  size_t total_length = cmd.length();
  size_t start_index = 0;
  const size_t chunk_size = 17;

  while (start_index < total_length) {
    size_t remaining = total_length - start_index;
    std::string chunk;

    if (remaining > chunk_size) {
      chunk = cmd.substr(start_index, chunk_size) + "@";
      start_index += chunk_size;
    } else {
      chunk = cmd.substr(start_index, remaining);
      start_index += remaining;
    }
    this->commands_.push(chunk);
  }
}

void FendtCaravan::on_data_received_(const char *data) {
  char *value = strchr(data, ':');
  if (value == nullptr) {
    ESP_LOGW(TAG, "Report without a key: %s", data);
    return;
  }
  *value++ = '\0';
  ESP_LOGD(TAG, "Data: '%s', Value: '%s'", data, value);

  if (this->mcu_hub_) {
    bool result = this->mcu_hub_->decode(data, value);
    if (result)
      return;
  }
  if (this->alde_unit_hub_) {
    bool result = this->alde_unit_hub_->decode(data, value);
    if (result)
      return;
  }
  if (this->lighting_unit_hub_) {
    bool result = this->lighting_unit_hub_->decode(data, value);
    if (result)
      return;
  }
  if (this->fridge_unit_hub_) {
    bool result = this->fridge_unit_hub_->decode(data, value);
    if (result)
      return;
  }
}

}  // namespace esphome::fendt_caravan
#endif
