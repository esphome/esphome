#include "nightboost_time.h"

#include <cctype>
#include <cstdio>

#include "esphome/core/log.h"

namespace esphome::duco {

ESPHOME_LOG_TAG(TAG, "duco.nightboost_time");

static constexpr uint8_t NIGHTBOOST_ADDRESS = 0x01;
static constexpr uint8_t NIGHTBOOST_GROUP = 0x03;
static constexpr uint8_t NIGHTBOOST_TIME_START_PARAMETER = 0x05;
static constexpr uint8_t NIGHTBOOST_TIME_STOP_PARAMETER = 0x06;

static void publish_time(text::Text *text, const DucoMessage &message) {
  if (message.size < 5)
    return;

  const uint16_t minutes = encode_uint16(message.data[4], message.data[3]);
  if (minutes >= 24 * 60)
    return;

  char value[6];
  snprintf(value, sizeof(value), "%02u:%02u", minutes / 60, minutes % 60);
  text->publish_state(value);
}

static bool parse_time(const std::string &value, uint16_t *minutes) {
  if (value.size() != 5 || value[2] != ':')
    return false;
  for (size_t index : {0, 1, 3, 4}) {
    if (!std::isdigit(static_cast<unsigned char>(value[index])))
      return false;
  }

  const uint8_t hours = (value[0] - '0') * 10 + value[1] - '0';
  const uint8_t minute = (value[3] - '0') * 10 + value[4] - '0';
  if (hours >= 24 || minute >= 60)
    return false;

  *minutes = hours * 60 + minute;
  return true;
}

static void control_time(Duco *parent, text::Text *text, const std::string &value, uint8_t parameter) {
  if (!parent->allows_dangerous_controls()) {
    ESP_LOGW(TAG, "NightBoost time change rejected: allow_dangerous_controls is disabled");
    text->publish_state(text->state);
    return;
  }

  uint16_t minutes;
  if (!parse_time(value, &minutes)) {
    ESP_LOGW(TAG, "NightBoost time must use HH:MM");
    text->publish_state(text->state);
    return;
  }

  parent->send(nullptr, FUNCTION_PARAMETER,
               {NIGHTBOOST_ADDRESS, parameter, NIGHTBOOST_GROUP, static_cast<uint8_t>(minutes & 0xFF),
                static_cast<uint8_t>(minutes >> 8), 0x00, 0x00});
}

void DucoNightboostTimeStart::update() {
  this->parent_->send(this, FUNCTION_PARAMETER, {0x00, NIGHTBOOST_TIME_START_PARAMETER, NIGHTBOOST_GROUP});
}

void DucoNightboostTimeStart::receive_response(const DucoMessage &message) { publish_time(this, message); }

void DucoNightboostTimeStart::control(const std::string &value) {
  control_time(this->parent_, this, value, NIGHTBOOST_TIME_START_PARAMETER);
}

void DucoNightboostTimeStop::update() {
  this->parent_->send(this, FUNCTION_PARAMETER, {0x00, NIGHTBOOST_TIME_STOP_PARAMETER, NIGHTBOOST_GROUP});
}

void DucoNightboostTimeStop::receive_response(const DucoMessage &message) { publish_time(this, message); }

void DucoNightboostTimeStop::control(const std::string &value) {
  control_time(this->parent_, this, value, NIGHTBOOST_TIME_STOP_PARAMETER);
}

}  // namespace esphome::duco
