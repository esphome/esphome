#pragma once

#include "esphome/components/number/number.h"
#include "esphome/core/component.h"
#include "cc1101.h"

namespace esphome::cc1101 {

class CC1101Number : public number::Number, public PollingComponent, public Parented<CC1101Component> {
 public:
  enum CC1101NumberType {
    OUTPUT_POWER,
    FREQUENCY,
    IF_FREQUENCY,
    FILTER_BANDWIDTH,
    CHANNEL,
    CHANNEL_SPACING,
    FSK_DEVIATION,
    MSK_DEVIATION,
    SYMBOL_RATE,
    NUM_PREAMBLE,
    SYNC1,
    SYNC0,
    CARRIER_SENSE_ABS_THR,
    PACKET_LENGTH,
  };

  void set_type(CC1101NumberType type) { type_ = type; }

  void update() override {
    if (this->parent_ == nullptr) return;
    float value = 0;
    switch (this->type_) {
      case OUTPUT_POWER:
        value = this->parent_->get_output_power();
        break;
      case FREQUENCY:
        value = this->parent_->get_frequency();
        break;
      case IF_FREQUENCY:
        value = this->parent_->get_if_frequency();
        break;
      case FILTER_BANDWIDTH:
        value = this->parent_->get_filter_bandwidth();
        break;
      case CHANNEL:
        value = this->parent_->get_channel();
        break;
      case CHANNEL_SPACING:
        value = this->parent_->get_channel_spacing();
        break;
      case FSK_DEVIATION:
        value = this->parent_->get_fsk_deviation();
        break;
      case MSK_DEVIATION:
        value = this->parent_->get_msk_deviation();
        break;
      case SYMBOL_RATE:
        value = this->parent_->get_symbol_rate();
        break;
      case NUM_PREAMBLE:
        value = this->parent_->get_num_preamble();
        break;
      case SYNC1:
        value = this->parent_->get_sync1();
        break;
      case SYNC0:
        value = this->parent_->get_sync0();
        break;
      case CARRIER_SENSE_ABS_THR:
        value = this->parent_->get_carrier_sense_abs_thr();
        break;
      case PACKET_LENGTH:
        value = this->parent_->get_packet_length();
        break;
    }
    this->publish_state(value);
  }

 protected:
  void control(float value) override {
    if (this->parent_ == nullptr) return;
    switch (this->type_) {
      case OUTPUT_POWER:
        this->parent_->set_output_power(value);
        break;
      case FREQUENCY:
        this->parent_->set_frequency(value);
        break;
      case IF_FREQUENCY:
        this->parent_->set_if_frequency(value);
        break;
      case FILTER_BANDWIDTH:
        this->parent_->set_filter_bandwidth(value);
        break;
      case CHANNEL:
        this->parent_->set_channel(static_cast<uint8_t>(value));
        break;
      case CHANNEL_SPACING:
        this->parent_->set_channel_spacing(value);
        break;
      case FSK_DEVIATION:
        this->parent_->set_fsk_deviation(value);
        break;
      case MSK_DEVIATION:
        this->parent_->set_msk_deviation(static_cast<uint8_t>(value));
        break;
      case SYMBOL_RATE:
        this->parent_->set_symbol_rate(value);
        break;
      case NUM_PREAMBLE:
        this->parent_->set_num_preamble(static_cast<uint8_t>(value));
        break;
      case SYNC1:
        this->parent_->set_sync1(static_cast<uint8_t>(value));
        break;
      case SYNC0:
        this->parent_->set_sync0(static_cast<uint8_t>(value));
        break;
      case CARRIER_SENSE_ABS_THR:
        this->parent_->set_carrier_sense_abs_thr(static_cast<int8_t>(value));
        break;
      case PACKET_LENGTH:
        this->parent_->set_packet_length(static_cast<uint8_t>(value));
        break;
    }
    this->publish_state(value);
  }

  CC1101NumberType type_;
};

}  // namespace esphome::cc1101
