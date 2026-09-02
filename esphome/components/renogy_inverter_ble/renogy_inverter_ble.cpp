#include "renogy_inverter_ble.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

namespace esphome::renogy_inverter_ble {

static const char *const TAG = "renogy_inverter_ble";

// GATT UUIDs (16-bit). Notify svc/char and write svc/char + the proprietary init char.
static const uint16_t SERVICE_NOTIFY_UUID = 0xFFF0;
static const uint16_t CHAR_NOTIFY_UUID = 0xFFF1;
static const uint16_t SERVICE_WRITE_UUID = 0xFFD0;
static const uint16_t CHAR_WRITE_UUID = 0xFFD1;
static const uint16_t CHAR_INIT_UUID = 0xFFD4;

// Modbus over BLE.
static const uint8_t MODBUS_DEVICE_ID = 0x20;
static const uint8_t MODBUS_READ_HOLDING = 0x03;
static const uint16_t REG_MAIN = 4000;
static const uint16_t REG_MAIN_WORDS = 32;
static const uint16_t REG_LOAD = 4408;
static const uint16_t REG_LOAD_WORDS = 6;
// Largest expected response: 3 header + 2*32 data + 2 CRC = 69 bytes.
static const size_t MAX_FRAME = 80;
// Watchdog for one read cycle. A healthy init->main->load exchange finishes well under a second;
// 3 s is generous headroom. If it elapses the state machine is wedged (silent device, lost
// notification) and we reset so the next poll retries.
static const uint32_t CYCLE_TIMEOUT_MS = 3000;

static uint16_t modbus_crc16(const uint8_t *data, uint16_t len) {
  uint16_t crc = 0xFFFF;
  for (uint16_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8; bit++) {
      if ((crc & 0x0001) != 0) {
        crc = (crc >> 1) ^ 0xA001;
      } else {
        crc >>= 1;
      }
    }
  }
  return crc;  // serialized low byte first
}

void RenogyInverterBle::setup() { this->frame_.reserve(MAX_FRAME); }

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

static void publish(sensor::Sensor *s, float value) {
  if (s != nullptr) {
    s->publish_state(value);
  }
}

void RenogyInverterBle::reset_frame_() {
  this->frame_.clear();
  this->expected_len_ = 0;
}

void RenogyInverterBle::abort_cycle_() {
  this->cancel_timeout("rng_cycle");
  this->cancel_timeout("rng_load");
  this->reset_frame_();
  this->state_ = State::IDLE;
}

static bool crc_valid(const uint8_t *data, uint16_t len) {
  if (len < 5) {
    return false;
  }
  const uint16_t crc = modbus_crc16(data, len - 2);
  const uint8_t crc_lo = crc & 0xFF;
  const uint8_t crc_hi = (crc >> 8) & 0xFF;
  return data[len - 2] == crc_lo && data[len - 1] == crc_hi;
}

void RenogyInverterBle::read_register_(uint16_t start_register, uint16_t word_count) {
  std::array<uint8_t, 8> req{};
  req[0] = MODBUS_DEVICE_ID;
  req[1] = MODBUS_READ_HOLDING;
  req[2] = (start_register >> 8) & 0xFF;
  req[3] = start_register & 0xFF;
  req[4] = (word_count >> 8) & 0xFF;
  req[5] = word_count & 0xFF;
  const uint16_t crc = modbus_crc16(req.data(), 6);
  req[6] = crc & 0xFF;
  req[7] = (crc >> 8) & 0xFF;

  this->reset_frame_();
  const esp_err_t status =
      esp_ble_gattc_write_char(this->parent_->get_gattc_if(), this->parent_->get_conn_id(), this->write_handle_,
                               req.size(), req.data(), ESP_GATT_WRITE_TYPE_NO_RSP, ESP_GATT_AUTH_REQ_NONE);
  if (status != ESP_OK) {
    ESP_LOGW(TAG, "write_char (reg %u) failed, status=%d", start_register, status);
    this->abort_cycle_();
  }
}

void RenogyInverterBle::start_cycle_() {
  this->reset_frame_();
  // Arm the cycle watchdog. The YAML mqtt/wifi reboot_timeout cannot rescue a wedged read cycle:
  // MQTT stays alive while only this state machine is stuck, so the device would publish stale
  // values forever. This timer guarantees the cycle always returns to IDLE.
  this->set_timeout("rng_cycle", CYCLE_TIMEOUT_MS, [this]() {
    ESP_LOGW(TAG, "Read cycle timed out (state=%u); resetting", static_cast<uint8_t>(this->state_));
    this->abort_cycle_();
  });
  if (this->init_handle_ != 0) {
    this->state_ = State::INIT;
    const esp_err_t status = esp_ble_gattc_read_char(this->parent_->get_gattc_if(), this->parent_->get_conn_id(),
                                                     this->init_handle_, ESP_GATT_AUTH_REQ_NONE);
    if (status != ESP_OK) {
      ESP_LOGW(TAG, "init read_char failed, status=%d", status);
      this->abort_cycle_();
    }
  } else {
    // No init characteristic discovered — try the main read directly (best effort).
    this->state_ = State::MAIN;
    this->read_register_(REG_MAIN, REG_MAIN_WORDS);
  }
}

void RenogyInverterBle::update() {
  if (!this->established_) {
    ESP_LOGD(TAG, "Not connected yet; skipping poll");
    return;
  }
  if (this->state_ != State::IDLE) {
    ESP_LOGD(TAG, "Previous read cycle still in progress; skipping");
    return;
  }
  this->start_cycle_();
}

