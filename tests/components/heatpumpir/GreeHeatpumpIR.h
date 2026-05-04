#pragma once

#include <algorithm>
#include <cstdint>

#include "HeatpumpIR.h"
#if __has_include("../esphome/components/gree/gree.h")
#include "../esphome/components/gree/gree.h"
#include "../esphome/components/remote_base/remote_base.h"

namespace heatpumpir_test_stub {

class MockRemoteTransmitter : public esphome::remote_base::RemoteTransmitterBase {
 public:
  MockRemoteTransmitter() : RemoteTransmitterBase(nullptr) {}

  const esphome::remote_base::RawTimings &last_data() const { return this->last_data_; }
  uint32_t last_carrier_frequency() const { return this->last_carrier_frequency_; }

 protected:
  void send_internal(uint32_t, uint32_t) override {
    this->last_data_ = this->temp_.get_data();
    this->last_carrier_frequency_ = this->temp_.get_carrier_frequency();
  }

 private:
  esphome::remote_base::RawTimings last_data_{};
  uint32_t last_carrier_frequency_{0};
};

class TestableGreeClimate : public esphome::gree::GreeClimate {
 public:
  using GreeClimate::transmit_state;
  void set_mode_bits_for_test(uint8_t mode_bits) { this->mode_bits_ = mode_bits; }
};

inline esphome::climate::ClimateMode climate_mode_for_command(uint8_t power_mode_cmd, uint8_t operating_mode_cmd) {
  if (power_mode_cmd == POWER_OFF) {
    return esphome::climate::CLIMATE_MODE_OFF;
  }

  switch (operating_mode_cmd) {
    case MODE_AUTO:
      return esphome::climate::CLIMATE_MODE_HEAT_COOL;
    case MODE_HEAT:
      return esphome::climate::CLIMATE_MODE_HEAT;
    case MODE_COOL:
      return esphome::climate::CLIMATE_MODE_COOL;
    case MODE_DRY:
      return esphome::climate::CLIMATE_MODE_DRY;
    case MODE_FAN:
      return esphome::climate::CLIMATE_MODE_FAN_ONLY;
    default:
      return esphome::climate::CLIMATE_MODE_HEAT_COOL;
  }
}

inline esphome::climate::ClimateFanMode climate_fan_mode_for_command(uint8_t fan_speed_cmd,
                                                                     uint8_t operating_mode_cmd) {
  if (operating_mode_cmd == MODE_DRY) {
    return esphome::climate::CLIMATE_FAN_LOW;
  }

  switch (fan_speed_cmd) {
    case FAN_2:
      return esphome::climate::CLIMATE_FAN_LOW;
    case FAN_3:
      return esphome::climate::CLIMATE_FAN_MEDIUM;
    case FAN_4:
      return esphome::climate::CLIMATE_FAN_HIGH;
    case FAN_AUTO:
    default:
      return esphome::climate::CLIMATE_FAN_AUTO;
  }
}

inline esphome::gree::VerticalDirections vertical_direction_for_command(uint8_t swing_v_cmd) {
  switch (swing_v_cmd) {
    case VDIR_UP:
      return esphome::gree::VERTICAL_DIRECTION_UP;
    case VDIR_MUP:
      return esphome::gree::VERTICAL_DIRECTION_MUP;
    case VDIR_MIDDLE:
      return esphome::gree::VERTICAL_DIRECTION_MIDDLE;
    case VDIR_MDOWN:
      return esphome::gree::VERTICAL_DIRECTION_MDOWN;
    case VDIR_DOWN:
      return esphome::gree::VERTICAL_DIRECTION_DOWN;
    case VDIR_AUTO:
    default:
      return esphome::gree::VERTICAL_DIRECTION_AUTO;
  }
}

inline esphome::gree::HorizontalDirections horizontal_direction_for_command(uint8_t swing_h_cmd) {
  switch (swing_h_cmd) {
    case HDIR_LEFT:
      return esphome::gree::HORIZONTAL_DIRECTION_LEFT;
    case HDIR_MLEFT:
      return esphome::gree::HORIZONTAL_DIRECTION_MLEFT;
    case HDIR_MIDDLE:
      return esphome::gree::HORIZONTAL_DIRECTION_MIDDLE;
    case HDIR_MRIGHT:
      return esphome::gree::HORIZONTAL_DIRECTION_MRIGHT;
    case HDIR_RIGHT:
      return esphome::gree::HORIZONTAL_DIRECTION_RIGHT;
    case HDIR_AUTO:
    default:
      return esphome::gree::HORIZONTAL_DIRECTION_AUTO;
  }
}

inline esphome::climate::ClimateSwingMode swing_mode_for_commands(esphome::gree::Model model, uint8_t swing_v_cmd,
                                                                  uint8_t swing_h_cmd) {
  bool vertical_swing = swing_v_cmd == VDIR_SWING;
  bool horizontal_swing = swing_h_cmd == HDIR_SWING;

  if (model == esphome::gree::GREE_YAN && vertical_swing) {
    vertical_swing = false;
  }
  if (model == esphome::gree::GREE_YAC && swing_h_cmd == HDIR_AUTO) {
    horizontal_swing = true;
  }

  if (vertical_swing && horizontal_swing) {
    return esphome::climate::CLIMATE_SWING_BOTH;
  }
  if (vertical_swing) {
    return esphome::climate::CLIMATE_SWING_VERTICAL;
  }
  if (horizontal_swing) {
    return esphome::climate::CLIMATE_SWING_HORIZONTAL;
  }
  return esphome::climate::CLIMATE_SWING_OFF;
}

inline void emit_gree_via_esphome(IRSender &ir, esphome::gree::Model model, uint8_t power_mode_cmd,
                                  uint8_t operating_mode_cmd, uint8_t fan_speed_cmd, uint8_t temperature_cmd,
                                  uint8_t swing_v_cmd, uint8_t swing_h_cmd, bool light_enabled) {
  MockRemoteTransmitter source_tx;
  TestableGreeClimate source;
  source.set_transmitter(&source_tx);
  source.set_model(model);

  source.mode = climate_mode_for_command(power_mode_cmd, operating_mode_cmd);
  source.target_temperature =
      std::clamp<float>(temperature_cmd, esphome::gree::GREE_TEMP_MIN, esphome::gree::GREE_TEMP_MAX);
  if (power_mode_cmd != POWER_OFF && operating_mode_cmd == MODE_AUTO) {
    source.target_temperature = 25.0f;
  }
  source.fan_mode = climate_fan_mode_for_command(fan_speed_cmd, operating_mode_cmd);

  source.set_vertical_default(vertical_direction_for_command(swing_v_cmd));
  source.set_horizontal_default(horizontal_direction_for_command(swing_h_cmd));
  source.swing_mode = swing_mode_for_commands(model, swing_v_cmd, swing_h_cmd);

  if (model == esphome::gree::GREE_YAN) {
    source.set_mode_bits_for_test(esphome::gree::GREE_MODE_BIT_LIGHT | esphome::gree::GREE_MODE_BIT_HEALTH);
  } else if (model == esphome::gree::GREE_YAP1F) {
    source.set_mode_bits_for_test(light_enabled ? esphome::gree::GREE_MODE_BIT_LIGHT : 0);
  }

  source.transmit_state();

  ir.setFrequency(static_cast<int>(source_tx.last_carrier_frequency() / 1000));
  for (const int32_t timing : source_tx.last_data()) {
    if (timing >= 0) {
      ir.mark(static_cast<int>(timing));
    } else {
      ir.space(static_cast<int>(-timing));
    }
  }
}

}  // namespace heatpumpir_test_stub

