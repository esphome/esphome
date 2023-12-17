#include "madoka.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

namespace esphome {
namespace madoka {

using namespace esphome::climate;

void Madoka::dump_config() { LOG_CLIMATE(TAG, "Daikin Madoka Climate Controller", this); }

void Madoka::setup() {
  this->receive_semaphore_ = xSemaphoreCreateMutex();
}

void Madoka::loop() {
  chunk chk = {};
  if (xSemaphoreTake(this->receive_semaphore_, 0L)) {
    if (this->received_chunks_.size() > 0) {
      chk = this->received_chunks_.front();
      this->received_chunks_.pop();
    }
    xSemaphoreGive(this->receive_semaphore_);
    if (chk.size() > 0) {
      this->process_incoming_chunk(chk);
    }
  }
  if (this->should_update_) {
    this->should_update_ = false;
    this->update();
  }
}

void Madoka::control(const ClimateCall &call) {
  if (this->node_state != espbt::ClientState::ESTABLISHED)
    return;
  if (call.get_mode().has_value()) {
    ClimateMode mode = *call.get_mode();
    std::vector<chunk> pkt;
    uint8_t mode_ = 255, status_ = 0;
    switch (mode) {
      case climate::CLIMATE_MODE_OFF:
        status_ = 0;
        break;
      case climate::CLIMATE_MODE_HEAT_COOL:
        status_ = 1;
        mode_ = 2;
        break;
      case climate::CLIMATE_MODE_COOL:
        status_ = 1;
        mode_ = 3;
        break;
      case climate::CLIMATE_MODE_HEAT:
        status_ = 1;
        mode_ = 4;
        break;
      case climate::CLIMATE_MODE_FAN_ONLY:
        status_ = 1;
        mode_ = 0;
        break;
      case climate::CLIMATE_MODE_DRY:
        status_ = 1;
        mode_ = 1;
        break;
      default:
        ESP_LOGW(TAG, "Unsupported mode: %d", mode);
        break;
    }
    ESP_LOGD(TAG, "status: %d, mode: %d", status_, mode_);
    if (mode_ != 255) {
      this->query(0x4030, message({0x20, 0x01, (uint8_t) mode_}), 600);
    }
    this->query(0x4020, message({0x20, 0x01, (uint8_t) status_}), 200);
  }
  if (call.get_target_temperature_low().has_value() && call.get_target_temperature_high().has_value()) {
    uint16_t target_low = *call.get_target_temperature_low() * 128;
    uint16_t target_high = *call.get_target_temperature_high() * 128;
    this->query(0x4040,
                message({0x20, 0x02, (uint8_t)((target_high >> 8) & 0xFF), (uint8_t)(target_high & 0xFF), 0x21, 0x02,
                         (uint8_t)((target_low >> 8) & 0xFF), (uint8_t)(target_low & 0xFF)}),
                400);
  }
  if (call.get_fan_mode().has_value()) {
    uint8_t fan_mode = call.get_fan_mode().value();
    uint8_t fan_mode_ = 255;
    switch (fan_mode) {
      case climate::CLIMATE_FAN_AUTO:
        fan_mode_ = 0;
        break;
      case climate::CLIMATE_FAN_LOW:
        fan_mode_ = 1;
        break;
      case climate::CLIMATE_FAN_MEDIUM:
        fan_mode_ = 3;
        break;
      case climate::CLIMATE_FAN_HIGH:
        fan_mode_ = 5;
        break;
      default:
        ESP_LOGW(TAG, "Unsupported fan mode: %d", fan_mode);
        break;
    }
    if (fan_mode_ != 255) {
      this->query(0x4050, message({0x20, 0x01, (uint8_t) fan_mode_, 0x21, 0x01, (uint8_t) fan_mode_}), 200);
    }
  }
  this->should_update_ = true;
}

void Madoka::gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
  switch (event) {
    case ESP_GAP_BLE_SEC_REQ_EVT:
      esp_ble_gap_security_rsp(param->ble_security.ble_req.bd_addr, true);
      break;
    case ESP_GAP_BLE_NC_REQ_EVT:
      esp_ble_confirm_reply(param->ble_security.ble_req.bd_addr, true);
      ESP_LOGI(TAG, "ESP_GAP_BLE_NC_REQ_EVT, the passkey Notify number:%d", param->ble_security.key_notif.passkey);
      break;
    case ESP_GAP_BLE_AUTH_CMPL_EVT: {
      if (!param->ble_security.auth_cmpl.success) {
        ESP_LOGE(TAG, "Authentication failed, status: 0x%x", param->ble_security.auth_cmpl.fail_reason);
        break;
      }
      auto nfy = this->parent_->get_characteristic(MADOKA_SERVICE_UUID, NOTIFY_CHARACTERISTIC_UUID);
      auto wwr = this->parent_->get_characteristic(MADOKA_SERVICE_UUID, WWR_CHARACTERISTIC_UUID);
      if (nfy == nullptr || wwr == nullptr) {
        ESP_LOGW(TAG, "[%s] No control service found at device, not a Daikin Madoka..?", this->get_name().c_str());
        break;
      }
      this->notify_handle_ = nfy->handle;
      this->wwr_handle_ = wwr->handle;

      auto status = esp_ble_gattc_register_for_notify(this->parent_->get_gattc_if(), this->parent_->get_remote_bda(),
                                                      nfy->handle);
      if (status) {
        ESP_LOGW(TAG, "[%s] esp_ble_gattc_register_for_notify failed, status=%d", this->get_name().c_str(), status);
      }
      break;
    }
    default:
      break;
  }
}

void Madoka::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param) {
  switch (event) {
    case ESP_GATTC_DISCONNECT_EVT: {
      this->node_state = espbt::ClientState::IDLE;  // ??
      this->current_temperature = NAN;
      this->target_temperature = NAN;
      this->publish_state();
      break;
    }
    case ESP_GATTC_WRITE_DESCR_EVT:
      if (param->write.status != ESP_GATT_OK) {
        if (param->write.status == ESP_GATT_INSUF_AUTHENTICATION) {
          ESP_LOGE(TAG, "Insufficient authentication");
        } else {
          ESP_LOGE(TAG, "Failed writing characteristic descriptor, status = 0x%x", param->write.status);
        }
      }
      break;
    case ESP_GATTC_SEARCH_CMPL_EVT: {
      esp_ble_set_encryption(this->parent_->get_remote_bda(), ESP_BLE_SEC_ENCRYPT_MITM);
      break;
    }
    case ESP_GATTC_REG_FOR_NOTIFY_EVT: {
      this->node_state = espbt::ClientState::ESTABLISHED;  // ??
      break;
    }
    case ESP_GATTC_NOTIFY_EVT: {
      if (param->notify.handle != this->notify_handle_) {
        ESP_LOGW(TAG, "Different notify handle");
        break;
      }
      chunk chk = chunk(param->notify.value, param->notify.value + param->notify.value_len);
      xSemaphoreTake(this->receive_semaphore_, portMAX_DELAY);
      this->received_chunks_.push(chk);
      xSemaphoreGive(this->receive_semaphore_);
      break;
    }
    default:
      break;
  }
}

