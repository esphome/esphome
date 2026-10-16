#include "midea_portasplit.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::midea_portasplit {

ESPHOME_LOG_TAG(TAG, "midea_portasplit");

static constexpr uint32_t FOLLOWUP_TIMEOUT_ID = 0;

static constexpr uint32_t STATUS_INTERVAL_MS = 30000;
static constexpr uint32_t NETWORK_INTERVAL_MS = 10000;
static constexpr uint32_t FOLLOWUP_DELAY_MS = 1500;
// At 9600 baud bytes within a frame arrive about 1 ms apart.
static constexpr uint32_t RX_TIMEOUT_MS = 100;

// Ten header bytes plus CRC8 and checksum.
static constexpr size_t FRAME_OVERHEAD = 12;
static constexpr size_t MAX_TX_PAYLOAD = 24;

static constexpr float MIN_TARGET_TEMP = 16.0f;
static constexpr float MAX_TARGET_TEMP = 30.0f;

// The AC expects its Wi-Fi module to answer the 0x63 keepalive poll and to send periodic 0x0D network status
// frames. These frames use the XOR sync byte (0x1E ^ 0xAC = 0xB2). The AC does not use the IP address; the
// fixed frame below was captured from the original module.
static constexpr uint8_t MODULE_FRAME[31] PROGMEM = {0xAA, 0x1E, 0xAC, 0xB2, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x01,
                                                     0x01, 0x04, 0xC0, 0xA8, 0xB2, 0x72, 0x00, 0x00, 0x00, 0x01, 0x01,
                                                     0x01, 0x28, 0x09, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00};
static constexpr uint8_t STATUS_QUERY[12] PROGMEM = {0x41, 0x21, 0x00, 0xFF, 0x03, 0xFF,
                                                     0x00, 0x02, 0x00, 0x00, 0x00, 0x00};
// Same capability queries the original module sends at boot.
static constexpr uint8_t CAPS_0[] PROGMEM = {0xB5, 0x01, 0x00};
static constexpr uint8_t CAPS_1[] PROGMEM = {0xB5, 0x01, 0x01, 0x01};

// Two's complement of the sum of all bytes after the 0xAA start marker.
static uint8_t checksum(const uint8_t *frame, size_t len) { return (uint8_t) -sum_bytes(frame + 1, len - 1); }

static uint8_t climate_mode_to_midea(climate::ClimateMode mode) {
  switch (mode) {
    case climate::CLIMATE_MODE_COOL:
      return 2;
    case climate::CLIMATE_MODE_DRY:
      return 3;
    case climate::CLIMATE_MODE_HEAT:
      return 4;
    case climate::CLIMATE_MODE_FAN_ONLY:
      return 5;
    case climate::CLIMATE_MODE_HEAT_COOL:
    default:
      return 1;
  }
}

static climate::ClimateMode midea_mode_to_climate(uint8_t mode) {
  switch (mode) {
    case 2:
      return climate::CLIMATE_MODE_COOL;
    case 3:
      return climate::CLIMATE_MODE_DRY;
    case 4:
      return climate::CLIMATE_MODE_HEAT;
    case 5:
      return climate::CLIMATE_MODE_FAN_ONLY;
    case 1:
    default:
      return climate::CLIMATE_MODE_HEAT_COOL;
  }
}

static uint8_t fan_mode_to_midea(climate::ClimateFanMode mode) {
  switch (mode) {
    case climate::CLIMATE_FAN_QUIET:
      return 20;
    case climate::CLIMATE_FAN_LOW:
      return 40;
    case climate::CLIMATE_FAN_MEDIUM:
      return 60;
    case climate::CLIMATE_FAN_HIGH:
      return 80;
    default:
      return FAN_AUTO;
  }
}

