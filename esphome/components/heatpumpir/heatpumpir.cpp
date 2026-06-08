#include "heatpumpir.h"

#if defined(USE_ARDUINO) || defined(USE_ESP32)

#include <cmath>
#include <map>
#include <IRSender.h>
#include <HeatpumpIRFactory.h>
#include "esphome/components/remote_base/remote_base.h"
#include "esphome/core/log.h"

namespace esphome::heatpumpir {

// IRSenderESPHome - bridge between ESPHome's remote_transmitter and HeatpumpIR library
// Defined here (not in a header) to isolate HeatpumpIR's headers from the rest of ESPHome,
// as they define conflicting symbols like millis() in the global namespace.
class IRSenderESPHome : public IRSender {
 public:
  IRSenderESPHome(remote_base::RemoteTransmitterBase *transmitter) : IRSender(0), transmit_(transmitter->transmit()) {}

  void setFrequency(int frequency) override {  // NOLINT(readability-identifier-naming)
    auto *data = this->transmit_.get_data();
    data->set_carrier_frequency(1000 * frequency);
  }

  void space(int space_length) override {
    if (space_length) {
      auto *data = this->transmit_.get_data();
      data->space(space_length);
    } else {
      this->transmit_.perform();
    }
  }

  void mark(int mark_length) override {
    auto *data = this->transmit_.get_data();
    data->mark(mark_length);
  }