void Madoka::update() {
  ESP_LOGD(TAG, "Got update request...");
  if (this->node_state != espbt::ClientState::ESTABLISHED) {
    ESP_LOGD(TAG, "...but device is disconnected");
    return;
  }

  std::vector<uint16_t> all_cmds({0x0020, 0x0030, 0x0040, 0x0050, 0x0110});
  for (auto cmd : all_cmds) {
    this->query(cmd, message({0x00, 0x00}), 50);
  }
}

bool validate_buffer(message buffer) { return buffer[0] == buffer.size(); }

void Madoka::process_incoming_chunk(chunk chk) {
  if (chk.size() < 2) {
    ESP_LOGI(TAG, "Chunk discarded: invalid length.");
    return;
  }
  uint8_t chunk_id = chk[0];
  message stripped(chk.begin() + 1, chk.end());
  if (chunk_id == 0 && validate_buffer(stripped)) {
    this->parse_cb(stripped);
    return;
  }
  if (this->pending_chunks_.count(chunk_id)) {
    ESP_LOGE(TAG, "Another packet with the same chunk ID is already in the buffer.");
    ESP_LOGD(TAG, "Chunk ID: %d.", chunk_id);
    return;
  }
  this->pending_chunks_[chunk_id] = chk;

  if (this->pending_chunks_.size() != this->pending_chunks_.rbegin()->first + 1) {
    ESP_LOGW(TAG, "Buffer is missing packets");
    return;
  }

  message msg;
  int lim = this->pending_chunks_.size();
  for (int i = 0; i < lim; i++) {
    msg.insert(msg.end(), this->pending_chunks_[i].begin() + 1, this->pending_chunks_[i].end());
  }
  if (validate_buffer(msg)) {
    this->pending_chunks_.clear();
    this->parse_cb(msg);
  }
}

std::vector<chunk> Madoka::split_payload(message msg) {
  std::vector<chunk> result;
  size_t len = msg.size();
  result.push_back(chunk({0x00, (uint8_t)(len + 1)}));
  result[0].insert(result[0].end(), msg.begin(), min(msg.begin() + (MAX_CHUNK_SIZE - 2), msg.end()));
  int i = 0;
  for (i = 1; i < len / (MAX_CHUNK_SIZE - 1); i++) {  // from second to second-last
    result.push_back(
        chunk(msg.begin() + ((MAX_CHUNK_SIZE - 1) * i - 1), msg.begin() + ((MAX_CHUNK_SIZE - 1) * (i + 1) - 1)));
  }
  if (len > 18) {
    i++;
    result.push_back(chunk(msg.begin() + ((MAX_CHUNK_SIZE - 1) * i), msg.end()));
  }
  return result;
}