// The AC reports its own fan steps (30 for low, 50 for medium), so classify by range.
static climate::ClimateFanMode midea_fan_to_climate(uint8_t fan) {
  if (fan > 100)
    return climate::CLIMATE_FAN_AUTO;
  if (fan <= 30)
    return climate::CLIMATE_FAN_QUIET;
  if (fan <= 50)
    return climate::CLIMATE_FAN_LOW;
  if (fan <= 70)
    return climate::CLIMATE_FAN_MEDIUM;
  return climate::CLIMATE_FAN_HIGH;
}

void PortaSplitClimate::setup() {
  // The status interval starts with a random offset of up to 5 s, which also serves as the boot query.
  this->set_timeout(10000, [this]() { this->send_capability_queries_(); });
  this->set_interval(STATUS_INTERVAL_MS, [this]() { this->send_status_query_(); });
  this->set_interval(NETWORK_INTERVAL_MS, [this]() { this->send_module_frame_(0x0D, this->net_counter_++, 0x08); });
}

void PortaSplitClimate::loop() {
  const uint32_t now = App.get_loop_component_start_time();
  uint8_t buf[32];
  size_t n;
  while ((n = std::min((size_t) this->available(), sizeof(buf))) != 0 && this->read_array(buf, n)) {
    for (size_t i = 0; i < n; i++)
      this->rx_feed_(buf[i]);
    this->rx_last_byte_ = now;
  }
  // The line went quiet mid-frame; drop it so its bytes cannot swallow the next one.
  if (this->rx_len_ != 0 && now - this->rx_last_byte_ > RX_TIMEOUT_MS)
    this->rx_len_ = 0;
}

void PortaSplitClimate::dump_config() {
  ESP_LOGCONFIG(TAG, "Midea PortaSplit:");
  this->dump_traits_(TAG);
}

climate::ClimateTraits PortaSplitClimate::traits() {
  auto traits = climate::ClimateTraits();
  traits.add_feature_flags(climate::CLIMATE_SUPPORTS_CURRENT_TEMPERATURE);
  traits.set_visual_min_temperature(MIN_TARGET_TEMP);
  traits.set_visual_max_temperature(MAX_TARGET_TEMP);
  traits.set_visual_temperature_step(0.5f);
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
      climate::CLIMATE_PRESET_NONE,
      climate::CLIMATE_PRESET_SLEEP,
      climate::CLIMATE_PRESET_BOOST,
      climate::CLIMATE_PRESET_ECO,
      climate::CLIMATE_PRESET_AWAY,  // Frost protection (8 degC heating)
  });
  return traits;
}

void PortaSplitClimate::control(const climate::ClimateCall &call) {
  if (call.get_mode().has_value()) {
    const climate::ClimateMode mode = *call.get_mode();
    this->st_power_ = mode != climate::CLIMATE_MODE_OFF;
    if (this->st_power_)
      this->st_mode_ = climate_mode_to_midea(mode);
    // Frost protection only runs while heating.
    if (mode != climate::CLIMATE_MODE_HEAT)
      this->st_frost_ = false;
  }
  if (call.get_target_temperature().has_value()) {
    const float target = clamp(*call.get_target_temperature(), MIN_TARGET_TEMP, MAX_TARGET_TEMP);
    this->st_target_half_ = (uint8_t) lroundf(target * 2.0f);
  }
  if (call.get_fan_mode().has_value())
    this->st_fan_ = fan_mode_to_midea(*call.get_fan_mode());
  if (call.get_swing_mode().has_value())
    this->st_swing_ = *call.get_swing_mode() == climate::CLIMATE_SWING_VERTICAL;
  if (call.get_preset().has_value()) {
    const climate::ClimatePreset preset = *call.get_preset();
    this->st_turbo_ = preset == climate::CLIMATE_PRESET_BOOST;
    this->st_sleep_ = preset == climate::CLIMATE_PRESET_SLEEP;
    this->st_eco_ = preset == climate::CLIMATE_PRESET_ECO;
    this->st_frost_ = preset == climate::CLIMATE_PRESET_AWAY;
    if (this->st_frost_) {
      this->st_mode_ = climate_mode_to_midea(climate::CLIMATE_MODE_HEAT);
      this->st_power_ = true;
    }
  }
  this->send_set_command_();
}