 protected:
  remote_base::RemoteTransmitterBase::TransmitCall transmit_;
};

static const char *const TAG = "heatpumpir.climate";

const std::map<Protocol, std::function<HeatpumpIR *()>> PROTOCOL_CONSTRUCTOR_MAP = {
    {PROTOCOL_AUX, []() { return new AUXHeatpumpIR(); }},                                    // NOLINT
    {PROTOCOL_BALLU, []() { return new BalluHeatpumpIR(); }},                                // NOLINT
    {PROTOCOL_CARRIER_MCA, []() { return new CarrierMCAHeatpumpIR(); }},                     // NOLINT
    {PROTOCOL_CARRIER_NQV, []() { return new CarrierNQVHeatpumpIR(); }},                     // NOLINT
    {PROTOCOL_DAIKIN_ARC417, []() { return new DaikinHeatpumpARC417IR(); }},                 // NOLINT
    {PROTOCOL_DAIKIN_ARC480, []() { return new DaikinHeatpumpARC480A14IR(); }},              // NOLINT
    {PROTOCOL_DAIKIN, []() { return new DaikinHeatpumpIR(); }},                              // NOLINT
    {PROTOCOL_ELECTROLUXYAL, []() { return new ElectroluxYALHeatpumpIR(); }},                // NOLINT
    {PROTOCOL_FUEGO, []() { return new FuegoHeatpumpIR(); }},                                // NOLINT
    {PROTOCOL_FUJITSU_AWYZ, []() { return new FujitsuHeatpumpIR(); }},                       // NOLINT
    {PROTOCOL_GREE, []() { return new GreeGenericHeatpumpIR(); }},                           // NOLINT
    {PROTOCOL_GREEYAA, []() { return new GreeYAAHeatpumpIR(); }},                            // NOLINT
    {PROTOCOL_GREEYAN, []() { return new GreeYANHeatpumpIR(); }},                            // NOLINT
    {PROTOCOL_GREEYAC, []() { return new GreeYACHeatpumpIR(); }},                            // NOLINT
    {PROTOCOL_GREEYT, []() { return new GreeYTHeatpumpIR(); }},                              // NOLINT
    {PROTOCOL_GREEYAP, []() { return new GreeYAPHeatpumpIR(); }},                            // NOLINT
    {PROTOCOL_HISENSE_AUD, []() { return new HisenseHeatpumpIR(); }},                        // NOLINT
    {PROTOCOL_HITACHI, []() { return new HitachiHeatpumpIR(); }},                            // NOLINT
    {PROTOCOL_HYUNDAI, []() { return new HyundaiHeatpumpIR(); }},                            // NOLINT
    {PROTOCOL_IVT, []() { return new IVTHeatpumpIR(); }},                                    // NOLINT
    {PROTOCOL_MIDEA, []() { return new MideaHeatpumpIR(); }},                                // NOLINT
    {PROTOCOL_MITSUBISHI_FA, []() { return new MitsubishiFAHeatpumpIR(); }},                 // NOLINT
    {PROTOCOL_MITSUBISHI_FD, []() { return new MitsubishiFDHeatpumpIR(); }},                 // NOLINT
    {PROTOCOL_MITSUBISHI_FE, []() { return new MitsubishiFEHeatpumpIR(); }},                 // NOLINT
    {PROTOCOL_MITSUBISHI_HEAVY_FDTC, []() { return new MitsubishiHeavyFDTCHeatpumpIR(); }},  // NOLINT
    {PROTOCOL_MITSUBISHI_HEAVY_ZJ, []() { return new MitsubishiHeavyZJHeatpumpIR(); }},      // NOLINT
    {PROTOCOL_MITSUBISHI_HEAVY_ZM, []() { return new MitsubishiHeavyZMHeatpumpIR(); }},      // NOLINT
    {PROTOCOL_MITSUBISHI_HEAVY_ZMP, []() { return new MitsubishiHeavyZMPHeatpumpIR(); }},    // NOLINT
    {PROTOCOL_MITSUBISHI_KJ, []() { return new MitsubishiKJHeatpumpIR(); }},                 // NOLINT
    {PROTOCOL_MITSUBISHI_MSC, []() { return new MitsubishiMSCHeatpumpIR(); }},               // NOLINT
    {PROTOCOL_MITSUBISHI_MSY, []() { return new MitsubishiMSYHeatpumpIR(); }},               // NOLINT
    {PROTOCOL_MITSUBISHI_SEZ, []() { return new MitsubishiSEZKDXXHeatpumpIR(); }},           // NOLINT
    {PROTOCOL_PANASONIC_CKP, []() { return new PanasonicCKPHeatpumpIR(); }},                 // NOLINT
    {PROTOCOL_PANASONIC_DKE, []() { return new PanasonicDKEHeatpumpIR(); }},                 // NOLINT
    {PROTOCOL_PANASONIC_EKE, []() { return new PanasonicEKEHeatpumpIR(); }},                 // NOLINT
    {PROTOCOL_PANASONIC_JKE, []() { return new PanasonicJKEHeatpumpIR(); }},                 // NOLINT
    {PROTOCOL_PANASONIC_LKE, []() { return new PanasonicLKEHeatpumpIR(); }},                 // NOLINT
    {PROTOCOL_PANASONIC_NKE, []() { return new PanasonicNKEHeatpumpIR(); }},                 // NOLINT
    {PROTOCOL_SAMSUNG_AQV, []() { return new SamsungAQVHeatpumpIR(); }},                     // NOLINT
    {PROTOCOL_SAMSUNG_FJM, []() { return new SamsungFJMHeatpumpIR(); }},                     // NOLINT
    {PROTOCOL_SHARP, []() { return new SharpHeatpumpIR(); }},                                // NOLINT
    {PROTOCOL_TOSHIBA_DAISEIKAI, []() { return new ToshibaDaiseikaiHeatpumpIR(); }},         // NOLINT
    {PROTOCOL_TOSHIBA, []() { return new ToshibaHeatpumpIR(); }},                            // NOLINT
    {PROTOCOL_ZHLT01, []() { return new ZHLT01HeatpumpIR(); }},                              // NOLINT
    {PROTOCOL_NIBE, []() { return new NibeHeatpumpIR(); }},                                  // NOLINT
    {PROTOCOL_QLIMA_1, []() { return new Qlima1HeatpumpIR(); }},                             // NOLINT
    {PROTOCOL_QLIMA_2, []() { return new Qlima2HeatpumpIR(); }},                             // NOLINT
    {PROTOCOL_SAMSUNG_AQV12MSAN, []() { return new SamsungAQV12MSANHeatpumpIR(); }},         // NOLINT
    {PROTOCOL_ZHJG01, []() { return new ZHJG01HeatpumpIR(); }},                              // NOLINT
    {PROTOCOL_AIRWAY, []() { return new AIRWAYHeatpumpIR(); }},                              // NOLINT
    {PROTOCOL_BGH_AUD, []() { return new BGHHeatpumpIR(); }},                                // NOLINT
    {PROTOCOL_PANASONIC_ALTDKE, []() { return new PanasonicAltDKEHeatpumpIR(); }},           // NOLINT
    {PROTOCOL_PHILCO_PHS32, []() { return new PhilcoPHS32HeatpumpIR(); }},                   // NOLINT
    {PROTOCOL_VAILLANTVAI8, []() { return new VaillantHeatpumpIR(); }},                      // NOLINT
    {PROTOCOL_R51M, []() { return new R51MHeatpumpIR(); }},                                  // NOLINT
};

void HeatpumpIRClimate::setup() {
  auto protocol_constructor = PROTOCOL_CONSTRUCTOR_MAP.find(protocol_);
  if (protocol_constructor == PROTOCOL_CONSTRUCTOR_MAP.end()) {
    ESP_LOGE(TAG, "Invalid protocol");
    return;
  }
  this->heatpump_ir_ = protocol_constructor->second();
  climate_ir::ClimateIR::setup();
  if (this->sensor_) {
    this->sensor_->add_on_state_callback([this](float state) {
      this->current_temperature = state;

      IRSenderESPHome esp_sender(this->transmitter_);
      this->heatpump_ir_->send(esp_sender, uint8_t(std::lround(this->current_temperature)));

      // current temperature changed, publish state
      this->publish_state();
    });
    this->current_temperature = this->sensor_->state;
  } else {
    this->current_temperature = NAN;
  }
}

void HeatpumpIRClimate::transmit_state() {
  uint8_t power_mode_cmd;
  uint8_t operating_mode_cmd;
  uint8_t temperature_cmd;
  uint8_t fan_speed_cmd;

  uint8_t swing_v_cmd;
  switch (default_vertical_direction_) {
    case VERTICAL_DIRECTION_AUTO:
      swing_v_cmd = VDIR_AUTO;
      break;
    case VERTICAL_DIRECTION_UP:
      swing_v_cmd = VDIR_UP;
      break;
    case VERTICAL_DIRECTION_MUP:
      swing_v_cmd = VDIR_MUP;
      break;
    case VERTICAL_DIRECTION_MIDDLE:
      swing_v_cmd = VDIR_MIDDLE;
      break;
    case VERTICAL_DIRECTION_MDOWN:
      swing_v_cmd = VDIR_MDOWN;
      break;
    case VERTICAL_DIRECTION_DOWN:
      swing_v_cmd = VDIR_DOWN;
      break;
    default:
      ESP_LOGE(TAG, "Invalid default vertical direction");
      return;
  }
  if ((this->swing_mode == climate::CLIMATE_SWING_VERTICAL) || (this->swing_mode == climate::CLIMATE_SWING_BOTH)) {
    swing_v_cmd = VDIR_SWING;
  }

  uint8_t swing_h_cmd;
  switch (default_horizontal_direction_) {
    case HORIZONTAL_DIRECTION_AUTO:
      swing_h_cmd = HDIR_AUTO;
      break;
    case HORIZONTAL_DIRECTION_MIDDLE:
      swing_h_cmd = HDIR_MIDDLE;
      break;
    case HORIZONTAL_DIRECTION_LEFT:
      swing_h_cmd = HDIR_LEFT;
      break;
    case HORIZONTAL_DIRECTION_MLEFT:
      swing_h_cmd = HDIR_MLEFT;
      break;
    case HORIZONTAL_DIRECTION_MRIGHT:
      swing_h_cmd = HDIR_MRIGHT;
      break;
    case HORIZONTAL_DIRECTION_RIGHT:
      swing_h_cmd = HDIR_RIGHT;
      break;
    default:
      ESP_LOGE(TAG, "Invalid default horizontal direction");
      return;
  }
  if ((this->swing_mode == climate::CLIMATE_SWING_HORIZONTAL) || (this->swing_mode == climate::CLIMATE_SWING_BOTH)) {
    swing_h_cmd = HDIR_SWING;
  }

  switch (this->fan_mode.value_or(climate::CLIMATE_FAN_AUTO)) {
    case climate::CLIMATE_FAN_LOW:
      fan_speed_cmd = FAN_2;
      break;
    case climate::CLIMATE_FAN_MEDIUM:
      fan_speed_cmd = FAN_3;
      break;
    case climate::CLIMATE_FAN_HIGH:
      fan_speed_cmd = FAN_4;
      break;
    case climate::CLIMATE_FAN_AUTO:
    default:
      fan_speed_cmd = FAN_AUTO;
      break;
  }

  switch (this->mode) {
    case climate::CLIMATE_MODE_COOL:
      power_mode_cmd = POWER_ON;
      operating_mode_cmd = MODE_COOL;
      break;
    case climate::CLIMATE_MODE_HEAT:
      power_mode_cmd = POWER_ON;
      operating_mode_cmd = MODE_HEAT;
      break;
    // Map HEAT_COOL to hardware AUTO mode (automatic heat/cool changeover based on temperature).
    // In hardware AUTO mode, the device automatically switches between heating and cooling
    // based on the current temperature versus the target temperature.
    // See https://github.com/esphome/esphome/issues/11161 for further discussion.
    case climate::CLIMATE_MODE_HEAT_COOL:
    case climate::CLIMATE_MODE_AUTO:
      power_mode_cmd = POWER_ON;
      operating_mode_cmd = MODE_AUTO;
      break;
    case climate::CLIMATE_MODE_FAN_ONLY:
      power_mode_cmd = POWER_ON;
      operating_mode_cmd = MODE_FAN;
      break;
    case climate::CLIMATE_MODE_DRY:
      power_mode_cmd = POWER_ON;
      operating_mode_cmd = MODE_DRY;
      break;
    case climate::CLIMATE_MODE_OFF:
    default:
      power_mode_cmd = POWER_OFF;
      operating_mode_cmd = MODE_AUTO;
      break;
  }

  temperature_cmd = (uint8_t) clamp(this->target_temperature, this->min_temperature_, this->max_temperature_);

  IRSenderESPHome esp_sender(this->transmitter_);
  heatpump_ir_->send(esp_sender, power_mode_cmd, operating_mode_cmd, fan_speed_cmd, temperature_cmd, swing_v_cmd,
                     swing_h_cmd);
}

static const uint16_t MITSUBISHI_HEAVY_ZJ_HDR_MARK = 3200;
static const uint16_t MITSUBISHI_HEAVY_ZJ_HDR_SPACE = 1600;
static const uint16_t MITSUBISHI_HEAVY_ZJ_BIT_MARK = 400;
static const uint16_t MITSUBISHI_HEAVY_ZJ_ONE_SPACE = 1200;
static const uint16_t MITSUBISHI_HEAVY_ZJ_ZERO_SPACE = 400;
static const uint8_t MITSUBISHI_HEAVY_ZJ_STATE_LENGTH = 11;

static const uint8_t MITSUBISHI_HEAVY_ZJ_BYTE0 = 0x52;
static const uint8_t MITSUBISHI_HEAVY_ZJ_BYTE1 = 0xAE;
static const uint8_t MITSUBISHI_HEAVY_ZJ_BYTE2 = 0xC3;
static const uint8_t MITSUBISHI_HEAVY_ZJ_BYTE3 = 0x26;
static const uint8_t MITSUBISHI_HEAVY_ZJ_BYTE4 = 0xD9;

static const uint8_t MITSUBISHI_HEAVY_ZJ_MODE_AUTO = 0x07;
static const uint8_t MITSUBISHI_HEAVY_ZJ_MODE_HEAT = 0x03;
static const uint8_t MITSUBISHI_HEAVY_ZJ_MODE_COOL = 0x06;
static const uint8_t MITSUBISHI_HEAVY_ZJ_MODE_DRY = 0x05;
static const uint8_t MITSUBISHI_HEAVY_ZJ_MODE_FAN = 0x04;
static const uint8_t MITSUBISHI_HEAVY_ZJ_POWER_OFF = 0x08;

static const uint8_t MITSUBISHI_HEAVY_ZJ_FAN_AUTO = 0xE0;
static const uint8_t MITSUBISHI_HEAVY_ZJ_FAN1 = 0xA0;
static const uint8_t MITSUBISHI_HEAVY_ZJ_FAN2 = 0x80;
static const uint8_t MITSUBISHI_HEAVY_ZJ_FAN3 = 0x60;
static const uint8_t MITSUBISHI_HEAVY_ZJ_HIPOWER = 0x40;
static const uint8_t MITSUBISHI_HEAVY_ZJ_ECONO = 0x00;

static const uint8_t MITSUBISHI_HEAVY_ZJ_HS_SWING = 0x4C;
static const uint8_t MITSUBISHI_HEAVY_ZJ_VS_SWING = 0x0A;

bool HeatpumpIRClimate::on_receive(remote_base::RemoteReceiveData data) {
  if (this->protocol_ != PROTOCOL_MITSUBISHI_HEAVY_ZJ)
    return false;

  uint8_t frame[11] = {};

  if (!data.expect_item(MITSUBISHI_HEAVY_ZJ_HDR_MARK, MITSUBISHI_HEAVY_ZJ_HDR_SPACE)) {
    return false;
  }

  for (uint8_t pos = 0; pos < MITSUBISHI_HEAVY_ZJ_STATE_LENGTH; pos++) {
    uint8_t byte = 0;
    for (int8_t bit = 0; bit < 8; bit++) {
      if (data.expect_item(MITSUBISHI_HEAVY_ZJ_BIT_MARK, MITSUBISHI_HEAVY_ZJ_ONE_SPACE)) {
        byte |= 1 << bit;
      } else if (!data.expect_item(MITSUBISHI_HEAVY_ZJ_BIT_MARK, MITSUBISHI_HEAVY_ZJ_ZERO_SPACE)) {
        return false;
      }
    }
    frame[pos] = byte;

    if ((pos == 0 && byte != MITSUBISHI_HEAVY_ZJ_BYTE0) || (pos == 1 && byte != MITSUBISHI_HEAVY_ZJ_BYTE1) ||
        (pos == 2 && byte != MITSUBISHI_HEAVY_ZJ_BYTE2) || (pos == 3 && byte != MITSUBISHI_HEAVY_ZJ_BYTE3) ||
        (pos == 4 && byte != MITSUBISHI_HEAVY_ZJ_BYTE4)) {
      return false;
    }
  }

  if ((uint8_t) (frame[5] ^ frame[6]) != 0xFF || (uint8_t) (frame[7] ^ frame[8]) != 0xFF ||
      (uint8_t) (frame[9] ^ frame[10]) != 0xFF) {
    return false;
  }

  if (frame[9] & MITSUBISHI_HEAVY_ZJ_POWER_OFF) {
    this->mode = climate::CLIMATE_MODE_OFF;
  } else {
    uint8_t opmode = frame[9] & 0x07;
    switch (opmode) {
      case MITSUBISHI_HEAVY_ZJ_MODE_AUTO:
        this->mode = climate::CLIMATE_MODE_HEAT_COOL;
        break;
      case MITSUBISHI_HEAVY_ZJ_MODE_HEAT:
        this->mode = climate::CLIMATE_MODE_HEAT;
        break;
      case MITSUBISHI_HEAVY_ZJ_MODE_COOL:
        this->mode = climate::CLIMATE_MODE_COOL;
        break;
      case MITSUBISHI_HEAVY_ZJ_MODE_DRY:
        this->mode = climate::CLIMATE_MODE_DRY;
        break;
      case MITSUBISHI_HEAVY_ZJ_MODE_FAN:
        this->mode = climate::CLIMATE_MODE_FAN_ONLY;
        break;
      default:
        return false;
    }
  }

  uint8_t temp_nibble = (frame[9] >> 4) & 0x0F;
  this->target_temperature = 17 + ((~temp_nibble) & 0x0F);

  uint8_t fan = frame[7] & 0xE0;
  if (fan == MITSUBISHI_HEAVY_ZJ_FAN_AUTO) {
    this->fan_mode = climate::CLIMATE_FAN_AUTO;
    this->preset = climate::CLIMATE_PRESET_NONE;
  } else if (fan == MITSUBISHI_HEAVY_ZJ_FAN1) {
    this->fan_mode = climate::CLIMATE_FAN_LOW;
    this->preset = climate::CLIMATE_PRESET_NONE;
  } else if (fan == MITSUBISHI_HEAVY_ZJ_FAN2) {
    this->fan_mode = climate::CLIMATE_FAN_MEDIUM;
    this->preset = climate::CLIMATE_PRESET_NONE;
  } else if (fan == MITSUBISHI_HEAVY_ZJ_FAN3) {
    this->fan_mode = climate::CLIMATE_FAN_HIGH;
    this->preset = climate::CLIMATE_PRESET_NONE;
  } else if (fan == MITSUBISHI_HEAVY_ZJ_HIPOWER) {
    this->preset = climate::CLIMATE_PRESET_BOOST;
  } else if (fan == MITSUBISHI_HEAVY_ZJ_ECONO) {
    this->preset = climate::CLIMATE_PRESET_ECO;
  }

  uint8_t swing_h = frame[5] & 0xDC;
  uint8_t swing_v = (frame[5] & 0x02) | (frame[7] & 0x18);

  bool h_swing = (swing_h == MITSUBISHI_HEAVY_ZJ_HS_SWING);
  bool v_swing = (swing_v == MITSUBISHI_HEAVY_ZJ_VS_SWING);

  if (h_swing && v_swing) {
    this->swing_mode = climate::CLIMATE_SWING_BOTH;
  } else if (h_swing) {
    this->swing_mode = climate::CLIMATE_SWING_HORIZONTAL;
  } else if (v_swing) {
    this->swing_mode = climate::CLIMATE_SWING_VERTICAL;
  } else {
    this->swing_mode = climate::CLIMATE_SWING_OFF;
  }

  this->publish_state();
  return true;
}

}  // namespace esphome::heatpumpir

#endif
