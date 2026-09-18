#include "midea_portasplit.h"
#include "esphome/core/log.h"

namespace esphome::midea_portasplit {

static const char *const TAG = "midea_portasplit";

void PortaSplitClimate::setup() {
  // First status poll shortly after boot, serial number once at 5 s.
  this->last_status_poll_ = millis() - STATUS_INTERVAL + 2000;
  this->last_energy_poll_ = millis() - ENERGY_INTERVAL + 4000;
  this->last_props_poll_ = millis() - PROPS_INTERVAL + 6000;
  this->last_group5_poll_ = millis() - ENERGY_INTERVAL + 9000;
  this->last_diag_poll_ = millis() - ENERGY_INTERVAL + 12000;
}

void PortaSplitClimate::loop() {
  // ---- RX ----
  while (this->available()) {
    uint8_t b;
    this->read_byte(&b);
    this->rx_feed_(b);
  }

  const uint32_t now = millis();

  // ---- one-shot serial number query at boot (+5 s) ----
  if (!this->serial_requested_ && now > 5000) {
    this->serial_requested_ = true;
    this->send_b1_read_({PROP_SERIAL});
  }
  // ---- one-shot device info / firmware query at boot (+7 s) ----
  if (!this->fw_requested_ && now > 7000) {
    this->fw_requested_ = true;
    this->send_device_info_query_();
  }
  // ---- B5 capability queries at boot (+10 s, +11 s) ----
  // Required to trigger the AC's B5 type-0x05 telemetry pushes (coil temps).
  // Without these, the AC never starts pushing 0x00E0 telemetry.
  // Sub 0 and sub 1, same frames the original module sends.
  if (!this->caps_requested_ && now > 10000) {
    this->caps_requested_ = true;
    static const uint8_t CAP0[] = {0xB5, 0x01, 0x00};
    this->send_frame_(0x03, 0x03, CAP0, sizeof(CAP0));
  }
  if (!this->caps1_requested_ && now > 11000) {
    this->caps1_requested_ = true;
    static const uint8_t CAP1[] = {0xB5, 0x01, 0x01, 0x01};
    this->send_frame_(0x03, 0x03, CAP1, sizeof(CAP1));
  }

  // ---- polling schedule ----
  if (now - this->last_status_poll_ >= STATUS_INTERVAL) {
    this->last_status_poll_ = now;
    this->send_status_query_();
  }
  if (now - this->last_energy_poll_ >= ENERGY_INTERVAL) {
    this->last_energy_poll_ = now;
    this->send_energy_query_();
  }
  if (now - this->last_group5_poll_ >= ENERGY_INTERVAL) {
    this->last_group5_poll_ = now;
    this->send_group5_query_();
  }
  // 0x0D network status every ~10 s (required for Group 3 and full module emulation)
  if (now - this->last_net_status_ >= 10000) {
    this->last_net_status_ = now;
    this->send_network_status_();
  }
  if (now - this->last_diag_poll_ >= ENERGY_INTERVAL) {
    this->last_diag_poll_ = now;
    this->send_group_query_(0x41);  // compressor diagnostics
    this->send_group_query_(0x42);  // indoor fan RPM
  }
  if (now - this->last_props_poll_ >= PROPS_INTERVAL) {
    this->last_props_poll_ = now;
    this->send_b1_read_({PROP_SILENT_MODE, PROP_POWER_LIMIT, PROP_BEEPER, PROP_SELF_CLEAN, PROP_TEMP_RANGE});
  }
  // ---- fast follow-up status query after a command ----
  if (this->followup_at_ != 0 && now >= this->followup_at_) {
    this->followup_at_ = 0;
    this->send_status_query_();
  }
}

void PortaSplitClimate::dump_config() {
  ESP_LOGCONFIG(TAG, "Midea PortaSplit (sync byte 0x00 protocol variant)");
  this->dump_traits_(TAG);
}

climate::ClimateTraits PortaSplitClimate::traits() {
  auto traits = climate::ClimateTraits();
  traits.add_feature_flags(climate::CLIMATE_SUPPORTS_CURRENT_TEMPERATURE);
  traits.set_visual_min_temperature(16.0f);
  traits.set_visual_max_temperature(30.0f);
  traits.set_visual_target_temperature_step(0.5f);
  traits.set_visual_current_temperature_step(0.5f);
  traits.set_supported_modes({
      climate::CLIMATE_MODE_OFF,
      climate::CLIMATE_MODE_HEAT_COOL,
      climate::CLIMATE_MODE_COOL,
      climate::CLIMATE_MODE_DRY,
      climate::CLIMATE_MODE_HEAT,
      climate::CLIMATE_MODE_FAN_ONLY,
  });
  traits.set_supported_fan_modes({
      climate::CLIMATE_FAN_AUTO,
      climate::CLIMATE_FAN_QUIET,
      climate::CLIMATE_FAN_LOW,
      climate::CLIMATE_FAN_MEDIUM,
      climate::CLIMATE_FAN_HIGH,
  });
  traits.set_supported_swing_modes({
      climate::CLIMATE_SWING_OFF,
      climate::CLIMATE_SWING_VERTICAL,
  });
  traits.set_supported_presets({
      climate::CLIMATE_PRESET_NONE, climate::CLIMATE_PRESET_SLEEP, climate::CLIMATE_PRESET_BOOST,
      climate::CLIMATE_PRESET_ECO,
      climate::CLIMATE_PRESET_AWAY,  // Frost protection / 8 degC mode (capability 0x0213)
  });
  return traits;
}

void PortaSplitClimate::control(const climate::ClimateCall &call) {
  if (call.get_mode().has_value()) {
    climate::ClimateMode m = *call.get_mode();
    if (m == climate::CLIMATE_MODE_OFF) {
      this->st_power_ = false;
    } else {
      this->st_power_ = true;
      this->st_mode_ = this->climate_mode_to_midea_(m);
    }
  }
  if (call.get_target_temperature().has_value())
    this->st_target_ = clamp(*call.get_target_temperature(), 16.0f, 30.0f);
  if (call.get_fan_mode().has_value()) {
    switch (*call.get_fan_mode()) {
      case climate::CLIMATE_FAN_QUIET:
        this->st_fan_ = 20;
        break;
      case climate::CLIMATE_FAN_LOW:
        this->st_fan_ = 40;
        break;
      case climate::CLIMATE_FAN_MEDIUM:
        this->st_fan_ = 60;
        break;
      case climate::CLIMATE_FAN_HIGH:
        this->st_fan_ = 80;
        break;
      case climate::CLIMATE_FAN_AUTO:
      default:
        this->st_fan_ = 102;
        break;
    }
  }
  if (call.get_swing_mode().has_value())
    this->st_swing_ = (*call.get_swing_mode() == climate::CLIMATE_SWING_VERTICAL);
  if (call.get_preset().has_value()) {
    this->st_turbo_ = false;
    this->st_sleep_ = false;
    this->st_eco_ = false;
    this->st_frost_ = false;
    switch (*call.get_preset()) {
      case climate::CLIMATE_PRESET_BOOST:
        this->st_turbo_ = true;
        break;
      case climate::CLIMATE_PRESET_SLEEP:
        this->st_sleep_ = true;
        break;
      case climate::CLIMATE_PRESET_ECO:
        this->st_eco_ = true;
        break;
      case climate::CLIMATE_PRESET_AWAY:
        // Frost protection (8 degC heating). Device-side only meaningful in heat mode.
        this->st_frost_ = true;
        this->st_mode_ = 4;  // heat
        this->st_power_ = true;
        break;
      default:
        break;
    }
  }
  this->send_set_command_();
}

void PortaSplitClimate::switch_command(PortaSplitSwitchType type, bool state) {
  switch (type) {
    case SW_ION:
      this->st_ion_ = state;
      this->send_set_command_();
      break;
    case SW_BEEPER:
      this->send_b0_write_(PROP_BEEPER, state ? 1 : 0);
      if (this->beeper_switch_ != nullptr)
        this->beeper_switch_->publish_state(state);
      break;
    case SW_SELF_CLEAN:
      // NOTE: deactivating self-clean powers the AC OFF (device behavior).
      this->send_b0_write_(PROP_SELF_CLEAN, state ? 1 : 0);
      if (this->self_clean_switch_ != nullptr)
        this->self_clean_switch_->publish_state(state);
      break;
    case SW_LED:
      // The LED command is a TOGGLE. Only send when the requested state
      // differs from the last known state (read back from C0 byte 24).
      if (state != this->led_state_)
        this->send_led_toggle_();
      break;
    case SW_TEMP_RANGE:
      this->range_enable_ = state;
      this->send_temp_range_();
      break;
    case SW_SILENT:
      // Out Silent Mode: only 0 (off) and 3 (on) have any effect --
      // intermediate values 1/2 verified to do nothing (acoustic test).
      this->send_b0_write_(PROP_SILENT_MODE, state ? 3 : 0);
      if (this->silent_switch_ != nullptr)
        this->silent_switch_->publish_state(state);
      break;
  }
}

void PortaSplitClimate::select_command(PortaSplitSelectType type, const std::string &value) {
  if (type == SEL_POWER_LIMIT) {
    // Device is hard-stepped: intermediate values are clamped DOWN to the
    // next step (<75 -> 50, <100 -> 75). Verified by experiment.
    uint8_t v = 100;
    if (value == "50%")
      v = 50;
    else if (value == "75%")
      v = 75;
    this->send_b0_write_(PROP_POWER_LIMIT, v);
    if (this->power_limit_select_ != nullptr)
      this->power_limit_select_->publish_state(value);
  }
}

void PortaSplitClimate::number_command(PortaSplitNumberType type, float value) {
  switch (type) {
    case NUM_FAN_SPEED:
      this->st_fan_ = (uint8_t) clamp(value, 1.0f, 100.0f);
      this->send_set_command_();
      if (this->fan_number_ != nullptr)
        this->fan_number_->publish_state(value);
      break;
    case NUM_RANGE_MIN:
      this->range_min_ = clamp(value, 16.0f, 30.0f);
      if (this->range_min_ > this->range_max_)
        this->range_max_ = this->range_min_;
      this->send_temp_range_();
      break;
    case NUM_RANGE_MAX:
      this->range_max_ = clamp(value, 16.0f, 30.0f);
      if (this->range_max_ < this->range_min_)
        this->range_min_ = this->range_max_;
      this->send_temp_range_();
      break;
  }
}

// ================= frame helpers =================

uint8_t PortaSplitClimate::crc8_(const uint8_t *data, size_t len) {
  uint8_t crc = 0;
  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (uint8_t j = 0; j < 8; j++)
      crc = (crc & 1) ? (crc >> 1) ^ 0x8C : crc >> 1;
  }
  return crc;
}

