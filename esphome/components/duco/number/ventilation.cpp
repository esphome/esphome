#include "ventilation.h"

#include <cmath>

#include "esphome/core/log.h"

namespace esphome::duco {

ESPHOME_LOG_TAG(TAG, "duco.ventilation");

static constexpr uint8_t FUNCTION_VENTILATION = 0x18;
static constexpr uint8_t VENTILATION_ADDRESS = 0x01;
static constexpr uint8_t VENTILATION_BOX = 0x01;
static constexpr uint8_t VENTILATION_MIN = 0x00;
static constexpr uint8_t VENTILATION_MAX = 0x01;

static void publish_ventilation(number::Number *number, const DucoMessage &message) {
  if (message.size >= 8 && message.data[3] == 0x05 && message.data[7] == 0x64 && message.data[1] >= 10 &&
      message.data[1] <= 100)
    number->publish_state(message.data[1]);
}

static void control_ventilation(Duco *parent, number::Number *number, uint8_t type, float value) {
  if (!parent->allows_dangerous_controls()) {
    ESP_LOGW(TAG, "Ventilation limit change rejected: allow_dangerous_controls is disabled");
    number->publish_state(number->state);
    return;
  }

  const auto percentage = static_cast<uint8_t>(lroundf(value));
  parent->send(nullptr, FUNCTION_VENTILATION, {VENTILATION_ADDRESS, VENTILATION_BOX, type, 0x00, percentage});
  // Update state immediately
  number->publish_state(value);
}

void DucoBoxVentilationMin::update() {
  this->parent_->send(this, FUNCTION_VENTILATION, {0x00, VENTILATION_BOX, VENTILATION_MIN});
}

void DucoBoxVentilationMin::receive_response(const DucoMessage &message) { publish_ventilation(this, message); }

void DucoBoxVentilationMin::control(float value) {
  control_ventilation(this->parent_, this, VENTILATION_MIN, value);
}

void DucoBoxVentilationMax::update() {
  this->parent_->send(this, FUNCTION_VENTILATION, {0x00, VENTILATION_BOX, VENTILATION_MAX});
}

void DucoBoxVentilationMax::receive_response(const DucoMessage &message) { publish_ventilation(this, message); }

void DucoBoxVentilationMax::control(float value) {
  control_ventilation(this->parent_, this, VENTILATION_MAX, value);
}

}  // namespace esphome::duco
