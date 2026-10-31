#include "nightboost.h"

#include "esphome/core/log.h"

namespace esphome::duco {

ESPHOME_LOG_TAG(TAG, "duco.nightboost");

static constexpr uint8_t NIGHTBOOST_ADDRESS = 0x01;
static constexpr uint8_t NIGHTBOOST_PARAMETER = 0x01;
static constexpr uint8_t NIGHTBOOST_GROUP = 0x03;
static constexpr uint8_t NIGHTBOOST_CODES[] = {0x01, 0x00};
static constexpr uint8_t NIGHTBOOST_MONTH_START_PARAMETER = 0x03;
static constexpr uint8_t NIGHTBOOST_MONTH_STOP_PARAMETER = 0x04;

void DucoNightboost::update() {
  this->parent_->send(this, FUNCTION_PARAMETER, {0x00, NIGHTBOOST_PARAMETER, NIGHTBOOST_GROUP});
}

void DucoNightboost::receive_response(const DucoMessage &message) {
  if (message.size < 4)
    return;

  for (size_t index = 0; index < sizeof(NIGHTBOOST_CODES); index++) {
    if (message.data[3] == NIGHTBOOST_CODES[index]) {
      this->publish_state(index);
      return;
    }
  }
  ESP_LOGV(TAG, "Unknown NightBoost mode %02X", message.data[3]);
}

void DucoNightboost::control(size_t index) {
  if (!this->parent_->allows_dangerous_controls()) {
    ESP_LOGW(TAG, "NightBoost change rejected: allow_dangerous_controls is disabled");
    if (this->has_state())
      this->publish_state(this->active_index_);
    return;
  }

  this->parent_->send(nullptr, FUNCTION_PARAMETER,
                      {NIGHTBOOST_ADDRESS, NIGHTBOOST_PARAMETER, NIGHTBOOST_GROUP, NIGHTBOOST_CODES[index], 0x00,
                       0x00, 0x00});
}

static void publish_month(select::Select *select, const DucoMessage &message) {
  if (message.size >= 4 && message.data[3] >= 1 && message.data[3] <= 12)
    select->publish_state(message.data[3] - 1);
}

static void control_month(Duco *parent, select::Select *select, size_t index, uint8_t parameter) {
  if (!parent->allows_dangerous_controls()) {
    ESP_LOGW(TAG, "NightBoost month change rejected: allow_dangerous_controls is disabled");
    if (select->has_state())
      select->publish_state(select->active_index().value());
    return;
  }

  parent->send(nullptr, FUNCTION_PARAMETER,
               {NIGHTBOOST_ADDRESS, parameter, NIGHTBOOST_GROUP, static_cast<uint8_t>(index + 1), 0x00, 0x00, 0x00});
}

void DucoNightboostMonthStart::update() {
  this->parent_->send(this, FUNCTION_PARAMETER, {0x00, NIGHTBOOST_MONTH_START_PARAMETER, NIGHTBOOST_GROUP});
}

void DucoNightboostMonthStart::receive_response(const DucoMessage &message) { publish_month(this, message); }

void DucoNightboostMonthStart::control(size_t index) {
  control_month(this->parent_, this, index, NIGHTBOOST_MONTH_START_PARAMETER);
}

void DucoNightboostMonthStop::update() {
  this->parent_->send(this, FUNCTION_PARAMETER, {0x00, NIGHTBOOST_MONTH_STOP_PARAMETER, NIGHTBOOST_GROUP});
}

void DucoNightboostMonthStop::receive_response(const DucoMessage &message) { publish_month(this, message); }

void DucoNightboostMonthStop::control(size_t index) {
  control_month(this->parent_, this, index, NIGHTBOOST_MONTH_STOP_PARAMETER);
}

}  // namespace esphome::duco
