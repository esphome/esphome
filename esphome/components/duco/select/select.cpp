#include "select.h"
#include "../duco.h"
#include <vector>

namespace esphome {
namespace duco {

static const char *const TAG = "duco select";

const std::string DucoSelect::MODE_AUTO = "AUTO";
const std::string DucoSelect::MODE_MAN1 = "MAN1";
const std::string DucoSelect::MODE_MAN2 = "MAN2";
const std::string DucoSelect::MODE_MAN3 = "MAN3";
const std::string DucoSelect::MODE_EMPT = "EMPT";
const std::string DucoSelect::MODE_CNT1 = "CNT1";
const std::string DucoSelect::MODE_CNT2 = "CNT2";
const std::string DucoSelect::MODE_CNT3 = "CNT3";
const std::string DucoSelect::MODE_MAN1X2 = "MAN1x2";
const std::string DucoSelect::MODE_MAN2X2 = "MAN2x2";
const std::string DucoSelect::MODE_MAN3X2 = "MAN3x2";
const std::string DucoSelect::MODE_MAN1X3 = "MAN1x3";
const std::string DucoSelect::MODE_MAN2X3 = "MAN2x3";
const std::string DucoSelect::MODE_MAN3X3 = "MAN3x3";

const uint8_t DucoSelect::MODE_CODE_AUTO = 0x00;
const uint8_t DucoSelect::MODE_CODE_MAN1 = 0x04;
const uint8_t DucoSelect::MODE_CODE_MAN2 = 0x05;
const uint8_t DucoSelect::MODE_CODE_MAN3 = 0x06;
const uint8_t DucoSelect::MODE_CODE_EMPT = 0x07;
const uint8_t DucoSelect::MODE_CODE_CNT1 = 0x08;
const uint8_t DucoSelect::MODE_CODE_CNT2 = 0x09;
const uint8_t DucoSelect::MODE_CODE_CNT3 = 0x0a;
const uint8_t DucoSelect::MODE_CODE_MAN1X2 = 0x84;
const uint8_t DucoSelect::MODE_CODE_MAN2X2 = 0x85;
const uint8_t DucoSelect::MODE_CODE_MAN3X2 = 0x86;
const uint8_t DucoSelect::MODE_CODE_MAN1X3 = 0xc4;
const uint8_t DucoSelect::MODE_CODE_MAN2X3 = 0xc5;
const uint8_t DucoSelect::MODE_CODE_MAN3X3 = 0xc6;

void DucoSelect::setup() {}

void DucoSelect::update() {
  DucoMessage message;
  message.function = 0x0c;
  message.data = {0x02, 0x01};
  this->parent_->send(message, this);
}

float DucoSelect::get_setup_priority() const {
  // After DUCO
  return setup_priority::BUS - 2.0f;
}

std::string code_to_string(uint8_t mode) {
  switch (mode) {
    case DucoSelect::MODE_CODE_MAN1:
      return DucoSelect::MODE_MAN1;
    case DucoSelect::MODE_CODE_MAN2:
      return DucoSelect::MODE_MAN2;
    case DucoSelect::MODE_CODE_MAN3:
      return DucoSelect::MODE_MAN3;
    case DucoSelect::MODE_CODE_EMPT:
      return DucoSelect::MODE_EMPT;
    case DucoSelect::MODE_CODE_CNT1:
      return DucoSelect::MODE_CNT1;
    case DucoSelect::MODE_CODE_CNT2:
      return DucoSelect::MODE_CNT2;
    case DucoSelect::MODE_CODE_CNT3:
      return DucoSelect::MODE_CNT3;
    case DucoSelect::MODE_CODE_MAN1X2:
      return DucoSelect::MODE_MAN1X2;
    case DucoSelect::MODE_CODE_MAN2X2:
      return DucoSelect::MODE_MAN2X2;
    case DucoSelect::MODE_CODE_MAN3X2:
      return DucoSelect::MODE_MAN3X2;
    case DucoSelect::MODE_CODE_MAN1X3:
      return DucoSelect::MODE_MAN1X3;
    case DucoSelect::MODE_CODE_MAN2X3:
      return DucoSelect::MODE_MAN2X3;
    case DucoSelect::MODE_CODE_MAN3X3:
      return DucoSelect::MODE_MAN3X3;
    case DucoSelect::MODE_CODE_AUTO:
    default:
      return DucoSelect::MODE_AUTO;
  }
  return DucoSelect::MODE_AUTO;
}

uint8_t string_to_code(const std::string &mode) {
  if (mode == DucoSelect::MODE_MAN1) {
    return DucoSelect::MODE_CODE_MAN1;
  }
  if (mode == DucoSelect::MODE_MAN2) {
    return DucoSelect::MODE_CODE_MAN2;
  }
  if (mode == DucoSelect::MODE_MAN3) {
    return DucoSelect::MODE_CODE_MAN3;
  }
  if (mode == DucoSelect::MODE_EMPT) {
    return DucoSelect::MODE_CODE_EMPT;
  }
  if (mode == DucoSelect::MODE_CNT1) {
    return DucoSelect::MODE_CODE_CNT1;
  }
  if (mode == DucoSelect::MODE_CNT2) {
    return DucoSelect::MODE_CODE_CNT2;
  }
  if (mode == DucoSelect::MODE_CNT3) {
    return DucoSelect::MODE_CODE_CNT3;
  }
  if (mode == DucoSelect::MODE_MAN1X2) {
    return DucoSelect::MODE_CODE_MAN1X2;
  }
  if (mode == DucoSelect::MODE_MAN2X2) {
    return DucoSelect::MODE_CODE_MAN2X2;
  }
  if (mode == DucoSelect::MODE_MAN3X2) {
    return DucoSelect::MODE_CODE_MAN3X2;
  }
  if (mode == DucoSelect::MODE_MAN1X3) {
    return DucoSelect::MODE_CODE_MAN1X3;
  }
  if (mode == DucoSelect::MODE_MAN2X3) {
    return DucoSelect::MODE_CODE_MAN2X3;
  }
  if (mode == DucoSelect::MODE_MAN3X3) {
    return DucoSelect::MODE_CODE_MAN3X3;
  }
  if (mode == DucoSelect::MODE_AUTO) {
    return DucoSelect::MODE_CODE_AUTO;
  }
  return DucoSelect::MODE_CODE_AUTO;
}

void DucoSelect::receive_response(DucoMessage message) {
  if (message.function == 0x0e && message.data[0] != 0x01) {
    // mode response received, parse it
    auto mode = code_to_string(message.data[0]);

    publish_state(mode);

    ESP_LOGD(TAG, "Current mode: %s", mode.c_str());

    // do not wait for new messages with the same ID
    this->parent_->stop_waiting(message.id);
  }
  if (message.function == 0x0e && message.data[0] == 0x01) {
    this->parent_->stop_waiting(message.id);
  }
}

void DucoSelect::control(const std::string &value) {
  DucoMessage message;
  message.function = 0x0c;
  message.data = {0x04, 0x01, string_to_code(value)};
  this->parent_->send(message, this);
}

}  // namespace duco
}  // namespace esphome
