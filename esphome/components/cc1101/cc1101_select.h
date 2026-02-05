#pragma once

#include "esphome/core/defines.h"

#ifdef USE_SELECT

#include "esphome/components/select/select.h"
#include "esphome/core/component.h"
#include "cc1101.h"
#include <cmath>
#include <string>

namespace esphome::cc1101 {

class CC1101Select : public select::Select,
                     public PollingComponent,
                     public Parented<CC1101Component>,
                     public CC1101ConfigListener {
 public:
  enum CC1101SelectType {
    RX_ATTENUATION,
    SYNC_MODE,
    MODULATION_TYPE,
    MAGN_TARGET,
    MAX_LNA_GAIN,
    MAX_DVGA_GAIN,
    CARRIER_SENSE_REL_THR,
    FILTER_LENGTH_FSK_MSK,
    FILTER_LENGTH_ASK_OOK,
    FREEZE,
    WAIT_TIME,
    HYST_LEVEL,
    FREQUENCY_PRESET,
  };

  void set_type(CC1101SelectType type) { type_ = type; }

  void setup() override {
    if (this->parent_ != nullptr) {
      this->parent_->register_config_listener(this);
    }
  }

  void on_config_change() override { this->update(); }

  struct Option {
    const char *name;
    int value;
  };

  static constexpr Option RX_ATTENUATION_OPTIONS[4] = {{"0dB", 0}, {"6dB", 1}, {"12dB", 2}, {"18dB", 3}};

  static constexpr Option SYNC_MODE_OPTIONS[4] = {{"None", 0}, {"15/16", 1}, {"16/16", 2}, {"30/32", 3}};

  static constexpr Option MODULATION_OPTIONS[5] = {{"2-FSK", 0}, {"GFSK", 1}, {"ASK/OOK", 3}, {"4-FSK", 4}, {"MSK", 7}};

  static constexpr Option MAGN_TARGET_OPTIONS[8] = {{"24dB", 0}, {"27dB", 1}, {"30dB", 2}, {"33dB", 3},
                                                    {"36dB", 4}, {"38dB", 5}, {"40dB", 6}, {"42dB", 7}};

  static constexpr Option MAX_LNA_GAIN_OPTIONS[8] = {{"Default", 0}, {"2.6dB", 1},  {"6.1dB", 2},  {"7.4dB", 3},
                                                     {"9.2dB", 4},   {"11.5dB", 5}, {"14.6dB", 6}, {"17.1dB", 7}};

  static constexpr Option MAX_DVGA_GAIN_OPTIONS[4] = {{"Default", 0}, {"-1", 1}, {"-2", 2}, {"-3", 3}};

  static constexpr Option CARRIER_SENSE_REL_THR_OPTIONS[4] = {{"Default", 0}, {"+6dB", 1}, {"+10dB", 2}, {"+14dB", 3}};

  static constexpr Option FILTER_LENGTH_FSK_MSK_OPTIONS[4] = {{"8", 0}, {"16", 1}, {"32", 2}, {"64", 3}};

  static constexpr Option FILTER_LENGTH_ASK_OOK_OPTIONS[4] = {{"4dB", 0}, {"8dB", 1}, {"12dB", 2}, {"16dB", 3}};

  static constexpr Option FREEZE_OPTIONS[4] = {
      {"Default", 0}, {"On Sync", 1}, {"Analog Only", 2}, {"Analog And Digital", 3}};

  static constexpr Option WAIT_TIME_OPTIONS[4] = {{"8", 0}, {"16", 1}, {"24", 2}, {"32", 3}};

  static constexpr Option HYST_LEVEL_OPTIONS[4] = {{"None", 0}, {"Low", 1}, {"Medium", 2}, {"High", 3}};

  static constexpr Option FREQUENCY_PRESET_OPTIONS[5] = {
      {"315MHz", 315000000}, {"433.92MHz", 433920000}, {"868MHz", 868000000}, {"915MHz", 915000000}, {"Manual", 0}};

  void update() override {
    if (this->parent_ == nullptr)
      return;
    std::string value;
    switch (this->type_) {
      case RX_ATTENUATION:
        value = get_option_name(static_cast<int>(this->parent_->get_rx_attenuation()), RX_ATTENUATION_OPTIONS, 4);
        break;
      case SYNC_MODE:
        value = get_option_name(static_cast<int>(this->parent_->get_sync_mode()), SYNC_MODE_OPTIONS, 4);
        break;
      case MODULATION_TYPE:
        value = get_option_name(static_cast<int>(this->parent_->get_modulation_type()), MODULATION_OPTIONS, 5);
        break;
      case MAGN_TARGET:
        value = get_option_name(static_cast<int>(this->parent_->get_magn_target()), MAGN_TARGET_OPTIONS, 8);
        break;
      case MAX_LNA_GAIN:
        value = get_option_name(static_cast<int>(this->parent_->get_max_lna_gain()), MAX_LNA_GAIN_OPTIONS, 8);
        break;
      case MAX_DVGA_GAIN:
        value = get_option_name(static_cast<int>(this->parent_->get_max_dvga_gain()), MAX_DVGA_GAIN_OPTIONS, 4);
        break;
      case CARRIER_SENSE_REL_THR:
        value = get_option_name(static_cast<int>(this->parent_->get_carrier_sense_rel_thr()),
                                CARRIER_SENSE_REL_THR_OPTIONS, 4);
        break;
      case FILTER_LENGTH_FSK_MSK:
        value = get_option_name(static_cast<int>(this->parent_->get_filter_length_fsk_msk()),
                                FILTER_LENGTH_FSK_MSK_OPTIONS, 4);
        break;
      case FILTER_LENGTH_ASK_OOK:
        value = get_option_name(static_cast<int>(this->parent_->get_filter_length_ask_ook()),
                                FILTER_LENGTH_ASK_OOK_OPTIONS, 4);
        break;
      case FREEZE:
        value = get_option_name(static_cast<int>(this->parent_->get_freeze()), FREEZE_OPTIONS, 4);
        break;
      case WAIT_TIME:
        value = get_option_name(static_cast<int>(this->parent_->get_wait_time()), WAIT_TIME_OPTIONS, 4);
        break;
      case HYST_LEVEL:
        value = get_option_name(static_cast<int>(this->parent_->get_hyst_level()), HYST_LEVEL_OPTIONS, 4);
        break;
      case FREQUENCY_PRESET: {
        float freq = this->parent_->get_frequency();
        bool found = false;
        for (const auto &opt : FREQUENCY_PRESET_OPTIONS) {
          if (opt.value != 0 && std::abs(freq - opt.value) < 1000) {  // 1kHz tolerance
            value = opt.name;
            found = true;
            break;
          }
        }
        if (!found)
          value = "Manual";
        break;
      }
    }
    if (!value.empty()) {
      this->publish_state(value);
    }
  }

 protected:
  void control(const std::string &value) override {
    if (this->parent_ == nullptr)
      return;
    int val = 0;
    switch (this->type_) {
      case RX_ATTENUATION:
        val = get_option_value(value, RX_ATTENUATION_OPTIONS, 4);
        if (val != -1)
          this->parent_->set_rx_attenuation(static_cast<RxAttenuation>(val));
        break;
      case SYNC_MODE:
        val = get_option_value(value, SYNC_MODE_OPTIONS, 4);
        if (val != -1)
          this->parent_->set_sync_mode(static_cast<SyncMode>(val));
        break;
      case MODULATION_TYPE:
        val = get_option_value(value, MODULATION_OPTIONS, 5);
        if (val != -1)
          this->parent_->set_modulation_type(static_cast<Modulation>(val));
        break;
      case MAGN_TARGET:
        val = get_option_value(value, MAGN_TARGET_OPTIONS, 8);
        if (val != -1)
          this->parent_->set_magn_target(static_cast<MagnTarget>(val));
        break;
      case MAX_LNA_GAIN:
        val = get_option_value(value, MAX_LNA_GAIN_OPTIONS, 8);
        if (val != -1)
          this->parent_->set_max_lna_gain(static_cast<MaxLnaGain>(val));
        break;
      case MAX_DVGA_GAIN:
        val = get_option_value(value, MAX_DVGA_GAIN_OPTIONS, 4);
        if (val != -1)
          this->parent_->set_max_dvga_gain(static_cast<MaxDvgaGain>(val));
        break;
      case CARRIER_SENSE_REL_THR:
        val = get_option_value(value, CARRIER_SENSE_REL_THR_OPTIONS, 4);
        if (val != -1)
          this->parent_->set_carrier_sense_rel_thr(static_cast<CarrierSenseRelThr>(val));
        break;
      case FILTER_LENGTH_FSK_MSK:
        val = get_option_value(value, FILTER_LENGTH_FSK_MSK_OPTIONS, 4);
        if (val != -1)
          this->parent_->set_filter_length_fsk_msk(static_cast<FilterLengthFskMsk>(val));
        break;
      case FILTER_LENGTH_ASK_OOK:
        val = get_option_value(value, FILTER_LENGTH_ASK_OOK_OPTIONS, 4);
        if (val != -1)
          this->parent_->set_filter_length_ask_ook(static_cast<FilterLengthAskOok>(val));
        break;
      case FREEZE:
        val = get_option_value(value, FREEZE_OPTIONS, 4);
        if (val != -1)
          this->parent_->set_freeze(static_cast<Freeze>(val));
        break;
      case WAIT_TIME:
        val = get_option_value(value, WAIT_TIME_OPTIONS, 4);
        if (val != -1)
          this->parent_->set_wait_time(static_cast<WaitTime>(val));
        break;
      case HYST_LEVEL:
        val = get_option_value(value, HYST_LEVEL_OPTIONS, 4);
        if (val != -1)
          this->parent_->set_hyst_level(static_cast<HystLevel>(val));
        break;
      case FREQUENCY_PRESET:
        val = get_option_value(value, FREQUENCY_PRESET_OPTIONS, 5);
        if (val != -1 && val != 0) {  // 0 is Manual
          this->parent_->set_frequency(static_cast<float>(val));
        }
        break;
    }
    this->publish_state(value);
  }

  CC1101SelectType type_;

  std::string get_option_name(int val, const Option *opts, size_t size) {
    for (size_t i = 0; i < size; i++) {
      if (opts[i].value == val)
        return opts[i].name;
    }
    return "";
  }

  int get_option_value(const std::string &name, const Option *opts, size_t size) {
    for (size_t i = 0; i < size; i++) {
      if (name == opts[i].name)
        return opts[i].value;
    }
    return -1;
  }
};

}  // namespace esphome::cc1101

#endif