message Madoka::prepare_message(uint16_t cmd, message args) {
  message result({0x00, (uint8_t)((cmd >> 8) & 0xFF), (uint8_t)(cmd & 0xFF)});
  result.insert(result.end(), args.begin(), args.end());
  return result;
}

void Madoka::query(uint16_t cmd, message args, int t_d) {
  message payload = this->prepare_message(cmd, args);

  if (this->node_state != espbt::ClientState::ESTABLISHED) {
    return;
  }
  std::vector<chunk> chunks = this->split_payload(payload);

  for (auto chk : chunks) {
    esp_err_t status;
    for (int j = 0; j < BLE_SEND_MAX_RETRIES; j++) {
      status =
          esp_ble_gattc_write_char(this->parent_->get_gattc_if(), this->parent_->get_conn_id(), this->wwr_handle_,
                                    chk.size(), &chk[0], ESP_GATT_WRITE_TYPE_NO_RSP, ESP_GATT_AUTH_REQ_NONE);
      if (!status) {
        break;
      }
      ESP_LOGD(TAG, "[%s] esp_ble_gattc_write_char failed (%d of %d), status=%d",
                this->parent_->address_str().c_str(), j + 1, BLE_SEND_MAX_RETRIES, status);
    }
    if (status) {
      ESP_LOGE(TAG, "[%s] Command could not be sent, last status=%d", this->parent_->address_str().c_str(), status);
      return;
    }
  }
  delay(t_d);
}

void Madoka::parse_cb(message msg) {
  uint16_t function_id = msg[2] << 8 | msg[3];
  uint8_t i = 4;
  uint8_t message_size = msg.size();

  switch (function_id) {
    case 0x0020:
      while (i < message_size) {
        uint8_t argument_id = msg[i++];
        uint8_t len = msg[i++];
        if (argument_id == 0x20) {
          message val(msg.begin() + i, msg.begin() + i + len);
          this->cur_status_.status = val[0];
        }
        i += len;
      }
    case 0x0030:
      while (i < message_size) {
        uint8_t argument_id = msg[i++];
        uint8_t len = msg[i++];
        if (argument_id == 0x20) {
          message val(msg.begin() + i, msg.begin() + i + len);
          this->cur_status_.mode = val[0];
        }
        i += len;
      }
    default:
      break;
  }
  switch (function_id) {
    case 0x0020:
    case 0x0030:
      // ESP_LOGI(TAG, "status: %d, mode: %d", this->cur_status_.status, this->cur_status_.mode);
      if (this->cur_status_.status) {
        switch (this->cur_status_.mode) {
          case 0:
            this->mode = climate::CLIMATE_MODE_FAN_ONLY;
            break;
          case 1:
            this->mode = climate::CLIMATE_MODE_DRY;
            break;
          case 2:
            this->mode = climate::CLIMATE_MODE_HEAT_COOL;
            break;
          case 3:
            this->mode = climate::CLIMATE_MODE_COOL;
            break;
          case 4:
            this->mode = climate::CLIMATE_MODE_HEAT;
            break;
        }
      } else {
        this->mode = climate::CLIMATE_MODE_OFF;
      }
      break;
    case 0x0040:
      while (i < message_size) {
        uint8_t argument_id = msg[i++];
        uint8_t len = msg[i++];
        switch (argument_id) {
          case 0x20: {
            message val(msg.begin() + i, msg.begin() + i + len);
            this->target_temperature_high = (float) (val[0] << 8 | val[1]) / 128;
            break;
          }
          case 0x21: {
            message val(msg.begin() + i, msg.begin() + i + len);
            this->target_temperature_low = (float) (val[0] << 8 | val[1]) / 128;
            break;
          }
        }
        i += len;
      }
      break;
    case 0x0050: {
      uint8_t fan_mode = 255;
      while (i < message_size) {
        uint8_t argument_id = msg[i++];
        uint8_t len = msg[i++];
        if (this->cur_status_.mode  == 1) {}
        else if (argument_id == 0x21 && len == 1 && this->cur_status_.mode == 4) {
          fan_mode = msg[i];
        } else if (argument_id == 0x20 && len == 1){
          fan_mode = msg[i];
        }
        i += len;
      }
      switch (fan_mode) {
        case 0:
          this->fan_mode = climate::CLIMATE_FAN_AUTO;
          break;
        case 1:
          this->fan_mode = climate::CLIMATE_FAN_LOW;
          break;
        case 2:
        case 3:
        case 4:
          this->fan_mode = climate::CLIMATE_FAN_MEDIUM;
          break;
        case 5:
          this->fan_mode = climate::CLIMATE_FAN_HIGH;
        default:
          break;
      }
      break;
    }
    case 0x0110:
      while (i < message_size) {
        uint8_t argument_id = msg[i++];
        uint8_t len = msg[i++];
        if (argument_id == 0x40) {
          message val(msg.begin() + i, msg.begin() + i + len);
          this->current_temperature = val[0];
        }
        i += len;
      }
      break;
    default:
      break;
  }

  this->publish_state();
}

}  // namespace madoka
}  // namespace esphome

#endif
