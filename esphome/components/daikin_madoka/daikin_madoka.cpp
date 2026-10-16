#include "daikin_madoka.h"

#include <algorithm>
#include <cinttypes>
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

namespace esphome::daikin_madoka {

using namespace esphome::climate;

ESPHOME_LOG_TAG(TAG, "daikin_madoka");

static const uint16_t CMD_GET_SETTING_STATUS = 0x0020;
static const uint16_t CMD_SET_SETTING_STATUS = 0x4020;
static const uint16_t CMD_GET_OPERATION_MODE = 0x0030;
static const uint16_t CMD_SET_OPERATION_MODE = 0x4030;
static const uint16_t CMD_GET_SETPOINT = 0x0040;
static const uint16_t CMD_SET_SETPOINT = 0x4040;
static const uint16_t CMD_GET_FAN_SPEED = 0x0050;
static const uint16_t CMD_SET_FAN_SPEED = 0x4050;
static const uint16_t CMD_GET_SENSOR_INFORMATION = 0x0110;

// Indexed by the device's operation mode byte
static constexpr std::array<ClimateMode, 5> DEVICE_MODES{CLIMATE_MODE_FAN_ONLY, CLIMATE_MODE_DRY,
                                                         CLIMATE_MODE_HEAT_COOL, CLIMATE_MODE_COOL, CLIMATE_MODE_HEAT};
static constexpr uint8_t DEVICE_MODE_HEAT = 4;

void DaikinMadoka::dump_config() { LOG_CLIMATE(TAG, "Daikin Madoka Climate Controller", this); }

static uint32_t get_command_cooldown(uint16_t cmd) {
  switch (cmd) {
    case CMD_GET_SETTING_STATUS:
    case CMD_GET_OPERATION_MODE:
    case CMD_GET_SETPOINT:
    case CMD_GET_FAN_SPEED:
    case CMD_GET_SENSOR_INFORMATION:
      return 50;
    case CMD_SET_SETTING_STATUS:
      return 200;
    case CMD_SET_OPERATION_MODE:
      return 600;
    case CMD_SET_SETPOINT:
      return 400;
    case CMD_SET_FAN_SPEED:
      return 200;
    default:
      return 100;
  }
}

void DaikinMadoka::control(const ClimateCall &call) {
  if (this->node_state != espbt::ClientState::ESTABLISHED) {
    this->publish_state();  // Update state to reflect that the device is disconnected
    return;
  }

  auto mode_opt = call.get_mode();
  if (mode_opt.has_value()) {
    const ClimateMode mode = mode_opt.value();
    if (mode == CLIMATE_MODE_OFF) {
      this->enqueue_query_(CMD_SET_SETTING_STATUS, std::array<uint8_t, 3>{0x20, 0x01, 0});
    } else {
      auto it = std::find(DEVICE_MODES.begin(), DEVICE_MODES.end(), mode);
      if (it == DEVICE_MODES.end()) {
        ESP_LOGW(TAG, "Unsupported mode: %d", mode);
      } else {
        const auto device_mode = static_cast<uint8_t>(it - DEVICE_MODES.begin());
        this->enqueue_query_(CMD_SET_OPERATION_MODE, std::array<uint8_t, 3>{0x20, 0x01, device_mode});
        this->enqueue_query_(CMD_SET_SETTING_STATUS, std::array<uint8_t, 3>{0x20, 0x01, 1});
      }
    }
  }
  StaticVector<uint8_t, MAX_QUERY_ARGS> setpoint_args;
  if (auto high = call.get_target_temperature_high(); high.has_value()) {
    append_setpoint(setpoint_args, 0x20, *high);
  }
  if (auto low = call.get_target_temperature_low(); low.has_value()) {
    append_setpoint(setpoint_args, 0x21, *low);
  }
  if (!setpoint_args.empty()) {
    this->enqueue_query_(CMD_SET_SETPOINT, setpoint_args);
  }
  auto fan_mode_opt = call.get_fan_mode();
  if (fan_mode_opt.has_value()) {
    uint8_t fan_mode = fan_mode_opt.value();
    uint8_t fan_mode_out = 255;
    switch (fan_mode) {
      case climate::CLIMATE_FAN_AUTO:
        fan_mode_out = 0;
        break;
      case climate::CLIMATE_FAN_LOW:
        fan_mode_out = 1;
        break;
      case climate::CLIMATE_FAN_MEDIUM:
        fan_mode_out = 3;
        break;
      case climate::CLIMATE_FAN_HIGH:
        fan_mode_out = 5;
        break;
      default:
        ESP_LOGW(TAG, "Unsupported fan mode: %d", fan_mode);
        break;
    }
    if (fan_mode_out != 255) {
      // Sets the cooling (0x20) and heating (0x21) fan speeds together
      this->enqueue_query_(CMD_SET_FAN_SPEED,
                           std::array<uint8_t, 6>{0x20, 0x01, fan_mode_out, 0x21, 0x01, fan_mode_out});
    }
  }
  this->update();
}

void DaikinMadoka::fail_connection_(const LogString *reason) {
  ESP_LOGW(TAG, "[%s] %s, disconnecting", this->parent_->address_str(), LOG_STR_ARG(reason));
  this->status_set_warning(reason);
  this->parent_->disconnect();
}

void DaikinMadoka::gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
  switch (event) {
    case ESP_GAP_BLE_NC_REQ_EVT:
      if (!this->parent()->check_addr(param->ble_security.key_notif.bd_addr))
        return;
      esp_ble_confirm_reply(param->ble_security.key_notif.bd_addr, true);
      ESP_LOGI(TAG, "ESP_GAP_BLE_NC_REQ_EVT, the passkey Notify number: %06" PRIu32,
               param->ble_security.key_notif.passkey);
      break;
    case ESP_GAP_BLE_AUTH_CMPL_EVT: {
      if (!this->parent()->check_addr(param->ble_security.auth_cmpl.bd_addr))
        return;
      if (!param->ble_security.auth_cmpl.success) {
        ESP_LOGW(TAG, "Authentication failed, reason: 0x%x", param->ble_security.auth_cmpl.fail_reason);
        this->fail_connection_(LOG_STR("Pairing failed"));
        break;
      }
      auto *nfy = this->parent_->get_characteristic(MADOKA_SERVICE_UUID, NOTIFY_CHARACTERISTIC_UUID);
      auto *wwr = this->parent_->get_characteristic(MADOKA_SERVICE_UUID, WWR_CHARACTERISTIC_UUID);
      if (nfy == nullptr || wwr == nullptr) {
        this->fail_connection_(LOG_STR("Madoka control service not found"));
        break;
      }
      this->notify_handle_ = nfy->handle;
      this->wwr_handle_ = wwr->handle;

      auto status = this->parent_->register_for_notify(nfy->handle);
      if (status) {
        ESP_LOGW(TAG, "[%s] failed to register for notify, status=%d", this->get_name().c_str(), status);
      }
      break;
    }
    default:
      break;
  }
}