class GreeGenericHeatpumpIR : public HeatpumpIR {
 public:
  void send(IRSender &ir, uint8_t power_mode_cmd, uint8_t operating_mode_cmd, uint8_t fan_speed_cmd,
            uint8_t temperature_cmd, uint8_t swing_v_cmd, uint8_t swing_h_cmd) override {
    heatpumpir_test_stub::emit_gree_via_esphome(ir, esphome::gree::GREE_GENERIC, power_mode_cmd, operating_mode_cmd,
                                                fan_speed_cmd, temperature_cmd, swing_v_cmd, swing_h_cmd, true);
  }
};

class GreeYAAHeatpumpIR : public HeatpumpIR {
 public:
  void send(IRSender &ir, uint8_t power_mode_cmd, uint8_t operating_mode_cmd, uint8_t fan_speed_cmd,
            uint8_t temperature_cmd, uint8_t swing_v_cmd, uint8_t swing_h_cmd) override {
    heatpumpir_test_stub::emit_gree_via_esphome(ir, esphome::gree::GREE_YAA, power_mode_cmd, operating_mode_cmd,
                                                fan_speed_cmd, temperature_cmd, swing_v_cmd, swing_h_cmd, true);
  }
};

class GreeYANHeatpumpIR : public HeatpumpIR {
 public:
  void send(IRSender &ir, uint8_t power_mode_cmd, uint8_t operating_mode_cmd, uint8_t fan_speed_cmd,
            uint8_t temperature_cmd, uint8_t swing_v_cmd, uint8_t swing_h_cmd) override {
    heatpumpir_test_stub::emit_gree_via_esphome(ir, esphome::gree::GREE_YAN, power_mode_cmd, operating_mode_cmd,
                                                fan_speed_cmd, temperature_cmd, swing_v_cmd, swing_h_cmd, true);
  }
};