void PortaSplitClimate::send_frame_(uint8_t proto, uint8_t type, const uint8_t *payload, size_t len, bool add_crc) {
  std::vector<uint8_t> f;
  f.reserve(12 + len);
  const size_t total = 10 + len + (add_crc ? 1 : 0) + 1;
  f.push_back(0xAA);
  f.push_back((uint8_t) (total - 1));  // LEN = total - 1
  f.push_back(0xAC);                   // device type
  f.push_back(0x00);                   // SYNC -- MUST be 0x00 on the PortaSplit
  f.push_back(0x00);
  f.push_back(0x00);
  f.push_back(0x00);
  f.push_back(0x00);
  f.push_back(proto);
  f.push_back(type);
  f.insert(f.end(), payload, payload + len);
  if (add_crc)
    f.push_back(crc8_(payload, len));
  uint8_t cs = 0;
  for (size_t i = 1; i < f.size(); i++)
    cs = (uint8_t) (cs - f[i]);
  f.push_back(cs);
  this->write_array(f);
  ESP_LOGV(TAG, "TX: %s", format_hex_pretty(f).c_str());
}

// ================= outgoing commands =================

void PortaSplitClimate::send_set_command_() {
  uint8_t p[24] = {0};
  p[0] = 0x40;
  p[1] = 0x02;  // mandatory bit
  if (this->st_power_)
    p[1] |= 0x01;
  bool beeper_on = (this->beeper_switch_ != nullptr) ? this->beeper_switch_->state : true;
  if (beeper_on)
    p[1] |= 0x40;
  uint8_t ti = (uint8_t) this->st_target_;
  bool half = (this->st_target_ - (float) ti) >= 0.25f;
  p[2] = (uint8_t) ((this->st_mode_ << 5) | (half ? 0x10 : 0x00) | ((ti - 16) & 0x0F));
  p[3] = this->st_fan_;
  p[4] = 0x7F;
  p[5] = 0x7F;
  p[6] = 0xF0;
  p[7] = this->st_swing_ ? 0x3C : 0x30;
  p[8] = this->st_turbo_ ? 0x20 : 0x00;
  p[9] = (uint8_t) ((this->st_ion_ ? 0x20 : 0x00) | (this->st_eco_ ? 0x80 : 0x00));
  p[10] = this->st_sleep_ ? 0x01 : 0x00;
  p[18] = (uint8_t) ((ti - 12) & 0x1F);
  p[19] = 0x80;
  p[21] = 0x0A;
  p[22] = this->st_frost_ ? 0x80 : 0x00;  // frost protection (msmart mapping, experimental)
  p[23] = this->msg_id_++;
  this->send_frame_(0x02, 0x02, p, sizeof(p));
  this->followup_at_ = millis() + 1500;  // read back actual state
}

