#include "heatpumpir.h"

#include "receiver_mitsubishi_heavy.h"

#if defined(USE_ARDUINO) || defined(USE_ESP32)

#include <cmath>
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

ESPHOME_LOG_TAG(TAG, "heatpumpir.climate");

template<Protocol P> struct ProtocolClass;

template<Protocol P> HeatpumpIR *create_heatpump_ir() {
  return new typename ProtocolClass<P>::type();  // NOLINT
}

// Codegen calls only the configured protocols, so the linker drops the other classes
#define HEATPUMPIR_PROTOCOL(protocol, cls) \
  template<> struct ProtocolClass<protocol> { \
    using type = cls; \
  }; \
  template HeatpumpIR *create_heatpump_ir<protocol>();

HEATPUMPIR_PROTOCOL(PROTOCOL_AUX, AUXHeatpumpIR)
HEATPUMPIR_PROTOCOL(PROTOCOL_BALLU, BalluHeatpumpIR)
HEATPUMPIR_PROTOCOL(PROTOCOL_CARRIER_MCA, CarrierMCAHeatpumpIR)
HEATPUMPIR_PROTOCOL(PROTOCOL_CARRIER_NQV, CarrierNQVHeatpumpIR)
HEATPUMPIR_PROTOCOL(PROTOCOL_DAIKIN_ARC417, DaikinHeatpumpARC417IR)
HEATPUMPIR_PROTOCOL(PROTOCOL_DAIKIN_ARC480, DaikinHeatpumpARC480A14IR)
HEATPUMPIR_PROTOCOL(PROTOCOL_DAIKIN, DaikinHeatpumpIR)
HEATPUMPIR_PROTOCOL(PROTOCOL_ELECTROLUXYAL, ElectroluxYALHeatpumpIR)
HEATPUMPIR_PROTOCOL(PROTOCOL_FUEGO, FuegoHeatpumpIR)
HEATPUMPIR_PROTOCOL(PROTOCOL_FUJITSU_AWYZ, FujitsuHeatpumpIR)
HEATPUMPIR_PROTOCOL(PROTOCOL_GREE, GreeGenericHeatpumpIR)
HEATPUMPIR_PROTOCOL(PROTOCOL_GREEYAA, GreeYAAHeatpumpIR)
HEATPUMPIR_PROTOCOL(PROTOCOL_GREEYAN, GreeYANHeatpumpIR)
HEATPUMPIR_PROTOCOL(PROTOCOL_GREEYAC, GreeYACHeatpumpIR)
HEATPUMPIR_PROTOCOL(PROTOCOL_GREEYT, GreeYTHeatpumpIR)
HEATPUMPIR_PROTOCOL(PROTOCOL_GREEYAP, GreeYAPHeatpumpIR)
HEATPUMPIR_PROTOCOL(PROTOCOL_HISENSE_AUD, HisenseHeatpumpIR)
HEATPUMPIR_PROTOCOL(PROTOCOL_HITACHI, HitachiHeatpumpIR)
HEATPUMPIR_PROTOCOL(PROTOCOL_HYUNDAI, HyundaiHeatpumpIR)
HEATPUMPIR_PROTOCOL(PROTOCOL_IVT, IVTHeatpumpIR)
HEATPUMPIR_PROTOCOL(PROTOCOL_MIDEA, MideaHeatpumpIR)
HEATPUMPIR_PROTOCOL(PROTOCOL_MITSUBISHI_FA, MitsubishiFAHeatpumpIR)
HEATPUMPIR_PROTOCOL(PROTOCOL_MITSUBISHI_FD, MitsubishiFDHeatpumpIR)
HEATPUMPIR_PROTOCOL(PROTOCOL_MITSUBISHI_FE, MitsubishiFEHeatpumpIR)
HEATPUMPIR_PROTOCOL(PROTOCOL_MITSUBISHI_HEAVY_FDTC, MitsubishiHeavyFDTCHeatpumpIR)
HEATPUMPIR_PROTOCOL(PROTOCOL_MITSUBISHI_HEAVY_ZJ, MitsubishiHeavyZJHeatpumpIR)
HEATPUMPIR_PROTOCOL(PROTOCOL_MITSUBISHI_HEAVY_ZM, MitsubishiHeavyZMHeatpumpIR)
HEATPUMPIR_PROTOCOL(PROTOCOL_MITSUBISHI_HEAVY_ZMP, MitsubishiHeavyZMPHeatpumpIR)
HEATPUMPIR_PROTOCOL(PROTOCOL_MITSUBISHI_KJ, MitsubishiKJHeatpumpIR)
HEATPUMPIR_PROTOCOL(PROTOCOL_MITSUBISHI_MSC, MitsubishiMSCHeatpumpIR)
HEATPUMPIR_PROTOCOL(PROTOCOL_MITSUBISHI_MSY, MitsubishiMSYHeatpumpIR)
HEATPUMPIR_PROTOCOL(PROTOCOL_MITSUBISHI_SEZ, MitsubishiSEZKDXXHeatpumpIR)
HEATPUMPIR_PROTOCOL(PROTOCOL_PANASONIC_CKP, PanasonicCKPHeatpumpIR)
HEATPUMPIR_PROTOCOL(PROTOCOL_PANASONIC_DKE, PanasonicDKEHeatpumpIR)
HEATPUMPIR_PROTOCOL(PROTOCOL_PANASONIC_EKE, PanasonicEKEHeatpumpIR)
HEATPUMPIR_PROTOCOL(PROTOCOL_PANASONIC_JKE, PanasonicJKEHeatpumpIR)
HEATPUMPIR_PROTOCOL(PROTOCOL_PANASONIC_LKE, PanasonicLKEHeatpumpIR)
HEATPUMPIR_PROTOCOL(PROTOCOL_PANASONIC_NKE, PanasonicNKEHeatpumpIR)
HEATPUMPIR_PROTOCOL(PROTOCOL_SAMSUNG_AQV, SamsungAQVHeatpumpIR)
HEATPUMPIR_PROTOCOL(PROTOCOL_SAMSUNG_FJM, SamsungFJMHeatpumpIR)
HEATPUMPIR_PROTOCOL(PROTOCOL_SHARP, SharpHeatpumpIR)
HEATPUMPIR_PROTOCOL(PROTOCOL_TOSHIBA_DAISEIKAI, ToshibaDaiseikaiHeatpumpIR)
HEATPUMPIR_PROTOCOL(PROTOCOL_TOSHIBA, ToshibaHeatpumpIR)
HEATPUMPIR_PROTOCOL(PROTOCOL_ZHLT01, ZHLT01HeatpumpIR)
HEATPUMPIR_PROTOCOL(PROTOCOL_NIBE, NibeHeatpumpIR)
HEATPUMPIR_PROTOCOL(PROTOCOL_QLIMA_1, Qlima1HeatpumpIR)
HEATPUMPIR_PROTOCOL(PROTOCOL_QLIMA_2, Qlima2HeatpumpIR)
HEATPUMPIR_PROTOCOL(PROTOCOL_SAMSUNG_AQV12MSAN, SamsungAQV12MSANHeatpumpIR)
HEATPUMPIR_PROTOCOL(PROTOCOL_ZHJG01, ZHJG01HeatpumpIR)
HEATPUMPIR_PROTOCOL(PROTOCOL_AIRWAY, AIRWAYHeatpumpIR)
HEATPUMPIR_PROTOCOL(PROTOCOL_BGH_AUD, BGHHeatpumpIR)
HEATPUMPIR_PROTOCOL(PROTOCOL_PANASONIC_ALTDKE, PanasonicAltDKEHeatpumpIR)
HEATPUMPIR_PROTOCOL(PROTOCOL_PHILCO_PHS32, PhilcoPHS32HeatpumpIR)
HEATPUMPIR_PROTOCOL(PROTOCOL_VAILLANTVAI8, VaillantHeatpumpIR)
HEATPUMPIR_PROTOCOL(PROTOCOL_R51M, R51MHeatpumpIR)

