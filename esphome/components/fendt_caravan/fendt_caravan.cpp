#include "fendt_caravan.h"

#ifdef USE_ESP32

namespace esphome {
namespace fendt_caravan {

namespace espbt = esphome::esp32_ble_tracker;

using namespace std;
const uint8_t WIAT_COMMAND = 200;
static const char *TAG = "FC";

void FendtCaravan::dump_config() { ESP_LOGCONFIG(TAG, "Fend Caravan Log"); }

void FendtCaravan::setup() { ESP_LOGD(TAG, "Setup called"); }

void FendtCaravan::loop() {
  if (!command_enabled_)
    return;

  if (this->parent()->state() != espbt::ClientState::ESTABLISHED)
    return;

  uint32_t time_stamp = millis();
  if (!this->commands_.empty() && (time_stamp - this->last_command_time_) >= WIAT_COMMAND && !this->wait_buffer_) {
    std::string cmd = this->commands_.at(0);

    uint8_t buffer[cmd.length() + 1];
    memset(buffer, 0, sizeof(buffer));
    memcpy(buffer, cmd.c_str(), cmd.length());
    auto status =
        esp_ble_gattc_write_char(this->parent()->get_gattc_if(), this->parent()->get_conn_id(), this->char_handle_,
                                 cmd.length(), buffer, ESP_GATT_WRITE_TYPE_RSP, ESP_GATT_AUTH_REQ_NONE);
    if (status) {
      ESP_LOGE(TAG, "Error writing command to device, error = %d", status);
    }

    ESP_LOGD(TAG, "Command sent: %s, %d", buffer, cmd.length());
    this->last_command_time_ = time_stamp;
    this->commands_.erase(std::remove(this->commands_.begin(), this->commands_.end(), cmd), this->commands_.end());
  }
}

void FendtCaravan::update() {}

void FendtCaravan::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                       esp_ble_gattc_cb_param_t *param) {
  switch (event) {
    case ESP_GATTC_REG_EVT:
      ESP_LOGV(TAG, "BLE ESP_GATTC_UNREG_EVT called");
      break;
    case ESP_GATTC_UNREG_EVT:
      ESP_LOGV(TAG, "BLE ESP_GATTC_UNREG_EVT called");
      break;
    case ESP_GATTC_OPEN_EVT:
      ESP_LOGV(TAG, "BLE connected (OPEN_EVT)");
      break;
    case ESP_GATTC_READ_CHAR_EVT:
      ESP_LOGV(TAG, "BLE ESP_GATTC_READ_CHAR_EVT called");
      break;
    case ESP_GATTC_WRITE_CHAR_EVT:
      ESP_LOGV(TAG, "BLE ESP_GATTC_WRITE_CHAR_EVT called");
      break;
    case ESP_GATTC_CLOSE_EVT:
      ESP_LOGV(TAG, "BLE ESP_GATTC_CLOSE_EVT called");
      break;
    case ESP_GATTC_SEARCH_CMPL_EVT: {
      ESP_LOGV(TAG, "Service discovery complete");

      auto *service = this->parent()->get_service(espbt::ESPBTUUID::from_raw(service_uuid_));
      if (service == nullptr) {
        ESP_LOGW(TAG, "control service not found at device, not a Fendt Caravan..?");
        break;
      }
      auto *chr = service->get_characteristic(characteristic_uuid_);
      if (chr == nullptr) {
        ESP_LOGW(TAG, "control characteristic not found at device, not a Fendt Caravan..?");
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
        ;
      }

      this->add_command_("net-BT_ID-c0:ee:fb:90:b0:a7");
      this->add_command_("net-BT_VARS");
      this->command_enabled_ = true;
      break;
    }
    case ESP_GATTC_SEARCH_RES_EVT:
      ESP_LOGV(TAG, "BLE ESP_GATTC_SEARCH_RES_EVT called");
      break;
    case ESP_GATTC_READ_DESCR_EVT:
      ESP_LOGV(TAG, "BLE ESP_GATTC_READ_DESCR_EVT called");
      break;
    case ESP_GATTC_WRITE_DESCR_EVT:
      ESP_LOGV(TAG, "BLE ESP_GATTC_WRITE_DESCR_EVT called");
      break;
    case ESP_GATTC_NOTIFY_EVT:
      if (param->notify.handle == this->char_handle_) {
        this->wait_buffer_ = true;
        char buffer[param->notify.value_len + 1];
        memset(buffer, 0, param->notify.value_len + 1);
        memcpy(buffer, param->notify.value, param->notify.value_len);
        std::string result = std::string(buffer);
        if (!this->last_response_.empty()) {
          result = this->last_response_ + result;
          this->last_response_.clear();
        }
        if (strchr(buffer, '@') != nullptr) {
          this->last_response_.append(buffer, param->notify.value_len - 1);
          break;
        }
        this->wait_buffer_ = false;
        ESP_LOGD(TAG, "Notified value: %s", result.c_str());
        on_data_received_(result);
      }
      break;
    case ESP_GATTC_PREP_WRITE_EVT:
      ESP_LOGV(TAG, "BLE ESP_GATTC_PREP_WRITE_EVT called");
      break;
    case ESP_GATTC_EXEC_EVT:
      ESP_LOGV(TAG, "BLE ESP_GATTC_EXEC_EVT called");
      break;
    case ESP_GATTC_ACL_EVT:
      ESP_LOGV(TAG, "BLE ESP_GATTC_ACL_EVT called");
      break;
    case ESP_GATTC_CANCEL_OPEN_EVT:
      ESP_LOGV(TAG, "BLE ESP_GATTC_CANCEL_OPEN_EVT called");
      break;
    case ESP_GATTC_SRVC_CHG_EVT:
      ESP_LOGV(TAG, "BLE ESP_GATTC_SRVC_CHG_EVT called");
      break;
    case ESP_GATTC_ENC_CMPL_CB_EVT:
      ESP_LOGV(TAG, "BLE ESP_GATTC_ENC_CMPL_CB_EVT called");
      break;
    case ESP_GATTC_CFG_MTU_EVT:
      ESP_LOGV(TAG, "BLE ESP_GATTC_CFG_MTU_EVT called");
      break;
    case ESP_GATTC_ADV_DATA_EVT:
      ESP_LOGV(TAG, "BLE ESP_GATTC_ADV_DATA_EVT called");
      break;
    case ESP_GATTC_MULT_ADV_ENB_EVT:
      ESP_LOGV(TAG, "BLE ESP_GATTC_MULT_ADV_ENB_EVT called");
      break;
    case ESP_GATTC_MULT_ADV_UPD_EVT:
      ESP_LOGV(TAG, "BLE ESP_GATTC_MULT_ADV_UPD_EVT called");
      break;
    case ESP_GATTC_MULT_ADV_DATA_EVT:
      ESP_LOGV(TAG, "BLE ESP_GATTC_MULT_ADV_DATA_EVT called");
      break;
    case ESP_GATTC_MULT_ADV_DIS_EVT:
      ESP_LOGV(TAG, "BLE ESP_GATTC_MULT_ADV_DIS_EVT called");
      break;
    case ESP_GATTC_CONGEST_EVT:
      ESP_LOGV(TAG, "BLE ESP_GATTC_CONGEST_EVT called");
      break;
    case ESP_GATTC_BTH_SCAN_ENB_EVT:
      ESP_LOGV(TAG, "BLE ESP_GATTC_BTH_SCAN_ENB_EVT called");
      break;
    case ESP_GATTC_BTH_SCAN_CFG_EVT:
      ESP_LOGV(TAG, "BLE ESP_GATTC_BTH_SCAN_CFG_EVT called");
      break;
    case ESP_GATTC_BTH_SCAN_RD_EVT:
      ESP_LOGV(TAG, "BLE ESP_GATTC_BTH_SCAN_RD_EVT called");
      break;
    case ESP_GATTC_BTH_SCAN_THR_EVT:
      ESP_LOGV(TAG, "BLE ESP_GATTC_BTH_SCAN_THR_EVT called");
      break;
    case ESP_GATTC_BTH_SCAN_PARAM_EVT:
      ESP_LOGV(TAG, "BLE ESP_GATTC_BTH_SCAN_PARAM_EVT called");
      break;
    case ESP_GATTC_BTH_SCAN_DIS_EVT:
      ESP_LOGV(TAG, "BLE ESP_GATTC_BTH_SCAN_DIS_EVT called");
      break;
    case ESP_GATTC_SCAN_FLT_CFG_EVT:
      ESP_LOGV(TAG, "BLE ESP_GATTC_SCAN_FLT_CFG_EVT called");
      break;
    case ESP_GATTC_SCAN_FLT_PARAM_EVT:
      ESP_LOGV(TAG, "BLE ESP_GATTC_SCAN_FLT_PARAM_EVT called");
      break;
    case ESP_GATTC_SCAN_FLT_STATUS_EVT:
      ESP_LOGV(TAG, "BLE ESP_GATTC_SCAN_FLT_STATUS_EVT called");
      break;
    case ESP_GATTC_ADV_VSC_EVT:
      ESP_LOGV(TAG, "BLE ESP_GATTC_ADV_VSC_EVT called");
      break;
    case ESP_GATTC_UNREG_FOR_NOTIFY_EVT:
      ESP_LOGV(TAG, "BLE ESP_GATTC_UNREG_FOR_NOTIFY_EVT called");
      break;
    case ESP_GATTC_REG_FOR_NOTIFY_EVT:
      ESP_LOGV(TAG, "BLE ESP_GATTC_REG_FOR_NOTIFY_EVT called");
      break;
    case ESP_GATTC_CONNECT_EVT:
      ESP_LOGV(TAG, "BLE ESP_GATTC_CONNECT_EVT called");
      break;
    case ESP_GATTC_DISCONNECT_EVT:
      ESP_LOGV(TAG, "BLE ESP_GATTC_DISCONNECT_EVT called");
      break;
    case ESP_GATTC_READ_MULTIPLE_EVT:
      ESP_LOGV(TAG, "BLE ESP_GATTC_READ_MULTIPLE_EVT called");
      break;
    case ESP_GATTC_QUEUE_FULL_EVT:
      ESP_LOGV(TAG, "BLE ESP_GATTC_QUEUE_FULL_EVT called");
      break;
    case ESP_GATTC_SET_ASSOC_EVT:
      ESP_LOGV(TAG, "BLE ESP_GATTC_SET_ASSOC_EVT called");
      break;
    case ESP_GATTC_GET_ADDR_LIST_EVT:
      ESP_LOGV(TAG, "BLE ESP_GATTC_GET_ADDR_LIST_EVT called");
      break;
    case ESP_GATTC_DIS_SRVC_CMPL_EVT:
      ESP_LOGV(TAG, "BLE ESP_GATTC_DIS_SRVC_CMPL_EVT called");
      break;
    case ESP_GATTC_READ_MULTI_VAR_EVT:
      ESP_LOGV(TAG, "BLE ESP_GATTC_READ_MULTI_VAR_EVT called");
      break;
  }
}
void FendtCaravan::add_command_(const std::string &cmd) {
  int8_t start_index = 0;
  int8_t end_index = 17;
  int8_t last_index = cmd.length();
  bool last_chunk = false;
  while (!last_chunk) {
    std::string chunk = "";
    if (end_index < last_index) {
      chunk = cmd.substr(0, 17);
      chunk += "@";
    } else {
      chunk = cmd.substr(start_index, last_index);
      last_chunk = true;
    }
    this->commands_.push_back(chunk);
    start_index = end_index;
    end_index = start_index + 17;
    if (end_index > last_index)
      end_index = last_index;
  }
}

void FendtCaravan::on_data_received_(const std::string &data) {
  std::string key, value;

  size_t start = 0;
  size_t end = data.find(':');
  key = data.substr(start, end);
  value = data.substr(end + 1);
  // if (this->control_unit_device_)
  //   this->control_unit_device_->decode(key, value);
  // if (this->alde_device_)
  //   this->alde_device_->decode(key, value);
  // if (this->fridge_device_)
  //   this->fridge_device_->decode(key, value);
  // if (this->lighting_device_)
  //   this->lighting_device_->decode(key, value);
}

void FendtCaravan::on_command_send(const std::string &command) {
  ESP_LOGD(TAG, "on_command_send called. Command: %s", command.c_str());
  this->add_command_(command);
}

}  // namespace fendt_caravan
}  // namespace esphome
#endif