void PortaSplitClimate::send_status_query_() {
  uint8_t p[12] = {0x41, 0x21, 0x00, 0xFF, 0x03, 0xFF, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00};
  p[11] = this->msg_id_++;
  this->send_frame_(0x00, 0x03, p, sizeof(p));
}

void PortaSplitClimate::send_energy_query_() {
  uint8_t p[21] = {0x41, 0x21, 0x01, 0x44};
  p[20] = this->msg_id_++;
  this->send_frame_(0x00, 0x03, p, sizeof(p));
}

void PortaSplitClimate::send_group_query_(uint8_t group) {
  uint8_t p[21] = {0x41, 0x21, 0x01, group};
  p[20] = this->msg_id_++;
  this->send_frame_(0x00, 0x03, p, sizeof(p));
}

void PortaSplitClimate::send_group5_query_() {
  // Group 5: humidity / outdoor fan speed / defrost (byte-verified frame in doc Section 12)
  uint8_t p[21] = {0x41, 0x21, 0x01, 0x45};
  p[20] = this->msg_id_++;
  this->send_frame_(0x00, 0x03, p, sizeof(p));
}

void PortaSplitClimate::send_network_status_() {
  // Periodic 0x0D network status -- module sends these every ~10 s to prove
  // it's fully online. Without them, the AC withholds Group 3 responses.
  // Uses XOR sync. Byte 6 is an incrementing counter.
  uint8_t f[31] = {
      0xAA, 0x1E, 0xAC, 0xB2,  // start, LEN=30, device, sync=XOR
      0x00, 0x00, 0x00, 0x00,  // reserved (byte 6 = counter, filled below)
      0x08,                    // protocol
      0x0D,                    // network status
      0x01,                    // present
      0x01,                    // connected
      0x04,                    // WiFi
      0xC0, 0xA8, 0xB2, 0x72,  // IP (192.168.178.114 BE)
      0x00, 0x00, 0x00,        //
      0x01, 0x01, 0x01,        //
      0x28,                    // signal
      0x09,                    // channel
      0x08,                    // WiFi flags
      0x03,                    // protocol version
      0x00, 0x00, 0x00,        //
      0x00                     // checksum
  };
  f[6] = this->net_counter_++;
  uint8_t cs = 0;
  for (int i = 1; i < 30; i++)
    cs = (uint8_t) (cs - f[i]);
  f[30] = cs;
  this->write_array(f, sizeof(f));
}