void PortaSplitClimate::send_frame_(uint8_t proto, uint8_t type, const uint8_t *payload, size_t len) {
  uint8_t f[FRAME_OVERHEAD + MAX_TX_PAYLOAD] = {0};
  const size_t total = FRAME_OVERHEAD + len;
  f[0] = 0xAA;
  f[1] = (uint8_t) (total - 1);
  f[2] = 0xAC;
  // f[3] is the sync byte and must stay 0x00 on the PortaSplit; f[4..7] are reserved.
  f[8] = proto;
  f[9] = type;
  progmem_memcpy(&f[10], payload, len);
  f[10 + len] = crc8(&f[10], (uint8_t) len);
  f[11 + len] = checksum(f, 11 + len);
  this->write_array(f, total);
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
  char hex_buf[format_hex_pretty_size(sizeof(f))];
  ESP_LOGV(TAG, "TX: %s", format_hex_pretty_to(hex_buf, f, total));
#endif
}

void PortaSplitClimate::send_set_command_() {
  uint8_t p[MAX_TX_PAYLOAD] = {0};
  const uint8_t whole = this->st_target_half_ >> 1;
  const bool half = this->st_target_half_ & 1;
  p[0] = 0x40;
  p[1] = 0x42;  // 0x02 is mandatory, 0x40 enables the beeper
  if (this->st_power_)
    p[1] |= 0x01;
  p[2] = (uint8_t) ((this->st_mode_ << 5) | (half ? 0x10 : 0x00) | ((whole - 16) & 0x0F));
  p[3] = this->st_fan_;
  p[4] = 0x7F;
  p[5] = 0x7F;
  p[6] = 0xF0;
  p[7] = this->st_swing_ ? 0x3C : 0x30;
  p[8] = this->st_turbo_ ? 0x20 : 0x00;
  p[9] = (uint8_t) ((this->st_ion_ ? 0x20 : 0x00) | (this->st_eco_ ? 0x80 : 0x00));
  p[10] = this->st_sleep_ ? 0x01 : 0x00;
  p[18] = (uint8_t) ((whole - 12) & 0x1F);
  p[19] = 0x80;
  p[21] = 0x0A;
  p[22] = this->st_frost_ ? 0x80 : 0x00;
  p[23] = this->msg_id_++;
  this->send_frame_(0x02, 0x02, p, sizeof(p));
  // Read back the state the AC actually applied.
  this->set_timeout(FOLLOWUP_TIMEOUT_ID, FOLLOWUP_DELAY_MS, [this]() { this->send_status_query_(); });
}

void PortaSplitClimate::send_status_query_() {
  uint8_t p[sizeof(STATUS_QUERY)];
  progmem_memcpy(p, STATUS_QUERY, sizeof(p));
  p[11] = this->msg_id_++;
  this->send_frame_(0x00, 0x03, p, sizeof(p));
}

void PortaSplitClimate::send_capability_queries_() {
  this->send_frame_(0x03, 0x03, CAPS_0, sizeof(CAPS_0));
  this->set_timeout(1000, [this]() { this->send_frame_(0x03, 0x03, CAPS_1, sizeof(CAPS_1)); });
}

void PortaSplitClimate::send_module_frame_(uint8_t type, uint8_t counter, uint8_t wifi_flags) {
  uint8_t f[sizeof(MODULE_FRAME)];
  progmem_memcpy(f, MODULE_FRAME, sizeof(f));
  f[6] = counter;
  f[9] = type;
  f[25] = wifi_flags;
  f[30] = checksum(f, 30);
  this->write_array(f, sizeof(f));
}

