#include "sdp3x.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace sdp3x {

static const char *const TAG = "sdp3x.sensor";
static const uint16_t SDP3X_SOFT_RESET = 0x0006;
static const uint16_t SDP3X_READ_ID1 = 0x367C;
static const uint16_t SDP3X_READ_ID2 = 0xE102;
static const uint16_t SDP3X_START_DP_AVG = 0x3615;
static const uint16_t SDP3X_START_MASS_FLOW_AVG = 0x3603;
static const uint16_t SDP3X_STOP_MEAS = 0x3FF9;

void SDP3XComponent::update() { this->read_pressure_(); }

void SDP3XComponent::setup() {
  ESP_LOGD(TAG, "Setting up SDP3X...");

  if (!this->write_command(SDP3X_STOP_MEAS)) {
    ESP_LOGW(TAG, "Stop SDP3X failed!");  // This sometimes fails for no good reason
  }

  if (!this->write_command(SDP3X_SOFT_RESET)) {
    ESP_LOGW(TAG, "Soft Reset SDP3X failed!");  // This sometimes fails for no good reason
  }

  this->set_timeout(20, [this] {
    if (!this->write_command(SDP3X_READ_ID1)) {
      ESP_LOGE(TAG, "Read ID1 SDP3X failed!");
      this->mark_failed();
      return;
    }
    if (!this->write_command(SDP3X_READ_ID2)) {
      ESP_LOGE(TAG, "Read ID2 SDP3X failed!");
      this->mark_failed();
      return;
    }

    uint16_t data[6];
    if (!this->read_data(data, 6)) {
      ESP_LOGE(TAG, "Read ID SDP3X failed!");
      this->mark_failed();
      return;
    }

    // SDP8xx
    // ref:
    // https://www.sensirion.com/fileadmin/user_upload/customers/sensirion/Dokumente/8_Differential_Pressure/Datasheets/Sensirion_Differential_Pressure_Datasheet_SDP8xx_Digital.pdf
    if (data[1] >> 8 == 0x02) {
      switch (data[1] & 0xFF) {
        case 0x01:  // SDP800-500Pa
          ESP_LOGCONFIG(TAG, "Sensor is SDP800-500Pa");
          break;
        case 0x0A:  // SDP810-500Pa
          ESP_LOGCONFIG(TAG, "Sensor is SDP810-500Pa");
          break;
        case 0x04:  // SDP801-500Pa
          ESP_LOGCONFIG(TAG, "Sensor is SDP801-500Pa");
          break;
        case 0x0D:  // SDP811-500Pa
          ESP_LOGCONFIG(TAG, "Sensor is SDP811-500Pa");
          break;
        case 0x02:  // SDP800-125Pa
          ESP_LOGCONFIG(TAG, "Sensor is SDP800-125Pa");
          break;
        case 0x0B:  // SDP810-125Pa
          ESP_LOGCONFIG(TAG, "Sensor is SDP810-125Pa");
          break;
      }
    } else if (data[1] >> 8 == 0x01) {
      if ((data[1] & 0xFF) == 0x01) {
        ESP_LOGCONFIG(TAG, "Sensor is SDP31-500Pa");
      } else if ((data[1] & 0xFF) == 0x02) {
        ESP_LOGCONFIG(TAG, "Sensor is SDP32-125Pa");
      }
    }

    if (!this->write_command(measurement_mode_ == DP_AVG ? SDP3X_START_DP_AVG : SDP3X_START_MASS_FLOW_AVG)) {
      ESP_LOGE(TAG, "Start Measurements SDP3X failed!");
      this->mark_failed();
      return;
    }
    ESP_LOGCONFIG(TAG, "SDP3X started!");
  });
}
void SDP3XComponent::dump_config() {
  LOG_SENSOR("  ", "SDP3X", this);
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "  Connection with SDP3X failed!");
  }
  LOG_UPDATE_INTERVAL(this);
}

void SDP3XComponent::read_pressure_() {
  uint16_t data[3];
  if (!this->read_data(data, 3)) {
    ESP_LOGW(TAG, "Couldn't read SDP3X data!");
    this->status_set_warning();
    return;
  }

  int16_t pressure_raw = data[0];
  int16_t temperature_raw = data[1];
  int16_t scale_factor_raw = data[2];
  // scale factor is in Pa - convert to hPa
  float pressure = pressure_raw / (scale_factor_raw * 100.0f);
  ESP_LOGV(TAG, "Got raw pressure=%d, raw scale factor =%d, raw temperature=%d ", pressure_raw, scale_factor_raw,
           temperature_raw);
  ESP_LOGD(TAG, "Got Pressure=%.3f hPa", pressure);

  this->publish_state(pressure);
  this->status_clear_warning();
}

float SDP3XComponent::get_setup_priority() const { return setup_priority::DATA; }

}  // namespace sdp3x
}  // namespace esphome