void PortaSplitClimate::send_keepalive_response_() {
  // Respond to the AC's 0x63 poll with module-present + IP.
  // Uses XOR sync (0x1E ^ 0xAC = 0xB2) like the original module.
  // Without this response, the AC withholds Group 1/3 queries and
  // B5 type-0x05 telemetry pushes (verified by UART dump). The AC does
  // not actually use the IP -- it only needs a non-zero, checksum-valid
  // response, so a fixed known-good pattern from the sniffer captures is used.
  uint8_t f[31] = {
      0xAA, 0x1E, 0xAC, 0xB2,  // start, LEN=30, device, sync=XOR
      0x00, 0x00, 0x00, 0x00,  // reserved
      0x08,                    // protocol
      0x63,                    // keepalive response
      0x01,                    // module present
      0x01,                    // connection state: connected
      0x04,                    // connection type (WiFi)
      0xC0, 0xA8, 0xB2, 0x72,  // IP (192.168.178.114 BE)
      0x00, 0x00, 0x00,        //
      0x01, 0x01, 0x01,        // flags
      0x28,                    // signal quality (~40)
      0x09,                    // WiFi channel (placeholder)
      0x00,                    //
      0x03,                    // protocol version
      0x00, 0x00, 0x00,        //
      0x00                     // checksum placeholder
  };
  uint8_t cs = 0;
  for (int i = 1; i < 30; i++)
    cs = (uint8_t) (cs - f[i]);
  f[30] = cs;
  this->write_array(f, sizeof(f));
}

void PortaSplitClimate::send_device_info_query_() {
  // Special short frame without proto/CRC8 -- byte-verified: AA 0A AC 00 00 00 00 00 03 A0 A7
  static const uint8_t FRAME[11] = {0xAA, 0x0A, 0xAC, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0xA0, 0xA7};
  this->write_array(FRAME, sizeof(FRAME));
}

void PortaSplitClimate::send_led_toggle_() {
  uint8_t p[22] = {0x41, 0x61, 0x00, 0xFF, 0x02, 0x00, 0x02};
  p[21] = this->msg_id_++;
  this->send_frame_(0x00, 0x03, p, sizeof(p));
  this->followup_at_ = millis() + 1500;
}

void PortaSplitClimate::send_b1_read_(const std::vector<uint16_t> &ids) {
  std::vector<uint8_t> p;
  p.push_back(0xB1);
  p.push_back((uint8_t) ids.size());
  for (uint16_t id : ids) {
    p.push_back((uint8_t) (id & 0xFF));
    p.push_back((uint8_t) (id >> 8));
  }
  p.push_back(this->msg_id_++);
  this->send_frame_(0x02, 0x03, p.data(), p.size());
}

void PortaSplitClimate::send_b0_write_bytes_(uint16_t id, const uint8_t *data, uint8_t len) {
  std::vector<uint8_t> p;
  p.reserve(7 + len);
  p.push_back(0xB0);
  p.push_back(0x01);
  p.push_back((uint8_t) (id & 0xFF));
  p.push_back((uint8_t) (id >> 8));
  p.push_back(len);
  p.insert(p.end(), data, data + len);
  p.push_back(this->msg_id_++);
  this->send_frame_(0x02, 0x02, p.data(), p.size());
  // Re-read properties shortly after a write
  this->last_props_poll_ = millis() - PROPS_INTERVAL + 2000;
}