void DaikinMadoka::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                       esp_ble_gattc_cb_param_t *param) {
  switch (event) {
    case ESP_GATTC_DISCONNECT_EVT: {
      this->notify_handle_ = 0;
      this->wwr_handle_ = 0;

      this->current_temperature = NAN;
      this->target_temperature_high = NAN;
      this->target_temperature_low = NAN;
      this->mode = climate::CLIMATE_MODE_OFF;
      this->fan_mode.reset();
      this->publish_state();

      this->assembler_.reset();
      this->cancel_timeout("query");
      this->query_cooldown_ = false;
      this->query_queue_.clear();
      this->pending_responses_ = 0;
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
      if (param->reg_for_notify.handle != this->notify_handle_) {
        break;
      }
      if (param->reg_for_notify.status != ESP_GATT_OK) {
        ESP_LOGE(TAG, "Failed to register for notify, status = 0x%x", param->reg_for_notify.status);
        break;
      }
      this->node_state = espbt::ClientState::ESTABLISHED;
      this->status_clear_warning();
      this->update();
      break;
    }
    case ESP_GATTC_NOTIFY_EVT: {
      if (param->notify.handle != this->notify_handle_) {
        ESP_LOGW(TAG, "Different notify handle");
        break;
      }
      this->process_incoming_chunk_(std::span<const uint8_t>{param->notify.value, param->notify.value_len});
      break;
    }
    default:
      break;
  }
}

void DaikinMadoka::update() {
  static constexpr std::array<uint16_t, 5> ALL_CMDS{CMD_GET_SETTING_STATUS, CMD_GET_OPERATION_MODE, CMD_GET_SETPOINT,
                                                    CMD_GET_FAN_SPEED, CMD_GET_SENSOR_INFORMATION};
  static constexpr std::array<uint8_t, 2> NO_ARGS{0x00, 0x00};

  ESP_LOGD(TAG, "Got update request...");
  if (this->node_state != espbt::ClientState::ESTABLISHED) {
    ESP_LOGD(TAG, "...but device is disconnected");
    return;
  }
  // Nothing is in flight, so responses still counted from the last batch were lost
  if (!this->query_cooldown_) {
    this->pending_responses_ = 0;
  }
  for (auto cmd : ALL_CMDS) {
    this->enqueue_query_(cmd, NO_ARGS);
  }
}

void DaikinMadoka::process_incoming_chunk_(std::span<const uint8_t> chunk) {
  switch (this->assembler_.feed(chunk)) {
    case ChunkResult::CHUNK_RESULT_COMPLETE:
      this->parse_cb_(this->assembler_.message());
      break;
    case ChunkResult::CHUNK_RESULT_INVALID_LENGTH:
      ESP_LOGW(TAG, "Chunk discarded: invalid length");
      break;
    case ChunkResult::CHUNK_RESULT_OUT_OF_ORDER:
      ESP_LOGW(TAG, "Chunk discarded: unexpected chunk id %u", chunk[0]);
      break;
    case ChunkResult::CHUNK_RESULT_TOO_LONG:
      ESP_LOGW(TAG, "Message discarded: too long");
      break;
    case ChunkResult::CHUNK_RESULT_PARTIAL:
      break;
  }
}