#undef HEATPUMPIR_PROTOCOL

void HeatpumpIRClimate::setup() {
  climate_ir::ClimateIR::setup();
  if (this->temperature_sensor_) {
    this->temperature_sensor_->add_on_state_callback([this](float state) {
      this->current_temperature = state;

      IRSenderESPHome esp_sender(this->transmitter_);
      this->heatpump_ir_->send(esp_sender, uint8_t(std::lround(this->current_temperature)));

      // current temperature changed, publish state
      this->publish_state();
    });
    this->current_temperature = this->temperature_sensor_->state;
  } else {
    this->current_temperature = NAN;
  }
}

void HeatpumpIRClimate::control(const climate::ClimateCall &call) {
  // Picking a fan speed ends HiPower or Econo, as it does on the remote
  if (this->has_fan_byte_presets_() && call.get_fan_mode().has_value() && !call.get_preset().has_value())
    this->preset = climate::CLIMATE_PRESET_NONE;
  climate_ir::ClimateIR::control(call);
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

  // MitsubishiMSCHeatpumpIR only knows FAN_1..FAN_3 and sends anything else as auto,
  // so low/medium/high have to start at FAN_1 there instead of FAN_2. Mitsubishi Heavy
  // ZJ and ZMP also stop at FAN_3 and use FAN_4 and FAN_5 for HiPower and Econo.
  const bool three_speed_fan = this->protocol_ == PROTOCOL_MITSUBISHI_MSC || this->has_fan_byte_presets_();
  switch (this->fan_mode.value_or(climate::CLIMATE_FAN_AUTO)) {
    case climate::CLIMATE_FAN_LOW:
      fan_speed_cmd = three_speed_fan ? FAN_1 : FAN_2;
      break;
    case climate::CLIMATE_FAN_MEDIUM:
      fan_speed_cmd = three_speed_fan ? FAN_2 : FAN_3;
      break;
    case climate::CLIMATE_FAN_HIGH:
      fan_speed_cmd = three_speed_fan ? FAN_3 : FAN_4;
      break;
    case climate::CLIMATE_FAN_AUTO:
    default:
      fan_speed_cmd = FAN_AUTO;
      break;
  }

  if (this->has_fan_byte_presets_()) {
    switch (this->preset.value_or(climate::CLIMATE_PRESET_NONE)) {
      case climate::CLIMATE_PRESET_BOOST:
        fan_speed_cmd = FAN_4;  // HiPower
        break;
      case climate::CLIMATE_PRESET_ECO:
        fan_speed_cmd = FAN_5;  // Econo
        break;
      default:
        break;
    }
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

bool HeatpumpIRClimate::on_receive(remote_base::RemoteReceiveData data) {
  switch (this->protocol_) {
    case PROTOCOL_MITSUBISHI_HEAVY_ZMP: {
      uint8_t frame[MITSUBISHI_HEAVY_FRAME_LENGTH];
      if (!decode_mitsubishi_heavy_frame(data, frame, *this))
        return false;
      decode_mitsubishi_heavy_zmp(frame, *this);
      break;
    }
    default:
      return false;  // no decoder for this protocol
  }
  this->target_temperature = clamp(this->target_temperature, this->min_temperature_, this->max_temperature_);
  this->publish_state();
  return true;
}

}  // namespace esphome::heatpumpir

#endif