void PortaSplitClimate::send_temp_range_() {
  // Property 0x0051: [enable, min*2, max*2, FF, FF]
  uint8_t d[5] = {
      (uint8_t) (this->range_enable_ ? 0x01 : 0x00),
      (uint8_t) (this->range_min_ * 2.0f),
      (uint8_t) (this->range_max_ * 2.0f),
      0xFF,
      0xFF,
  };
  this->send_b0_write_bytes_(PROP_TEMP_RANGE, d, sizeof(d));
  if (this->temp_range_switch_ != nullptr)
    this->temp_range_switch_->publish_state(this->range_enable_);
  if (this->range_min_number_ != nullptr)
    this->range_min_number_->publish_state(this->range_min_);
  if (this->range_max_number_ != nullptr)
    this->range_max_number_->publish_state(this->range_max_);
}

// ================= RX handling =================

void PortaSplitClimate::rx_feed_(uint8_t b) {
  if (this->rx_.empty() && b != 0xAA)
    return;  // wait for start marker
  this->rx_.push_back(b);
  if (this->rx_.size() < 2)
    return;
  const size_t total = (size_t) this->rx_[1] + 1;  // LEN = total - 1
  if (total < 11 || total > 128) {
    this->rx_.clear();
    return;
  }
  if (this->rx_.size() < total)
    return;
  // full frame collected -- validate checksum
  uint8_t cs = 0;
  for (size_t i = 1; i + 1 < total; i++)
    cs = (uint8_t) (cs - this->rx_[i]);
  if (cs == this->rx_[total - 1]) {
    if (this->log_frames_) {
      ESP_LOGI(TAG, "RX: %s", format_hex_pretty(this->rx_).c_str());
    } else {
      ESP_LOGV(TAG, "RX: %s", format_hex_pretty(this->rx_).c_str());
    }
    this->handle_frame_(this->rx_);
  } else {
    ESP_LOGW(TAG, "RX checksum mismatch, dropping frame");
  }
  this->rx_.clear();
}

void PortaSplitClimate::handle_frame_(const std::vector<uint8_t> &f) {
  if (f.size() < 12)
    return;
  // 0x63 and 0xA0 use f[9] as the frame type (not f[10])
  if (f[9] == 0x63) {
    this->send_keepalive_response_();
    return;
  }
  if (f[9] == 0xA0) {
    this->handle_a0_(f);
    return;
  }
  switch (f[10]) {
    case 0xC0:
      this->handle_c0_(f);
      break;
    case 0xC1:
      this->handle_c1_(f);
      break;
    case 0xB1:
    case 0xB0:
      this->handle_b1_b0_(f);
      break;
    case 0xB5:
      this->handle_b5_(f);
      break;

    default:
      ESP_LOGV(TAG, "Unhandled response type 0x%02X", f[10]);
      break;
  }
}

void PortaSplitClimate::handle_c0_(const std::vector<uint8_t> &f) {
  if (f.size() < 26)
    return;
  const bool power = f[11] & 0x01;
  const uint8_t mode = (f[12] >> 5) & 0x07;
  const float target = (float) ((f[12] & 0x0F) + 16) + ((f[12] & 0x10) ? 0.5f : 0.0f);
  uint8_t fan = f[13];
  // Device clamps to internal steps and reports them back (40->30, 60->50)
  if (fan == 30)
    fan = 40;
  if (fan == 50)
    fan = 60;
  const bool swing = (f[17] & 0x0F) == 0x0C;
  const bool turbo = f[18] & 0x20;
  const bool ion = f[19] & 0x20;
  // ECO active flag: frame byte 19 bit 4 (0x10), per common Midea protocol
  // (msmart-ng payload[9] & 0x10). Set command uses bit 7, readback uses bit 4.
  const bool eco = f[19] & 0x10;
  const bool sleep = f[20] & 0x01;
  // Frost protection readback: frame byte 31 bit 7 (msmart payload[21] & 0x80, experimental)
  const bool frost = (f.size() > 31) && (f[31] & 0x80);
  const float indoor = ((float) f[21] - 50.0f) / 2.0f;
  const float outdoor = ((float) f[22] - 50.0f) / 2.0f;
  const bool led_on = f[24] != 0x70;

  // ---- update internal command state so the next 0x40 doesn't clobber ----
  this->st_power_ = power;
  if (mode >= 1 && mode <= 5)
    this->st_mode_ = mode;
  this->st_target_ = target;
  this->st_fan_ = fan;
  this->st_swing_ = swing;
  this->st_turbo_ = turbo;
  this->st_ion_ = ion;
  this->st_eco_ = eco;
  this->st_frost_ = frost;
  this->st_sleep_ = sleep;
  this->led_state_ = led_on;

  // ---- publish climate state ----
  if (!power) {
    this->mode = climate::CLIMATE_MODE_OFF;
  } else {
    this->mode = this->midea_mode_to_climate_(mode);
  }
  this->target_temperature = target;
  this->current_temperature = indoor;
  if (fan >= 101)
    this->fan_mode = climate::CLIMATE_FAN_AUTO;
  else if (fan <= 30)
    this->fan_mode = climate::CLIMATE_FAN_QUIET;
  else if (fan <= 50)
    this->fan_mode = climate::CLIMATE_FAN_LOW;
  else if (fan <= 70)
    this->fan_mode = climate::CLIMATE_FAN_MEDIUM;
  else
    this->fan_mode = climate::CLIMATE_FAN_HIGH;
  this->swing_mode = swing ? climate::CLIMATE_SWING_VERTICAL : climate::CLIMATE_SWING_OFF;
  if (frost)
    this->preset = climate::CLIMATE_PRESET_AWAY;
  else if (turbo)
    this->preset = climate::CLIMATE_PRESET_BOOST;
  else if (sleep)
    this->preset = climate::CLIMATE_PRESET_SLEEP;
  else if (eco)
    this->preset = climate::CLIMATE_PRESET_ECO;
  else
    this->preset = climate::CLIMATE_PRESET_NONE;
  this->publish_state();

  // ---- publish sub-entities ----
  if (this->outdoor_temperature_sensor_ != nullptr && f[22] != 0x00 && f[22] != 0xFF)
    this->outdoor_temperature_sensor_->publish_state(outdoor);
  if (this->ion_switch_ != nullptr)
    this->ion_switch_->publish_state(ion);
  if (this->led_switch_ != nullptr)
    this->led_switch_->publish_state(led_on);
  if (this->fan_number_ != nullptr && fan <= 100)
    this->fan_number_->publish_state((float) fan);
  if (this->compressor_freq_sensor_ != nullptr)
    this->compressor_freq_sensor_->publish_state(
        (float) f[23]);  // NOT frequency -- reads 8 off, 4 running. Status/state code.
}