void DaikinMadoka::enqueue_query_(uint16_t cmd, std::span<const uint8_t> args) {
  Query query{.cmd = cmd, .len = static_cast<uint8_t>(args.size()), .args = {}};
  std::copy(args.begin(), args.end(), query.args.begin());
  if (!this->query_queue_.push(query)) {
    ESP_LOGW(TAG, "Query queue full, dropping command 0x%04X", cmd);
    return;
  }
  if (!this->query_cooldown_) {
    this->send_next_query_();
  }
}

void DaikinMadoka::send_next_query_() {
  if (this->query_queue_.empty()) {
    this->query_cooldown_ = false;
    return;
  }
  const Query &query = this->query_queue_.front();
  const uint16_t cmd = query.cmd;
  // On a failed write the query stays queued for the next pass
  if (this->write_query_(query)) {
    this->query_queue_.pop();
    this->pending_responses_++;
  }
  this->query_cooldown_ = true;
  this->set_timeout("query", get_command_cooldown(cmd), [this]() { this->send_next_query_(); });
}

bool DaikinMadoka::write_query_(const Query &query) {
  static_assert(MAX_QUERY_ARGS + MESSAGE_HEADER_SIZE <= MAX_CHUNK_SIZE - 1, "A query must fit in one chunk");
  // Chunk id 0, then the message: length, reserved byte, command, args
  std::array<uint8_t, MAX_CHUNK_SIZE> frame;
  const uint8_t msg_len = MESSAGE_HEADER_SIZE + query.len;
  const auto cmd = decode_value<uint16_t>(query.cmd);
  frame[0] = 0;
  frame[1] = msg_len;
  frame[2] = 0;
  frame[3] = cmd[0];
  frame[4] = cmd[1];
  std::copy_n(query.args.begin(), query.len, frame.begin() + 1 + MESSAGE_HEADER_SIZE);
  esp_err_t err =
      esp_ble_gattc_write_char(this->parent_->get_gattc_if(), this->parent_->get_conn_id(), this->wwr_handle_,
                               msg_len + 1, frame.data(), ESP_GATT_WRITE_TYPE_NO_RSP, ESP_GATT_AUTH_REQ_NONE);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "[%s] Writing command 0x%04X failed, status=%d", this->parent_->address_str(), query.cmd, err);
    return false;
  }
  return true;
}

void DaikinMadoka::update_mode_from_status_() {
  if (!this->power_on_) {
    this->mode = CLIMATE_MODE_OFF;
  } else if (this->device_mode_ < DEVICE_MODES.size()) {
    this->mode = DEVICE_MODES[this->device_mode_];
  }
}

void DaikinMadoka::parse_cb_(std::span<const uint8_t> msg) {
  if (!is_valid_message(msg)) {
    ESP_LOGW(TAG, "Discarding message: invalid length");
    return;
  }

  switch (message_function_id(msg)) {
    case CMD_GET_SETTING_STATUS: {
      if (auto value = find_arg(msg, 0x20); value.size() == 1) {
        this->power_on_ = value[0] == 1;
        this->update_mode_from_status_();
      }
      break;
    }
    case CMD_GET_OPERATION_MODE: {
      if (auto value = find_arg(msg, 0x20); value.size() == 1) {
        this->device_mode_ = value[0];
        this->update_mode_from_status_();
      }
      break;
    }
    case CMD_GET_SETPOINT: {
      if (auto high = find_arg(msg, 0x20); high.size() == 2) {
        this->target_temperature_high = decode_setpoint(high);
      }
      if (auto low = find_arg(msg, 0x21); low.size() == 2) {
        this->target_temperature_low = decode_setpoint(low);
      }
      break;
    }
    case CMD_GET_FAN_SPEED: {
      // Heat mode uses the heating fan speed (0x21); the other modes use the cooling one (0x20)
      auto value = find_arg(msg, this->device_mode_ == DEVICE_MODE_HEAT ? 0x21 : 0x20);
      if (value.size() != 1) {
        break;
      }
      switch (value[0]) {
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
          break;
        default:
          break;
      }
      break;
    }
    case CMD_GET_SENSOR_INFORMATION: {
      if (auto value = find_arg(msg, 0x40); value.size() == 1) {
        this->current_temperature = static_cast<float>(value[0]);
      }
      break;
    }
    default:
      break;
  }

  if (this->pending_responses_ > 0 && --this->pending_responses_ == 0) {
    this->publish_state();
  }
}

}  // namespace esphome::daikin_madoka

#endif
