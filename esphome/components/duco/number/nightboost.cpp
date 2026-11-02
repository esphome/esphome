#include "nightboost.h"

#include <cmath>

#include "esphome/core/log.h"

namespace esphome::duco {

ESPHOME_LOG_TAG(TAG, "duco.nightboost_number");

static constexpr uint8_t NIGHTBOOST_GROUP = 0x03;
static constexpr uint8_t NIGHTBOOST_TEMPERATURE_PARAMETER = 0x02;
static constexpr uint8_t NIGHTBOOST_MAX_PARAMETER = 0x07;

void DucoNightboostTemperature::update() {
  this->parent_->send(this, FUNCTION_PARAMETER, {0x00, NIGHTBOOST_TEMPERATURE_PARAMETER, NIGHTBOOST_GROUP});
}

void DucoNightboostTemperature::receive_response(const DucoMessage &message) {
  if (message.size >= 4 && message.data[3] <= 60)
    this->publish_state(message.data[3]);
}

void DucoNightboostTemperature::control(float value) {
  if (!this->parent_->allows_dangerous_controls()) {
    ESP_LOGW(TAG, "NightBoost temperature change rejected: allow_dangerous_controls is disabled");
    this->publish_state(this->state);
    return;
  }

  const auto temperature = static_cast<uint8_t>(lroundf(value));
  this->parent_->send(nullptr, FUNCTION_PARAMETER,
                      {0x01, NIGHTBOOST_TEMPERATURE_PARAMETER, NIGHTBOOST_GROUP, temperature, 0x00, 0x00, 0x00});
  // Update state immediately
  this->publish_state(value);
}

void DucoNightboostMax::update() {
  this->parent_->send(this, FUNCTION_PARAMETER, {0x00, NIGHTBOOST_MAX_PARAMETER, NIGHTBOOST_GROUP});
}

void DucoNightboostMax::receive_response(const DucoMessage &message) {
  if (message.size >= 4 && message.data[3] >= 10 && message.data[3] <= 100)
    this->publish_state(message.data[3]);
}

void DucoNightboostMax::control(float value) {
  if (!this->parent_->allows_dangerous_controls()) {
    ESP_LOGW(TAG, "NightBoost maximum change rejected: allow_dangerous_controls is disabled");
    this->publish_state(this->state);
    return;
  }

  const auto percentage = static_cast<uint8_t>(lroundf(value));
  this->parent_->send(nullptr, FUNCTION_PARAMETER,
                      {0x00, NIGHTBOOST_MAX_PARAMETER, NIGHTBOOST_GROUP, percentage, 0x00, 0x00, 0x00});
  // Update state immediately
  this->publish_state(value);
}

}  // namespace esphome::duco
