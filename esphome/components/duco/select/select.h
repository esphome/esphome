#pragma once

#include "esphome/core/log.h"
#include "esphome/core/component.h"
#include "esphome/components/select/select.h"
#include "../duco.h"

namespace esphome {
namespace duco {

class DucoSelect : public DucoDevice, public PollingComponent, public select::Select {
 public:
  static const std::string MODE_AUTO;
  static const std::string MODE_MAN1;
  static const std::string MODE_MAN2;
  static const std::string MODE_MAN3;
  static const std::string MODE_EMPT;
  static const std::string MODE_CNT1;
  static const std::string MODE_CNT2;
  static const std::string MODE_CNT3;
  static const std::string MODE_MAN1x2;
  static const std::string MODE_MAN2x2;
  static const std::string MODE_MAN3x2;
  static const std::string MODE_MAN1x3;
  static const std::string MODE_MAN2x3;
  static const std::string MODE_MAN3x3;

  static const uint8_t MODE_CODE_AUTO;
  static const uint8_t MODE_CODE_MAN1;
  static const uint8_t MODE_CODE_MAN2;
  static const uint8_t MODE_CODE_MAN3;
  static const uint8_t MODE_CODE_EMPT;
  static const uint8_t MODE_CODE_CNT1;
  static const uint8_t MODE_CODE_CNT2;
  static const uint8_t MODE_CODE_CNT3;
  static const uint8_t MODE_CODE_MAN1x2;
  static const uint8_t MODE_CODE_MAN2x2;
  static const uint8_t MODE_CODE_MAN3x2;
  static const uint8_t MODE_CODE_MAN1x3;
  static const uint8_t MODE_CODE_MAN2x3;
  static const uint8_t MODE_CODE_MAN3x3;

  void setup() override;
  void update() override;

  float get_setup_priority() const override;

  void receive_response(std::vector<uint8_t> message) override;

  void control(const std::string &value) override;

 protected:
  uint8_t attempts = 0;
};

}  // namespace duco
}  // namespace esphome