class GreeYACHeatpumpIR : public HeatpumpIR {
 public:
  void send(IRSender &ir, uint8_t power_mode_cmd, uint8_t operating_mode_cmd, uint8_t fan_speed_cmd,
            uint8_t temperature_cmd, uint8_t swing_v_cmd, uint8_t swing_h_cmd) override {
    heatpumpir_test_stub::emit_gree_via_esphome(ir, esphome::gree::GREE_YAC, power_mode_cmd, operating_mode_cmd,
                                                fan_speed_cmd, temperature_cmd, swing_v_cmd, swing_h_cmd, true);
  }
};

class GreeYTHeatpumpIR : public HeatpumpIR {
 public:
  void send(IRSender &ir, uint8_t power_mode_cmd, uint8_t operating_mode_cmd, uint8_t fan_speed_cmd,
            uint8_t temperature_cmd, uint8_t swing_v_cmd, uint8_t swing_h_cmd) override {
    heatpumpir_test_stub::emit_gree_via_esphome(ir, esphome::gree::GREE_YX1FF, power_mode_cmd, operating_mode_cmd,
                                                fan_speed_cmd, temperature_cmd, swing_v_cmd, swing_h_cmd, true);
  }
};

class GreeYAPHeatpumpIR : public HeatpumpIR {
 public:
  void send(IRSender &ir, uint8_t power_mode_cmd, uint8_t operating_mode_cmd, uint8_t fan_speed_cmd,
            uint8_t temperature_cmd, uint8_t swing_v_cmd, uint8_t swing_h_cmd) override {
    this->send(ir, power_mode_cmd, operating_mode_cmd, fan_speed_cmd, temperature_cmd, swing_v_cmd, swing_h_cmd, false,
               false, true);
  }

  void send(IRSender &ir, uint8_t power_mode_cmd, uint8_t operating_mode_cmd, uint8_t fan_speed_cmd,
            uint8_t temperature_cmd, uint8_t swing_v_cmd, uint8_t swing_h_cmd, bool, bool, bool light) {
    heatpumpir_test_stub::emit_gree_via_esphome(ir, esphome::gree::GREE_YAP1F, power_mode_cmd, operating_mode_cmd,
                                                fan_speed_cmd, temperature_cmd, swing_v_cmd, swing_h_cmd, light);
  }
};

#else

class GreeGenericHeatpumpIR : public HeatpumpIR {};
class GreeYAAHeatpumpIR : public HeatpumpIR {};
class GreeYANHeatpumpIR : public HeatpumpIR {};
class GreeYACHeatpumpIR : public HeatpumpIR {};
class GreeYTHeatpumpIR : public HeatpumpIR {};

class GreeYAPHeatpumpIR : public HeatpumpIR {
 public:
  using HeatpumpIR::send;
  void send(IRSender &, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, bool, bool, bool) {}
};

#endif
