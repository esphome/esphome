#pragma once

#include "esphome/core/defines.h"

#ifdef USE_SWITCH

#include "esphome/components/switch/switch.h"
#include "esphome/core/component.h"
#include "cc1101.h"

namespace esphome::cc1101 {

class CC1101Switch : public switch_::Switch,
                     public PollingComponent,
                     public Parented<CC1101Component>,
                     public CC1101ConfigListener {
 public:
  enum CC1101SwitchType {
    DC_BLOCKING_FILTER,
    CARRIER_SENSE_ABOVE_THRESHOLD,
    MANCHESTER,
    LNA_PRIORITY,
    PACKET_MODE,
    CRC_ENABLE,
    WHITENING,
  };

  void set_type(CC1101SwitchType type) { type_ = type; }

  void setup() override {
    if (this->parent_ != nullptr) {
      this->parent_->register_config_listener(this);
    }
  }

  void on_config_change() override { this->update(); }

  void update() override {
    if (this->parent_ == nullptr)
      return;
    bool value = false;
    switch (this->type_) {
      case DC_BLOCKING_FILTER:
        value = this->parent_->get_dc_blocking_filter();
        break;
      case CARRIER_SENSE_ABOVE_THRESHOLD:
        value = this->parent_->get_carrier_sense_above_threshold();
        break;
      case MANCHESTER:
        value = this->parent_->get_manchester();
        break;
      case LNA_PRIORITY:
        value = this->parent_->get_lna_priority();
        break;
      case PACKET_MODE:
        value = this->parent_->get_packet_mode();
        break;
      case CRC_ENABLE:
        value = this->parent_->get_crc_enable();
        break;
      case WHITENING:
        value = this->parent_->get_whitening();
        break;
    }
    if (!this->has_state() || value != this->state) {
      this->publish_state(value);
    }
  }

 protected:
  void write_state(bool state) override {
    if (this->parent_ == nullptr)
      return;
    switch (this->type_) {
      case DC_BLOCKING_FILTER:
        this->parent_->set_dc_blocking_filter(state);
        break;
      case CARRIER_SENSE_ABOVE_THRESHOLD:
        this->parent_->set_carrier_sense_above_threshold(state);
        break;
      case MANCHESTER:
        this->parent_->set_manchester(state);
        break;
      case LNA_PRIORITY:
        this->parent_->set_lna_priority(state);
        break;
      case PACKET_MODE:
        this->parent_->set_packet_mode(state);
        break;
      case CRC_ENABLE:
        this->parent_->set_crc_enable(state);
        break;
      case WHITENING:
        this->parent_->set_whitening(state);
        break;
    }
    this->publish_state(state);
  }

  CC1101SwitchType type_;
};

}  // namespace esphome::cc1101

#endif