void PortaSplitClimate::rx_feed_(uint8_t b) {
  if (this->rx_len_ == 0 && b != 0xAA)
    return;
  this->rx_[this->rx_len_++] = b;
  if (this->rx_len_ < 3)
    return;
  const size_t total = (size_t) this->rx_[1] + 1;
  // Every frame carries the appliance type 0xAC; anything else is a false start.
  if (this->rx_len_ == 3 && (total < FRAME_OVERHEAD || total > MAX_FRAME || this->rx_[2] != 0xAC)) {
    this->rx_len_ = 0;
    return;
  }
  if (this->rx_len_ < total)
    return;
  this->rx_len_ = 0;
  if (checksum(this->rx_.data(), total - 1) != this->rx_[total - 1]) {
    ESP_LOGW(TAG, "RX checksum mismatch, dropping frame");
    return;
  }
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
  char hex_buf[format_hex_pretty_size(MAX_FRAME)];
  ESP_LOGV(TAG, "RX: %s", format_hex_pretty_to(hex_buf, this->rx_.data(), total));
#endif
  this->handle_frame_(this->rx_.data(), total);
}

void PortaSplitClimate::handle_frame_(const uint8_t *f, size_t len) {
  if (f[9] == 0x63) {
    this->send_module_frame_(0x63, 0, 0x00);
  } else if (f[9] == 0x05 && f[10] == 0xB5) {
    // The original module acknowledges telemetry pushes by echoing them back.
    this->write_array(f, len);
  } else if (f[10] == 0xC0) {
    this->handle_c0_(f, len);
  }
}

void PortaSplitClimate::handle_c0_(const uint8_t *f, size_t len) {
  if (len < 26)
    return;
  const uint8_t mode = (f[12] >> 5) & 0x07;
  const uint8_t target_half = (uint8_t) ((((f[12] & 0x0F) + 16) << 1) | ((f[12] & 0x10) ? 1 : 0));
  const climate::ClimateFanMode fan = midea_fan_to_climate(f[13]);

  this->st_power_ = f[11] & 0x01;
  if (mode >= 1 && mode <= 5)
    this->st_mode_ = mode;
  this->st_target_half_ = target_half;
  this->st_fan_ = fan_mode_to_midea(fan);
  this->st_swing_ = (f[17] & 0x0F) == 0x0C;
  this->st_turbo_ = f[18] & 0x20;
  this->st_ion_ = f[19] & 0x20;
  this->st_eco_ = f[19] & 0x10;  // Set command uses bit 7, readback uses bit 4.
  this->st_sleep_ = f[20] & 0x01;
  this->st_frost_ = len > 31 && (f[31] & 0x80);

  this->mode = this->st_power_ ? midea_mode_to_climate(this->st_mode_) : climate::CLIMATE_MODE_OFF;
  this->target_temperature = target_half / 2.0f;
  // 0x00 and 0xFF mean the indoor probe gave no reading.
  const uint8_t raw_temp = f[21];
  this->current_temperature = (raw_temp == 0x00 || raw_temp == 0xFF) ? NAN : ((float) raw_temp - 50.0f) / 2.0f;
  this->fan_mode = fan;
  this->swing_mode = this->st_swing_ ? climate::CLIMATE_SWING_VERTICAL : climate::CLIMATE_SWING_OFF;
  if (this->st_frost_) {
    this->preset = climate::CLIMATE_PRESET_AWAY;
  } else if (this->st_turbo_) {
    this->preset = climate::CLIMATE_PRESET_BOOST;
  } else if (this->st_sleep_) {
    this->preset = climate::CLIMATE_PRESET_SLEEP;
  } else if (this->st_eco_) {
    this->preset = climate::CLIMATE_PRESET_ECO;
  } else {
    this->preset = climate::CLIMATE_PRESET_NONE;
  }
  this->publish_state();
}

}  // namespace esphome::midea_portasplit
