#include "template_nmea.h"
#include "esphome/core/log.h"
#include <cmath>
#include <ctime>

namespace esphome {
namespace template_ {

static const char *const TAG = "template.nmea";

void TemplateNMEA::update() {
  // Check if required sensors are available
  if (this->latitude_sensor_ == nullptr || this->longitude_sensor_ == nullptr) {
    ESP_LOGW(TAG, "Latitude or longitude sensor not configured");
    return;
  }

  // Read sensor values
  float lat = this->latitude_sensor_->state;
  float lon = this->longitude_sensor_->state;

  // Validate required values
  if (!std::isfinite(lat) || !std::isfinite(lon)) {
    ESP_LOGW(TAG, "Invalid latitude or longitude value (NaN or Inf)");
    return;
  }

  // Populate GnssInfo structure
  nmea::GnssInfo gi;
  gi.lat_deg = lat;
  gi.lon_deg = lon;
  gi.fix_valid = true;

  // Optional: altitude (default to 0.0 if not available)
  if (this->altitude_sensor_ != nullptr && std::isfinite(this->altitude_sensor_->state)) {
    gi.alt_m = this->altitude_sensor_->state;
  } else {
    gi.alt_m = 0.0;
  }

  // Optional: speed in knots (default to 0.0)
  if (this->speed_sensor_ != nullptr && std::isfinite(this->speed_sensor_->state)) {
    gi.spd = this->speed_sensor_->state;
  } else {
    gi.spd = 0.0;
  }

  // Optional: course over ground (default to 0.0)
  if (this->course_sensor_ != nullptr && std::isfinite(this->course_sensor_->state)) {
    gi.cog_deg = this->course_sensor_->state;
  } else {
    gi.cog_deg = 0.0;
  }

  // Optional: HDOP (default to 1.0 for good precision)
  if (this->hdop_sensor_ != nullptr && std::isfinite(this->hdop_sensor_->state)) {
    gi.hdop = this->hdop_sensor_->state;
  } else {
    gi.hdop = 1.0;
  }

  // Optional: satellites used (default to 4)
  if (this->satellites_sensor_ != nullptr && std::isfinite(this->satellites_sensor_->state)) {
    gi.sat_used = static_cast<int>(this->satellites_sensor_->state);
  } else {
    gi.sat_used = 4;
  }

  // Set current time as UTC time
  gi.utc_time = std::time(nullptr);

  // Generate NMEA sentences using base class method
  this->populate_nmea_from_gnss_info_(gi);
}

}  // namespace template_
}  // namespace esphome
