#include "mhz19.h"
#include "esphome/core/log.h"

namespace esphome {
namespace mhz19 {

static const char *TAG = "mhz19";
static const uint8_t MHZ19_PDU_LENGTH = 9;
static const uint8_t MHZ19_COMMAND_GET_PPM[] = {0xFF, 0x01, 0x86, 0x00, 0x00, 0x00, 0x00, 0x00, 0x79};
static const uint8_t MHZ19_COMMAND_ABC_ENABLE[] = {0xFF, 0x01, 0x79, 0xA0, 0x00, 0x00, 0x00, 0x00, 0xE6};
static const uint8_t MHZ19_COMMAND_ABC_DISABLE[] = {0xFF, 0x01, 0x79, 0x00, 0x00, 0x00, 0x00, 0x00, 0x86};

uint8_t mhz19_checksum(const uint8_t *command) {
  uint8_t sum = 0;
  for (uint8_t i = 1; i < MHZ19_PDU_LENGTH - 1; i++) {
    sum += command[i];
  }
  return 0xFF - sum + 0x01;
}

void MHZ19Component::setup() {
    /*
    By default sensor enables abc, only detect sensor version if we explicitly disabled abc
    */
    if (this->abc_enabled_ == false) {
      uint8_t response[MHZ19_PDU_LENGTH];
      uint32_t start = millis();
      while (!this->mhz19_write_command_(MHZ19_COMMAND_GET_PPM, response)) {
        if (millis() - start > 500) {
          /* MH-Z19B(s == 0) and MH-Z19(s != 0) */
          uint8_t s = response[5];
          if (response[5] == 0 && this->model_b_ == false) {
            ESP_LOGD(TAG, "MH-Z19B detected");
            this->model_b_ = true;
          }
        }
        yield();
      }
      /*
      Issue MHZ19_COMMAND_ABC_DISABLE only if we successfully detected model b
      */
      if (this->model_b_) {
        /* per spec response isn't expected but sensor replies anyway.
      * Read reply out and discard it so it won't get in the way of following commands */
        this->mhz19_write_command_(MHZ19_COMMAND_ABC_DISABLE, response);
      } else {
        this->mhz19_write_command_(MHZ19_COMMAND_ABC_ENABLE, response);
      }
    }
}

void MHZ19Component::update() {
  if (!this->setup_done) {
    return;
  }

  uint8_t response[MHZ19_PDU_LENGTH];
  if (!this->mhz19_write_command_(MHZ19_COMMAND_GET_PPM, response)) {
    ESP_LOGW(TAG, "Reading data from MHZ19 failed!");
    this->status_set_warning();
    return;
  }

  if (response[0] != 0xFF || response[1] != 0x86) {
    ESP_LOGW(TAG, "Invalid response from MHZ19!");
    this->status_set_warning();
    return;
  }

  /* Sensor reports U(15000) during boot, ingnore reported CO2 until it boots */
  uint16_t u = (response[6] << 8) + response[7];
  if (u == 15000) {
    ESP_LOGD(TAG, "Sensor is booting, measurements will be available in a while");
    return;
  }

  this->status_clear_warning();
  const uint16_t ppm = (uint16_t(response[2]) << 8) | response[3];
  const int temp = int(response[4]) - 40;
  const uint8_t status = response[5];

  ESP_LOGD(TAG, "MHZ19 Received CO₂=%uppm Temperature=%d°C Status=0x%02X", ppm, temp, status);
  if (this->co2_sensor_ != nullptr)
    this->co2_sensor_->publish_state(ppm);
  if (this->temperature_sensor_ != nullptr)
    this->temperature_sensor_->publish_state(temp);
}

bool MHZ19Component::mhz19_write_command_(const uint8_t *command, uint8_t *response) {
  bool ret;
  int rx_error = 0;
  uint32_t start = millis();
  do {
    this->write_array(command, MHZ19_PDU_LENGTH);
    this->flush();

    if (response == nullptr)
      return true;

    memset(response, 0, MHZ19_PDU_LENGTH);
    ret = this->read_array(response, MHZ19_PDU_LENGTH);

    uint8_t checksum = mhz19_checksum(response);
    if (checksum != response[8]) {
      ESP_LOGW(TAG, "MHZ19 Checksum doesn't match: 0x%02X!=0x%02X", response[8], checksum);
      ret = false;
      if (++rx_error > 2) {
          this->status_set_warning();
          /* give up trying to recover, hope that it will be fine next update cycle */
          rx_error = 0;
      }
      delay(100);
    } else {
      rx_error = 0;
    }
    yield();
  } while (rx_error);

  return ret;
}
float MHZ19Component::get_setup_priority() const { return setup_priority::DATA; }
void MHZ19Component::dump_config() {
  ESP_LOGCONFIG(TAG, "MH-Z19%s:", this->model_b_ ? "B" : "");
  ESP_LOGCONFIG(TAG, "  Automatic calibration: %s", this->abc_enabled_ ? " enabled" : "disabled");
  LOG_SENSOR("  ", "CO2", this->co2_sensor_);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
}

}  // namespace mhz19
}  // namespace esphome