void RenogyInverterBle::on_frame_complete_() {
  const uint16_t len = this->expected_len_;
  if (!crc_valid(this->frame_.data(), len)) {
    ESP_LOGW(TAG, "CRC mismatch (state=%u, len=%u)", static_cast<uint8_t>(this->state_), len);
    this->abort_cycle_();
    return;
  }
  // Modbus exception response: the function code echoes back with the high bit set (e.g. 0x83 for
  // a read). The third byte is the exception code, not a byte count — bail before parsing data.
  if ((this->frame_[1] & 0x80) != 0) {
    ESP_LOGW(TAG, "Modbus exception 0x%02X (state=%u)", this->frame_[2], static_cast<uint8_t>(this->state_));
    this->abort_cycle_();
    return;
  }
  const uint8_t word_count = this->frame_[2] / 2;
  // NOTE: do NOT name this `word` — Arduino.h defines a `word(...)` macro that expands the calls
  // to makeWord() and silently returns the argument (every word(i) became i → all values wrong).
  auto reg16 = [this](uint8_t i) -> uint16_t {
    return static_cast<uint16_t>((this->frame_[3 + 2 * i] << 8) | this->frame_[3 + 2 * i + 1]);
  };

  if (this->state_ == State::MAIN) {
    if (word_count < 10) {
      ESP_LOGW(TAG, "Main response too short: %u words", word_count);
      this->abort_cycle_();
      return;
    }
    publish(this->ac_input_voltage_sensor_, reg16(0) * 0.1f);
    publish(this->ac_output_voltage_sensor_, reg16(2) * 0.1f);
    publish(this->ac_output_current_sensor_, reg16(3) * 0.01f);
    publish(this->ac_output_frequency_sensor_, reg16(4) * 0.01f);
    publish(this->battery_voltage_sensor_, reg16(5) * 0.1f);
    publish(this->temperature_sensor_, reg16(6) * 0.1f);
    publish(this->input_frequency_sensor_, reg16(9) * 0.01f);
    this->reset_frame_();
    // Inter-command delay, then read the load register.
    this->set_timeout("rng_load", 300, [this]() {
      this->state_ = State::LOAD;
      this->read_register_(REG_LOAD, REG_LOAD_WORDS);
    });
  } else if (this->state_ == State::LOAD) {
    if (word_count < 3) {
      ESP_LOGW(TAG, "Load response too short: %u words", word_count);
    } else {
      publish(this->load_current_sensor_, reg16(0) * 0.01f);
      publish(this->load_active_power_sensor_, static_cast<float>(reg16(1)));
      publish(this->load_apparent_power_sensor_, static_cast<float>(reg16(2)));
    }
    this->abort_cycle_();
  } else {
    this->abort_cycle_();
  }
}

void RenogyInverterBle::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                            esp_ble_gattc_cb_param_t *param) {
  switch (event) {
    case ESP_GATTC_DISCONNECT_EVT: {
      this->established_ = false;
      this->notify_handle_ = 0;
      this->write_handle_ = 0;
      this->init_handle_ = 0;
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
        ESP_LOGE(TAG, "Required characteristics not found — not a Renogy inverter?");
        break;
      }
      this->notify_handle_ = notify_chr->handle;
      this->write_handle_ = write_chr->handle;
      this->init_handle_ = (init_chr != nullptr) ? init_chr->handle : 0;
      ESP_LOGI(TAG, "handles: notify=%u write=%u init=%u", this->notify_handle_, this->write_handle_,
               this->init_handle_);
      const esp_err_t status = esp_ble_gattc_register_for_notify(this->parent_->get_gattc_if(),
                                                                 this->parent_->get_remote_bda(), this->notify_handle_);
      if (status != ESP_OK) {
        ESP_LOGW(TAG, "register_for_notify failed, status=%d", status);
      }
      break;
    }
    case ESP_GATTC_REG_FOR_NOTIFY_EVT: {
      this->established_ = true;
      this->state_ = State::IDLE;
      this->node_state = espbt::ClientState::ESTABLISHED;
      ESP_LOGI(TAG, "Connected; will poll the inverter every update interval");
      break;
    }
    case ESP_GATTC_READ_CHAR_EVT: {
      if (this->state_ == State::INIT && param->read.handle == this->init_handle_) {
        // Init handshake done — now the inverter answers Modbus reads.
        this->state_ = State::MAIN;
        this->read_register_(REG_MAIN, REG_MAIN_WORDS);
      }
      break;
    }
    case ESP_GATTC_NOTIFY_EVT: {
      if (param->notify.handle != this->notify_handle_) {
        break;
      }
      if (this->frame_.size() + param->notify.value_len > MAX_FRAME) {
        ESP_LOGW(TAG, "Frame overflow; resetting");
        this->reset_frame_();
        this->state_ = State::IDLE;
        break;
      }
      this->frame_.insert(this->frame_.end(), param->notify.value, param->notify.value + param->notify.value_len);
      if (this->expected_len_ == 0 && this->frame_.size() >= 3) {
        // Exception frame ([id][func|0x80][exc][crc][crc]) is 5 bytes; a normal response is
        // [id][func][byte_count][...data][crc][crc]. Pick the expected length by the function code.
        this->expected_len_ = ((this->frame_[1] & 0x80) != 0) ? 5 : (3 + this->frame_[2] + 2);
      }
      if (this->expected_len_ != 0 && this->frame_.size() >= this->expected_len_) {
        this->on_frame_complete_();
      }
      break;
    }
    default:
      break;
  }
}

}  // namespace esphome::renogy_inverter_ble

#endif  // USE_ESP32