void PortaSplitClimate::handle_c1_(const std::vector<uint8_t> &f) {
  if (f.size() < 30)
    return;
  if (f[13] == 0x41 && f.size() >= 26) {
    // Group 1 -- compressor diagnostics (TurboLed #256 / Midea reference source)
    // Contains: compressor freq, current, voltage, T1-T4, TP
    // Cross-verified: f[20-24] are temperatures using standard (byte-50)/2 formula
    // f[20]=T2 evap, f[22]=T1 indoor, f[23]=T4 outdoor (28.5 degC match), f[24]=TP discharge
    // f[14-17]: compressor data -- encoding uncertain, logging for analysis
    if (this->comp_frequency_sensor_ != nullptr)
      this->comp_frequency_sensor_->publish_state((float) f[14]);
    // f[18] is NOT current (reads 223 in standby). Log all non-temp bytes
    // for analysis; current field offset is still unknown.
    if (this->comp_current_sensor_ != nullptr) {
      // Disabled until the correct byte offset is identified.
      // this->comp_current_sensor_->publish_state((float) f[18] / 10.0f);
    }
    ESP_LOGD(TAG, "Group1: f14=%u f15=%u f16=%u f17=%u f18=%u f19=%u temps: %.1f %.1f %.1f %.1f %.1f", f[14], f[15],
             f[16], f[17], f[18], f[19], (f[20] - 50) / 2.0f, (f[21] - 50) / 2.0f, (f[22] - 50) / 2.0f,
             (f[23] - 50) / 2.0f, (f[24] - 50) / 2.0f);
    return;
  }
  if (f[13] == 0x42 && f.size() >= 16) {
    // Group 2 -- indoor fan RPM (TurboLed #256)
    // f[14]=target, f[15]=actual (observed: both 62 while AC off -- raw RPM or x10?)
    if (this->indoor_fan_rpm_sensor_ != nullptr)
      this->indoor_fan_rpm_sensor_->publish_state((float) f[15]);
    ESP_LOGD(TAG, "Group2: target=%u actual=%u", f[14], f[15]);
    return;
  }
  if (f[13] == 0x45) {
    // Group 5 -- environmental data (offsets confirmed via live sniffer captures):
    // humidity = frame byte 15, outdoor fan = frame byte 18 (x8 RPM), defrost = frame byte 20.
    if (f.size() < 22)
      return;
    // Humidity confirmed at frame byte 15 via live captures (values 61/67 %RH);
    // frame byte 14 is always 0x00 on this unit.
    if (this->humidity_sensor_ != nullptr && f[15] > 0 && f[15] <= 100)
      this->humidity_sensor_->publish_state((float) f[15]);
    if (this->outdoor_fan_sensor_ != nullptr)
      this->outdoor_fan_sensor_->publish_state((float) f[18] * 8.0f);  // x8 RPM
    // f[19]: drops with evap temp (27->20 observed) -- suction line temp in raw degC
    // NOT the (byte-50)/2 encoding used in C0; confirmed by standby readback
    // ((byte-50)/2 gives -11.5 degC which is nonsensical in standby)
    if (this->suction_temp_sensor_ != nullptr)
      this->suction_temp_sensor_->publish_state((float) f[19]);
    if (this->defrost_sensor_ != nullptr)
      this->defrost_sensor_->publish_state(f[20] != 0);
    // f[23]: monotonic counter, increments ~1/min while compressor runs.
    // NOT water level -- verified: does not reset after pump cycle.
    // Likely compressor runtime in minutes (or a cumulative operating counter).
    if (this->compressor_runtime_sensor_ != nullptr)
      this->compressor_runtime_sensor_->publish_state((float) f[23]);
    return;
  }
  if (f[13] != 0x44)
    return;  // Group 4 carries energy data
  float energy, power;
  if (this->bcd_energy_) {
    // Legacy BCD interpretation (digit pairs, whole value / 100 resp. / 10)
    energy = (float) bcd_(f[14]) * 10000.0f + (float) bcd_(f[15]) * 100.0f + (float) bcd_(f[16]) +
             (float) bcd_(f[17]) * 0.01f;
    power = (float) bcd_(f[26]) * 1000.0f + (float) bcd_(f[27]) * 10.0f + (float) bcd_(f[28]) * 0.1f;
  } else {
    // Binary interpretation (default). Verified against an external meter:
    // bytes 00 31 63 -> BCD misread 316.3 W, binary 0x3163 = 12643 * 0.1 = 1264.3 W (actual).
    const uint32_t e_raw =
        ((uint32_t) f[14] << 24) | ((uint32_t) f[15] << 16) | ((uint32_t) f[16] << 8) | (uint32_t) f[17];
    const uint32_t p_raw = ((uint32_t) f[26] << 16) | ((uint32_t) f[27] << 8) | (uint32_t) f[28];
    energy = (float) e_raw * 0.01f;  // kWh
    power = (float) p_raw * 0.1f;    // W
  }
  if (this->energy_sensor_ != nullptr)
    this->energy_sensor_->publish_state(energy);
  if (this->power_sensor_ != nullptr)
    this->power_sensor_->publish_state(power);
}

