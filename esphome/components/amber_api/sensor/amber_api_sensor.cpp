#include "amber_api_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace amber_api {

static const char *const TAG = "amber_api.sensor";

void AmberApiSensor::on_amber_api_update(const AmberApiData &data) {
  float value = NAN;
  switch (this->type_) {
    case GENERAL:
      value = data.general_price;
      break;
    case GENERAL_FORECAST:
      value = data.general_forecast_price;
      break;
    case FEEDIN:
      value = data.feedin_price;
      break;
    case FEEDIN_FORECAST:
      value = data.feedin_forecast_price;
      break;
  }
  if (!std::isnan(value)) {
    this->publish_state(value);  // Convert cents to dollars
  }
}

void AmberApiSensor::dump_config() {
  LOG_SENSOR("", "Amber API Sensor", this);
  const char *type_str;
  switch (this->type_) {
    default:
      type_str = "General Price";
      break;
    case GENERAL_FORECAST:
      type_str = "General Forecast Price";
      break;
    case FEEDIN:
      type_str = "Feed-in Price";
      break;
    case FEEDIN_FORECAST:
      type_str = "Feed-in Forecast Price";
      break;
  }
  ESP_LOGCONFIG(TAG, "  Type: %s", type_str);
}

}  // namespace amber_api
}  // namespace esphome
