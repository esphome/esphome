#include "renogy_inverter_ble.h"

#ifdef USE_ESP32

#include <cstring>
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::renogy_inverter_ble {

ESPHOME_LOG_TAG(TAG, "renogy_inverter_ble");

static constexpr uint16_t SERVICE_NOTIFY_UUID = 0xFFF0;
static constexpr uint16_t CHAR_NOTIFY_UUID = 0xFFF1;
static constexpr uint16_t SERVICE_WRITE_UUID = 0xFFD0;
static constexpr uint16_t CHAR_WRITE_UUID = 0xFFD1;
static constexpr uint16_t CHAR_INIT_UUID = 0xFFD4;

static constexpr uint8_t MODBUS_DEVICE_ID = 0x20;
static constexpr uint8_t MODBUS_READ_HOLDING = 0x03;
static constexpr uint16_t REG_MAIN = 4000;
static constexpr uint16_t REG_MAIN_WORDS = 32;
static constexpr uint16_t REG_LOAD = 4408;
static constexpr uint16_t REG_LOAD_WORDS = 6;

// Watchdog: returns the cycle to IDLE when the inverter stops answering
static constexpr uint32_t CYCLE_TIMEOUT_MS = 3000;
// Gap the inverter needs between two consecutive register reads
static constexpr uint32_t LOAD_READ_DELAY_MS = 300;

static constexpr uint32_t CYCLE_TIMEOUT_ID = 0;
static constexpr uint32_t LOAD_READ_ID = 1;

static void publish(sensor::Sensor *s, float value) {
  if (s != nullptr) {
    s->publish_state(value);
  }
}

void RenogyInverterBle::dump_config() {
  ESP_LOGCONFIG(TAG, "Renogy Inverter BLE:");
  LOG_SENSOR("  ", "AC input voltage", this->ac_input_voltage_sensor_);
  LOG_SENSOR("  ", "AC output voltage", this->ac_output_voltage_sensor_);
  LOG_SENSOR("  ", "AC output current", this->ac_output_current_sensor_);
  LOG_SENSOR("  ", "AC output frequency", this->ac_output_frequency_sensor_);
  LOG_SENSOR("  ", "Input frequency", this->input_frequency_sensor_);
  LOG_SENSOR("  ", "Battery voltage", this->battery_voltage_sensor_);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Load current", this->load_current_sensor_);
  LOG_SENSOR("  ", "Load active power", this->load_active_power_sensor_);
  LOG_SENSOR("  ", "Load apparent power", this->load_apparent_power_sensor_);
}

void RenogyInverterBle::abort_cycle_() {
  this->cancel_timeout(CYCLE_TIMEOUT_ID);
  this->cancel_timeout(LOAD_READ_ID);
  this->frame_len_ = 0;
  this->state_ = State::IDLE;
}

void RenogyInverterBle::read_register_(uint16_t start_register, uint16_t word_count) {
  std::array<uint8_t, 8> req{MODBUS_DEVICE_ID,
                             MODBUS_READ_HOLDING,
                             static_cast<uint8_t>(start_register >> 8),
                             static_cast<uint8_t>(start_register),
                             static_cast<uint8_t>(word_count >> 8),
                             static_cast<uint8_t>(word_count)};
  const uint16_t crc = crc16(req.data(), 6);
  req[6] = crc & 0xFF;  // Modbus sends the CRC low byte first
  req[7] = crc >> 8;

  this->frame_len_ = 0;
  const esp_err_t status =
      esp_ble_gattc_write_char(this->parent_->get_gattc_if(), this->parent_->get_conn_id(), this->write_handle_,
                               req.size(), req.data(), ESP_GATT_WRITE_TYPE_NO_RSP, ESP_GATT_AUTH_REQ_NONE);
  if (status != ESP_OK) {
    ESP_LOGW(TAG, "write_char (reg %u) failed, status=%d", start_register, status);
    this->abort_cycle_();
  }
}

void RenogyInverterBle::start_cycle_() {
  this->set_timeout(CYCLE_TIMEOUT_ID, CYCLE_TIMEOUT_MS, [this]() {
    ESP_LOGW(TAG, "Read cycle timed out (state=%u); resetting", static_cast<uint8_t>(this->state_));
    this->abort_cycle_();
  });
  if (this->init_handle_ == 0) {
    // No init characteristic discovered; try the main read directly
    this->state_ = State::MAIN;
    this->read_register_(REG_MAIN, REG_MAIN_WORDS);
    return;
  }
  this->state_ = State::INIT;
  const esp_err_t status = esp_ble_gattc_read_char(this->parent_->get_gattc_if(), this->parent_->get_conn_id(),
                                                   this->init_handle_, ESP_GATT_AUTH_REQ_NONE);
  if (status != ESP_OK) {
    ESP_LOGW(TAG, "init read_char failed, status=%d", status);
    this->abort_cycle_();
  }
}

void RenogyInverterBle::update() {
  if (this->node_state != espbt::ClientState::ESTABLISHED) {
    ESP_LOGD(TAG, "Not connected yet; skipping poll");
    return;
  }
  if (this->state_ != State::IDLE) {
    ESP_LOGD(TAG, "Previous read cycle still in progress; skipping");
    return;
  }
  this->start_cycle_();
}

void RenogyInverterBle::on_frame_complete_(uint16_t len) {
  // A Modbus frame with its CRC appended checks to zero
  if (len < 5 || crc16(this->frame_.data(), len) != 0) {
    ESP_LOGW(TAG, "CRC mismatch (state=%u, len=%u)", static_cast<uint8_t>(this->state_), len);
    this->abort_cycle_();
    return;
  }
  // Exception response: function code with the high bit set, third byte is the exception code
  if ((this->frame_[1] & 0x80) != 0) {
    ESP_LOGW(TAG, "Modbus exception 0x%02X (state=%u)", this->frame_[2], static_cast<uint8_t>(this->state_));
    this->abort_cycle_();
    return;
  }
  // Only the reply to the outstanding read is accepted: same device, same function, the requested word count
  const bool awaiting_reply = this->state_ == State::MAIN || this->state_ == State::LOAD;
  const uint16_t expected_words = this->state_ == State::MAIN ? REG_MAIN_WORDS : REG_LOAD_WORDS;
  if (!awaiting_reply || this->frame_[0] != MODBUS_DEVICE_ID || this->frame_[1] != MODBUS_READ_HOLDING ||
      this->frame_[2] != 2 * expected_words) {
    ESP_LOGW(TAG, "Unexpected response %02X %02X %02X (state=%u)", this->frame_[0], this->frame_[1], this->frame_[2],
             static_cast<uint8_t>(this->state_));
    this->abort_cycle_();
    return;
  }
  auto reg16 = [this](uint8_t i) { return encode_uint16(this->frame_[3 + 2 * i], this->frame_[4 + 2 * i]); };

  if (this->state_ == State::MAIN) {
    publish(this->ac_input_voltage_sensor_, reg16(0) * 0.1f);
    publish(this->ac_output_voltage_sensor_, reg16(2) * 0.1f);
    publish(this->ac_output_current_sensor_, reg16(3) * 0.01f);
    publish(this->ac_output_frequency_sensor_, reg16(4) * 0.01f);
    publish(this->battery_voltage_sensor_, reg16(5) * 0.1f);
    publish(this->temperature_sensor_, reg16(6) * 0.1f);
    publish(this->input_frequency_sensor_, reg16(9) * 0.01f);
    this->frame_len_ = 0;
    this->set_timeout(LOAD_READ_ID, LOAD_READ_DELAY_MS, [this]() {
      this->state_ = State::LOAD;
      this->read_register_(REG_LOAD, REG_LOAD_WORDS);
    });
    return;
  }
  publish(this->load_current_sensor_, reg16(0) * 0.01f);
  publish(this->load_active_power_sensor_, static_cast<float>(reg16(1)));
  publish(this->load_apparent_power_sensor_, static_cast<float>(reg16(2)));
  this->abort_cycle_();
}

void RenogyInverterBle::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                            esp_ble_gattc_cb_param_t *param) {
  switch (event) {
    case ESP_GATTC_DISCONNECT_EVT: {
      this->abort_cycle_();
      this->node_state = espbt::ClientState::IDLE;
      break;
    }
    case ESP_GATTC_SEARCH_CMPL_EVT: {
      auto *notify_chr = this->parent_->get_characteristic(espbt::ESPBTUUID::from_uint16(SERVICE_NOTIFY_UUID),
                                                           espbt::ESPBTUUID::from_uint16(CHAR_NOTIFY_UUID));
      auto *write_chr = this->parent_->get_characteristic(espbt::ESPBTUUID::from_uint16(SERVICE_WRITE_UUID),
                                                          espbt::ESPBTUUID::from_uint16(CHAR_WRITE_UUID));
      auto *init_chr = this->parent_->get_characteristic(espbt::ESPBTUUID::from_uint16(SERVICE_WRITE_UUID),
                                                         espbt::ESPBTUUID::from_uint16(CHAR_INIT_UUID));
      if (notify_chr == nullptr || write_chr == nullptr) {
        ESP_LOGE(TAG, "Required characteristics not found; not a Renogy inverter?");
        break;
      }
      this->notify_handle_ = notify_chr->handle;
      this->write_handle_ = write_chr->handle;
      this->init_handle_ = (init_chr != nullptr) ? init_chr->handle : 0;
      ESP_LOGI(TAG, "handles: notify=%u write=%u init=%u", this->notify_handle_, this->write_handle_,
               this->init_handle_);
      const esp_err_t status = this->parent_->register_for_notify(this->notify_handle_);
      if (status != ESP_OK) {
        ESP_LOGW(TAG, "register_for_notify failed, status=%d", status);
      }
      break;
    }
    case ESP_GATTC_REG_FOR_NOTIFY_EVT: {
      // Other nodes on the same client see this event too
      if (param->reg_for_notify.handle != this->notify_handle_) {
        break;
      }
      if (param->reg_for_notify.status != ESP_GATT_OK) {
        ESP_LOGW(TAG, "Notification registration failed, status=%d", param->reg_for_notify.status);
        break;
      }
      this->node_state = espbt::ClientState::ESTABLISHED;
      ESP_LOGI(TAG, "Connected; will poll the inverter every update interval");
      break;
    }
    case ESP_GATTC_READ_CHAR_EVT: {
      if (this->state_ == State::INIT && param->read.handle == this->init_handle_) {
        // Init handshake done; the inverter now answers Modbus reads
        this->state_ = State::MAIN;
        this->read_register_(REG_MAIN, REG_MAIN_WORDS);
      }
      break;
    }
    case ESP_GATTC_NOTIFY_EVT: {
      if (param->notify.handle != this->notify_handle_) {
        break;
      }
      const uint16_t value_len = param->notify.value_len;
      if (this->frame_len_ + value_len > MAX_FRAME) {
        ESP_LOGW(TAG, "Frame overflow; resetting");
        this->abort_cycle_();
        break;
      }
      memcpy(&this->frame_[this->frame_len_], param->notify.value, value_len);
      this->frame_len_ += value_len;
      if (this->frame_len_ < 3) {
        break;
      }
      // An exception frame ([id][func|0x80][exc][crc][crc]) is 5 bytes; a normal response is
      // [id][func][byte_count][...data][crc][crc]
      const uint16_t expected_len = ((this->frame_[1] & 0x80) != 0) ? 5 : 3 + this->frame_[2] + 2;
      if (this->frame_len_ >= expected_len) {
        this->on_frame_complete_(expected_len);
      }
      break;
    }
    default:
      break;
  }
}

}  // namespace esphome::renogy_inverter_ble

#endif  // USE_ESP32