void PortaSplitClimate::handle_b1_b0_(const std::vector<uint8_t> &f) {
  // Payload: B1/B0 | COUNT | { ID_LO ID_HI STATUS LENGTH VALUE... } * COUNT
  if (f.size() < 13)
    return;
  const size_t end = f.size() - 2;  // exclude CRC8 + CS
  size_t i = 12;                    // first property entry
  const uint8_t count = f[11];
  for (uint8_t n = 0; n < count && i + 4 <= end; n++) {
    const uint16_t id = (uint16_t) f[i] | ((uint16_t) f[i + 1] << 8);
    const uint8_t status = f[i + 2];
    const uint8_t len = f[i + 3];
    const size_t val = i + 4;
    if (val + len > end)
      break;
    if (status == 0x00) {
      this->apply_property_(id, &f[val], len);
    }
    i = val + len;
  }
}

void PortaSplitClimate::apply_property_(uint16_t id, const uint8_t *value, uint8_t len) {
  switch (id) {
    case PROP_SILENT_MODE:
      if (len >= 1 && this->silent_switch_ != nullptr)
        this->silent_switch_->publish_state(value[0] != 0);
      break;
    case PROP_POWER_LIMIT:
      if (len >= 1 && this->power_limit_select_ != nullptr) {
        uint8_t v = value[0];
        this->power_limit_select_->publish_state(v <= 50 ? "50%" : (v <= 75 ? "75%" : "100%"));
      }
      break;
    case PROP_BEEPER:
      if (len >= 1 && this->beeper_switch_ != nullptr)
        this->beeper_switch_->publish_state(value[0] != 0);
      break;
    case PROP_SELF_CLEAN:
      if (len >= 1 && this->self_clean_switch_ != nullptr)
        this->self_clean_switch_->publish_state(value[0] != 0);
      break;
    case PROP_TEMP_RANGE:
      // [enable, min*2, max*2, FF, FF]
      if (len >= 3) {
        this->range_enable_ = value[0] != 0;
        if (value[1] >= 32 && value[1] <= 60)
          this->range_min_ = (float) value[1] / 2.0f;
        if (value[2] >= 32 && value[2] <= 60)
          this->range_max_ = (float) value[2] / 2.0f;
        if (this->temp_range_switch_ != nullptr)
          this->temp_range_switch_->publish_state(this->range_enable_);
        if (this->range_min_number_ != nullptr)
          this->range_min_number_->publish_state(this->range_min_);
        if (this->range_max_number_ != nullptr)
          this->range_max_number_->publish_state(this->range_max_);
      }
      break;
    case PROP_TELEM_TEMPS:
      // [0]=unknown (0x03 observed), then 6 x LE16 temperatures in 0.01 degC.
      // Verified mapping (cooling): t0/t1 indoor, t2 evaporator coil,
      // t3 unknown, t4 outdoor ambient (exact match with C0 byte 22),
      // t5 condenser coil.
      if (len >= 13) {
        float t[6];
        for (int k = 0; k < 6; k++)
          t[k] = (float) ((uint16_t) value[1 + 2 * k] | ((uint16_t) value[2 + 2 * k] << 8)) / 100.0f;
        ESP_LOGD(TAG, "Telemetry seq=%u: indoor=%.1f evap=%.1f disch=%.1f outdoor=%.1f cond=%.1f degC", value[0], t[0],
                 t[2], t[3], t[4], t[5]);
        if (this->evap_temp_sensor_ != nullptr)
          this->evap_temp_sensor_->publish_state(t[2]);
        if (this->discharge_temp_sensor_ != nullptr)
          this->discharge_temp_sensor_->publish_state(t[3]);
        if (this->cond_temp_sensor_ != nullptr)
          this->cond_temp_sensor_->publish_state(t[5]);
      }
      break;
    case 0x00E1:
    case 0x00E2:
      // Telemetry companions of 0x00E0 -- format not yet decoded.
      ESP_LOGD(TAG, "Telemetry property 0x%04X (len %u) -- undecoded", id, len);
      break;
    case PROP_SERIAL:
      if (this->serial_number_sensor_ != nullptr && len > 0) {
        std::string sn((const char *) value, len);
        this->serial_number_sensor_->publish_state(sn);
      }
      break;
    default:
      ESP_LOGV(TAG, "Property 0x%04X (len %u) not mapped", id, len);
      break;
  }
}

