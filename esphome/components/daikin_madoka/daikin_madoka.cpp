#include "daikin_madoka.h"

#include "esphome/core/log.h"
#include <utility>
#include <algorithm>

#ifdef USE_ESP32

namespace esphome::daikin_madoka {

using namespace esphome::climate;

static const char *const TAG = "daikin_madoka";

static const uint16_t CMD_GET_SETTING_STATUS = 0x0020;
static const uint16_t CMD_SET_SETTING_STATUS = 0x4020;
static const uint16_t CMD_GET_OPERATION_MODE = 0x0030;
static const uint16_t CMD_SET_OPERATION_MODE = 0x4030;
static const uint16_t CMD_GET_SETPOINT = 0x0040;
static const uint16_t CMD_SET_SETPOINT = 0x4040;
static const uint16_t CMD_GET_FAN_SPEED = 0x0050;
static const uint16_t CMD_SET_FAN_SPEED = 0x4050;
static const uint16_t CMD_GET_SENSOR_INFORMATION = 0x0110;

void DaikinMadoka::dump_config() { LOG_CLIMATE(TAG, "Daikin Madoka Climate Controller", this); }

void DaikinMadoka::setup() {}

inline static uint32_t get_command_cooldown(uint16_t cmd) {
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

void DaikinMadoka::loop() {
  while (!this->received_chunks_.empty()) {
    Chunk &chk = this->received_chunks_.front();

    if (chk.length != 0) {
      this->process_incoming_chunk_(chk);
    }
    this->received_chunks_.pop();
  }

  if (!this->query_queue_.empty() && !this->pending_message_) {
    Query query = std::move(this->query_queue_.front());
    this->query_queue_.pop();
    this->query_(query.cmd, query.args);
    this->pending_message_ = true;
    this->set_timeout("query", get_command_cooldown(query.cmd), [this]() { this->pending_message_ = false; });
  }
  if (this->should_update_) {
    this->should_update_ = false;
    this->update();
  }
}

void DaikinMadoka::control(const ClimateCall &call) {
  if (this->node_state != espbt::ClientState::ESTABLISHED) {
    this->publish_state();  // Update state to reflect that the device is disconnected
    return;
  }

  auto mode_opt = call.get_mode();
  if (mode_opt.has_value()) {
    ClimateMode mode = mode_opt.value();
    uint8_t mode_out = 255, status_out = 0;
    switch (mode) {
      case climate::CLIMATE_MODE_OFF:
        status_out = 0;
        break;
      case climate::CLIMATE_MODE_HEAT_COOL:
        status_out = 1;
        mode_out = 2;
        break;
      case climate::CLIMATE_MODE_COOL:
        status_out = 1;
        mode_out = 3;
        break;
      case climate::CLIMATE_MODE_HEAT:
        status_out = 1;
        mode_out = 4;
        break;
      case climate::CLIMATE_MODE_FAN_ONLY:
        status_out = 1;
        mode_out = 0;
        break;
      case climate::CLIMATE_MODE_DRY:
        status_out = 1;
        mode_out = 1;
        break;
      default:
        ESP_LOGW(TAG, "Unsupported mode: %d", mode);
        break;
    }
    ESP_LOGD(TAG, "status: %d, mode: %d", status_out, mode_out);
    if (mode_out != 255) {
      this->query_queue_.emplace(CMD_SET_OPERATION_MODE, std::vector<uint8_t>{0x20, 0x01, (uint8_t) mode_out});
    }
    this->query_queue_.emplace(CMD_SET_SETTING_STATUS, std::vector<uint8_t>{0x20, 0x01, (uint8_t) status_out});
  }
  std::vector<uint8_t> temp_setpoint_args;
  auto target_temperature_high_opt = call.get_target_temperature_high();
  if (target_temperature_high_opt.has_value()) {
    uint16_t target_high = std::clamp(target_temperature_high_opt.value(), MIN_TEMP, MAX_TEMP) * 128;
    temp_setpoint_args.insert(temp_setpoint_args.end(),
                              {0x20, 0x02, (uint8_t) ((target_high >> 8) & 0xFF), (uint8_t) (target_high & 0xFF)});
  }
  auto target_temperature_low_opt = call.get_target_temperature_low();
  if (target_temperature_low_opt.has_value()) {
    uint16_t target_low = std::clamp(target_temperature_low_opt.value(), MIN_TEMP, MAX_TEMP) * 128;
    temp_setpoint_args.insert(temp_setpoint_args.end(),
                              {0x21, 0x02, (uint8_t) ((target_low >> 8) & 0xFF), (uint8_t) (target_low & 0xFF)});
  }
  if (!temp_setpoint_args.empty()) {
    this->query_queue_.emplace(CMD_SET_SETPOINT, std::move(temp_setpoint_args));
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
      this->query_queue_.emplace(CMD_SET_FAN_SPEED, std::vector<uint8_t>{0x20, 0x01, (uint8_t) fan_mode_out, 0x21, 0x01,
                                                                         (uint8_t) fan_mode_out});
    }
  }
  this->should_update_ = true;
}

void DaikinMadoka::gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
  switch (event) {
    case ESP_GAP_BLE_NC_REQ_EVT:
      if (!this->parent()->check_addr(param->ble_security.key_notif.bd_addr))
        return;
      esp_ble_confirm_reply(param->ble_security.key_notif.bd_addr, true);
      ESP_LOGI(TAG, "ESP_GAP_BLE_NC_REQ_EVT, the passkey Notify number: %06u", param->ble_security.key_notif.passkey);
      break;
    case ESP_GAP_BLE_AUTH_CMPL_EVT: {
      if (!this->parent()->check_addr(param->ble_security.auth_cmpl.bd_addr))
        return;
      if (!param->ble_security.auth_cmpl.success) {
        ESP_LOGE(TAG, "Authentication failed, status: 0x%x", param->ble_security.auth_cmpl.fail_reason);
        break;
      }
      auto *nfy = this->parent_->get_characteristic(MADOKA_SERVICE_UUID, NOTIFY_CHARACTERISTIC_UUID);
      auto *wwr = this->parent_->get_characteristic(MADOKA_SERVICE_UUID, WWR_CHARACTERISTIC_UUID);
      if (nfy == nullptr || wwr == nullptr) {
        ESP_LOGW(TAG, "[%s] No control service found at device, not a Daikin Madoka..?", this->get_name().c_str());
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

      this->node_state = espbt::ClientState::IDLE;
      this->current_temperature = NAN;
      this->target_temperature_high = NAN;
      this->target_temperature_low = NAN;
      this->mode = climate::CLIMATE_MODE_OFF;
      this->fan_mode.reset();
      this->publish_state();

      this->received_chunks_.clear();
      this->partial_incoming_message_.data.clear();
      this->partial_incoming_message_.expected_chunk_id = 0;
      this->should_update_ = false;
      this->query_queue_.clear();
      this->pending_message_ = false;
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
      this->should_update_ = true;
      break;
    }
    case ESP_GATTC_NOTIFY_EVT: {
      if (param->notify.handle != this->notify_handle_) {
        ESP_LOGW(TAG, "Different notify handle");
        break;
      }
      if (param->notify.value_len > MAX_CHUNK_SIZE) {
        ESP_LOGW(TAG, "Received chunk too large: %d bytes", param->notify.value_len);
        break;
      }
      Chunk chk{};
      chk.length = param->notify.value_len;
      std::copy(param->notify.value, param->notify.value + param->notify.value_len, chk.data.begin());
      this->received_chunks_.push(chk);
      break;
    }
    default:
      break;
  }
}

void DaikinMadoka::update() {
  static constexpr std::array<uint16_t, 5> ALL_CMDS{CMD_GET_SETTING_STATUS, CMD_GET_OPERATION_MODE, CMD_GET_SETPOINT,
                                                    CMD_GET_FAN_SPEED, CMD_GET_SENSOR_INFORMATION};

  ESP_LOGD(TAG, "Got update request...");
  if (this->node_state != espbt::ClientState::ESTABLISHED) {
    ESP_LOGD(TAG, "...but device is disconnected");
    return;
  }
  for (auto cmd : ALL_CMDS) {
    this->query_queue_.emplace(cmd, std::vector<uint8_t>{0x00, 0x00});
  }
}

void DaikinMadoka::process_incoming_chunk_(const Chunk &chk) {
  if (chk.length < 2) {
    ESP_LOGI(TAG, "Chunk discarded: invalid length.");
    return;
  }
  uint8_t chunk_id = chk.data[0];
  std::span<const uint8_t> stripped{chk.data.begin() + 1, chk.data.begin() + chk.length};
  if (chunk_id == 0 && stripped.size() == stripped[0]) {
    this->parse_cb_(stripped);
    return;
  }

  if (chunk_id != this->partial_incoming_message_.expected_chunk_id) {
    ESP_LOGW(TAG, "Chunk discarded: invalid chunk id.");
    ESP_LOGD(TAG, "Expected chunk id: %u, got: %u", this->partial_incoming_message_.expected_chunk_id, chunk_id);
    this->partial_incoming_message_.data.clear();
    this->partial_incoming_message_.expected_chunk_id = 0;
    if (chunk_id != 0)
      return;
  }

  if (chunk_id == 0) {
    this->partial_incoming_message_.data.reserve(stripped[0]);
  }

  this->partial_incoming_message_.data.insert(this->partial_incoming_message_.data.end(), stripped.begin(),
                                              stripped.end());
  this->partial_incoming_message_.expected_chunk_id++;

  if (this->partial_incoming_message_.data.size() > this->partial_incoming_message_.data[0]) {
    ESP_LOGW(TAG, "Chunk discarded: message too long.");
    this->partial_incoming_message_.data.clear();
    this->partial_incoming_message_.expected_chunk_id = 0;
    return;
  }
  if (this->partial_incoming_message_.data.size() == this->partial_incoming_message_.data[0]) {
    this->parse_cb_(this->partial_incoming_message_.data);
    this->partial_incoming_message_.data.clear();
    this->partial_incoming_message_.expected_chunk_id = 0;
  }
}

esp_err_t DaikinMadoka::send_message_(std::span<uint8_t> chk) {
  esp_err_t status = ESP_OK;
  const char *addr = this->parent_->address_str();

  for (int j = 0; j < BLE_SEND_MAX_RETRIES; j++) {
    status = esp_ble_gattc_write_char(this->parent_->get_gattc_if(), this->parent_->get_conn_id(), this->wwr_handle_,
                                      chk.size(), chk.data(), ESP_GATT_WRITE_TYPE_NO_RSP, ESP_GATT_AUTH_REQ_NONE);
    if (!status) {
      break;
    }
    ESP_LOGD(TAG, "[%s] esp_ble_gattc_write_char failed (%d of %d), status=%d", addr, j + 1, BLE_SEND_MAX_RETRIES,
             status);
  }
  return status;
}

void DaikinMadoka::query_(uint16_t cmd, std::vector<uint8_t> &args) {
  if (this->node_state != espbt::ClientState::ESTABLISHED) {
    return;
  }

  const char *addr = this->parent_->address_str();

  // Leave first byte for length, then add 3 bytes for command, then add the args
  std::vector<uint8_t> payload({0x00, 0x00, (uint8_t) ((cmd >> 8) & 0xFF), (uint8_t) (cmd & 0xFF)});
  payload.insert(payload.end(), args.begin(), args.end());
  size_t len = payload.size();

  if (len > 255) {
    ESP_LOGE(TAG, "[%s] Command too long to send, len=%zu", addr, len);
    return;
  }

  // Populate leading length byte
  payload[0] = (uint8_t) len;

  Chunk chunk{};
  for (size_t i = 0; i * (MAX_CHUNK_SIZE - 1) < len; i++) {
    auto chunk_start = payload.begin() + (i * (MAX_CHUNK_SIZE - 1));
    auto chunk_end = payload.begin() + std::min(payload.size(), (i + 1) * (MAX_CHUNK_SIZE - 1));
    chunk.data[0] = (uint8_t) i;
    std::copy(chunk_start, chunk_end, chunk.data.begin() + 1);
    chunk.length = chunk_end - chunk_start + 1;

    esp_err_t status = this->send_message_(std::span<uint8_t>(chunk.data.data(), chunk.length));
    if (status) {
      ESP_LOGE(TAG, "[%s] Command could not be sent, last status=%d", addr, status);
      return;
    }
  }
}

using ArgPair = std::pair<uint16_t, std::span<const uint8_t>>;

static auto find_arg(const std::vector<ArgPair> &args, uint16_t arg_id) {
  return std::find_if(args.begin(), args.end(), [arg_id](const ArgPair &p) { return p.first == arg_id; });
};

void DaikinMadoka::parse_cb_(std::span<const uint8_t> msg) {
  if (msg.size() < 4) {
    ESP_LOGE(TAG, "Discarding message: invalid length.");
    return;
  }
  const uint16_t function_id = msg[2] << 8 | msg[3];
  size_t i = 4;
  const size_t message_size = msg.size();

  std::vector<ArgPair> parsed_args;
  while (i <= message_size - 2) {  // `-2` accounts for arg_id and len bytes
    uint8_t argument_id = msg[i++];
    uint8_t len = msg[i++];
    if (i + len > message_size) {
      ESP_LOGE(TAG, "Discarding message: invalid argument length.");
      return;
    }
    parsed_args.emplace_back(argument_id, std::span<const uint8_t>{msg.begin() + i, len});
    i += len;
  }

  switch (function_id) {
    case CMD_GET_SETTING_STATUS:
    case CMD_GET_OPERATION_MODE: {
      auto arg = find_arg(parsed_args, 0x20);
      if (arg == parsed_args.end() || arg->second.size() != 1) {
        break;
      }
      if (function_id == CMD_GET_SETTING_STATUS) {
        this->cur_status_.status = arg->second[0] == 1;
      } else if (function_id == CMD_GET_OPERATION_MODE) {
        this->cur_status_.mode = arg->second[0];
      }
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
    }
    case CMD_GET_SETPOINT: {
      auto arg_high = find_arg(parsed_args, 0x20);
      if (arg_high != parsed_args.end() && arg_high->second.size() == 2) {
        this->target_temperature_high = (float) (arg_high->second[0] << 8 | arg_high->second[1]) / 128;
      }

      auto arg_low = find_arg(parsed_args, 0x21);
      if (arg_low != parsed_args.end() && arg_low->second.size() == 2) {
        this->target_temperature_low = (float) (arg_low->second[0] << 8 | arg_low->second[1]) / 128;
      }
      break;
    }
    case CMD_GET_FAN_SPEED: {
      auto arg_fan = parsed_args.cend();
      switch (this->cur_status_.mode) {
        case 0:
        case 2:
        case 3:
          arg_fan = find_arg(parsed_args, 0x20);
          break;
        case 4:
          arg_fan = find_arg(parsed_args, 0x21);
          break;
        default:
          break;
      }
      if (arg_fan == parsed_args.end() || arg_fan->second.size() != 1) {
        break;
      }
      switch (arg_fan->second[0]) {
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
      auto arg_temp = find_arg(parsed_args, 0x40);
      if (arg_temp != parsed_args.end() && arg_temp->second.size() == 1) {
        this->current_temperature = (float) arg_temp->second[0];
      }
      break;
    }
    default:
      break;
  }

  this->set_timeout("publish_state", 800, [this]() { this->publish_state(); });
}

}  // namespace esphome::daikin_madoka

#endif
