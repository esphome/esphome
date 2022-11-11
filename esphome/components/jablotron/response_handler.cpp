#include "response_handler.h"
#include "string_view.h"
#include "esphome/core/log.h"

namespace esphome {
namespace jablotron {

static constexpr const char TAG[] = "jablotron";

bool ResponseHandler::is_last_response() const { return this->is_last_response_; }
void ResponseHandler::set_is_last_response_(bool value) { this->is_last_response_ = value; }

ResponseHandlerError::ResponseHandlerError() { this->set_is_last_response_(true); }
bool ResponseHandlerError::invoke(StringView response) const {
  if (response == "ERROR") {
    ESP_LOGE(TAG, "Received ERROR");
    return true;
  }
  if (try_remove_prefix_and_space(response, "ERROR:")) {
    ESP_LOGE(TAG, "Received ERROR: '%s'", response.data());
    return true;
  }
  return false;
}

ResponseHandlerOK::ResponseHandlerOK() { this->set_is_last_response_(true); }
bool ResponseHandlerOK::invoke(StringView response) const { return response == "OK"; }

ResponseHandlerPGState::ResponseHandlerPGState(const PGDeviceVector &devices) : devices_{devices} {
  this->set_is_last_response_(false);
}

bool ResponseHandlerPGState::invoke(StringView response) const {
  if (response == "PGSTATE:") {
    return true;
  }
  if (!try_remove_prefix_and_space(response, "PG")) {
    return false;
  }

  int index;
  if (!try_remove_integer_and_space(response, index)) {
    ESP_LOGE(TAG, "ResponseHandlerPGState: PG has no index: '%s'", response.data());
    return false;
  }

  bool state;
  if (response == "ON") {
    state = true;
  } else if (response == "OFF") {
    state = false;
  } else {
    ESP_LOGE(TAG, "ResponseHandlerPGState: Unknown PG state: '%s'", response.data());
    return false;
  }

  for (auto *device : this->devices_) {
    if (device->get_index() == index) {
      device->set_state(state);
    }
  }
  return true;
}

ResponseHandlerPrfState::ResponseHandlerPrfState(const PeripheralDeviceVector &devices) : devices_{devices} {
  this->set_is_last_response_(true);
}

bool ResponseHandlerPrfState::invoke(StringView response) const {
  if (!try_remove_prefix(response, "PRFSTATE ")) {
    return false;
  }
  for (auto *device : this->devices_) {
    auto state = get_bit_in_hex_string(response, device->get_index());
    device->set_state(state);
  }
  return true;
}

ResponseHandlerState::ResponseHandlerState(const SectionDeviceVector &devices) : devices_{devices} {
  this->set_is_last_response_(false);
}

bool ResponseHandlerState::invoke(StringView response) const {
  if (response == "STATE:") {
    return true;
  }
  if (!try_remove_prefix(response, "STATE ")) {
    return false;
  }
  int index;
  if (!try_remove_integer_and_space(response, index)) {
    ESP_LOGE(TAG, "ResponseHandlerState: STATE has no index: '%s'", response.data());
    return false;
  }
  for (auto *device : this->devices_) {
    if (device->get_index() == index) {
      device->set_state(response);
    }
  }
  return true;
}

ResponseHandlerVer::ResponseHandlerVer(const InfoDeviceVector &devices) : devices_{devices} {
  this->set_is_last_response_(true);
}

bool ResponseHandlerVer::invoke(StringView response) const {
  if (!response.starts_with("JA-121T,")) {
    return false;
  }
  for (auto *device : this->devices_) {
    device->set_state(response);
  }
  return true;
}

ResponseHandlerSectionFlag::ResponseHandlerSectionFlag(const SectionFlagDeviceVector &devices) : devices_{devices} {
  this->set_is_last_response_(false);
}

bool ResponseHandlerSectionFlag::invoke(StringView response) const {
  SectionFlag flag = SectionFlag::NONE;
  if (try_remove_prefix(response, "EXTERNAL_WARNING ")) {
    flag = SectionFlag::EXTERNAL_WARNING;
  } else if (try_remove_prefix(response, "INTERNAL_WARNING ")) {
    flag = SectionFlag::INTERNAL_WARNING;
  } else if (try_remove_prefix(response, "FIRE_ALARM ")) {
    flag = SectionFlag::FIRE_ALARM;
  } else if (try_remove_prefix(response, "INTRUDER_ALARM ")) {
    flag = SectionFlag::INTRUDER_ALARM;
  } else if (try_remove_prefix(response, "PANIC_ALARM ")) {
    flag = SectionFlag::PANIC_ALARM;
  } else if (try_remove_prefix(response, "ENTRY ")) {
    flag = SectionFlag::ENTRY;
  } else if (try_remove_prefix(response, "EXIT ")) {
    flag = SectionFlag::EXIT;
  } else {
    return false;
  }

  int index;
  if (!try_remove_integer_and_space(response, index)) {
    ESP_LOGE(TAG, "ResponseHandlerSectionFlag: section flag has no index: '%s'", response.data());
    return false;
  }

  bool state;
  if (response == "ON") {
    state = true;
  } else if (response == "OFF") {
    state = false;
  } else {
    ESP_LOGE(TAG, "ResponseHandlerSectionFlag: unknown flag state: '%s'", response.data());
    return false;
  }

  for (auto *device : this->devices_) {
    if (device->get_index() == index && device->get_flag() == flag) {
      device->set_state(state);
    }
  }
  return true;
}

}  // namespace jablotron
}  // namespace esphome