void PortaSplitClimate::handle_b5_(const std::vector<uint8_t> &f) {
  // f[9] = frame TYPE. Types 0x04/0x05 are unsolicited property/telemetry
  // notifications and use the same {ID_LO ID_HI LEN VALUE} layout as B1
  // (sniffer-verified: 0x05 carries 0x00E0 temps / 0x00E1 state / 0x00E2
  // compressor block; property-change pushes like power limit use it too).
  // Types 0x02/0x03 are capability responses whose values are "supported"
  // flags, NOT states -- those must never be routed into apply_property_.
  if (f.size() < 13)
    return;
  const uint8_t type = f[9];
  if (type == 0x05) {
    // The original module ACKs type-0x05 telemetry by echoing the frame
    // back verbatim (observed on the wire). Mimic that behavior.
    this->write_array(f);
  }
  if (type != 0x04 && type != 0x05) {
    // Unknown/capability B5 -- fall back to an early property re-read.
    this->last_props_poll_ = millis() - PROPS_INTERVAL + 1000;
    return;
  }
  const size_t end = f.size() - 2;  // exclude CRC8 + CS
  const uint8_t count = f[11];
  size_t i = 12;
  for (uint8_t n = 0; n < count && i + 3 <= end; n++) {
    const uint16_t id = (uint16_t) f[i] | ((uint16_t) f[i + 1] << 8);
    const uint8_t len = f[i + 2];
    const size_t val = i + 3;
    if (val + len > end)
      break;
    this->apply_property_(id, &f[val], len);
    i = val + len;
  }
}

void PortaSplitClimate::handle_a0_(const std::vector<uint8_t> &f) {
  if (f.size() < 16)
    return;
  ESP_LOGI(TAG, "Device info: type 0x%02X, firmware %02X.%02X.%02X", f[11], f[12], f[13], f[14]);
  if (this->firmware_sensor_ != nullptr) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%02X.%02X.%02X", f[12], f[13], f[14]);
    this->firmware_sensor_->publish_state(buf);
  }
}

// ================= mode mapping =================

uint8_t PortaSplitClimate::climate_mode_to_midea_(climate::ClimateMode m) {
  switch (m) {
    case climate::CLIMATE_MODE_HEAT_COOL:
      return 1;
    case climate::CLIMATE_MODE_COOL:
      return 2;
    case climate::CLIMATE_MODE_DRY:
      return 3;
    case climate::CLIMATE_MODE_HEAT:
      return 4;
    case climate::CLIMATE_MODE_FAN_ONLY:
      return 5;
    default:
      return 1;
  }
}

climate::ClimateMode PortaSplitClimate::midea_mode_to_climate_(uint8_t m) {
  switch (m) {
    case 1:
      return climate::CLIMATE_MODE_HEAT_COOL;
    case 2:
      return climate::CLIMATE_MODE_COOL;
    case 3:
      return climate::CLIMATE_MODE_DRY;
    case 4:
      return climate::CLIMATE_MODE_HEAT;
    case 5:
      return climate::CLIMATE_MODE_FAN_ONLY;
    default:
      return climate::CLIMATE_MODE_HEAT_COOL;
  }
}

// ---- sub-entity implementations ----

void PortaSplitSwitch::write_state(bool state) {
  if (this->parent_ != nullptr)
    this->parent_->switch_command(this->type_, state);
}

void PortaSplitSelect::control(const std::string &value) {
  if (this->parent_ != nullptr)
    this->parent_->select_command(this->type_, value);
}

void PortaSplitNumber::control(float value) {
  if (this->parent_ != nullptr)
    this->parent_->number_command(this->type_, value);
}

}  // namespace esphome::midea_portasplit
