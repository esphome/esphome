#include "dht.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace dht {

static const char *const TAG = "dht";

void DHT::setup() {
  ESP_LOGCONFIG(TAG, "Setting up DHT...");
  this->pin_->digital_write(true);
  this->pin_->setup();
  this->pin_->digital_write(true);
}
void DHT::dump_config() {
  ESP_LOGCONFIG(TAG, "DHT:");
  LOG_PIN("  Pin: ", this->pin_);
  if (this->is_auto_detect_) {
    ESP_LOGCONFIG(TAG, "  Auto-detected model: %s", this->model_ == DHT_MODEL_DHT11 ? "DHT11" : "DHT22");
  } else if (this->model_ == DHT_MODEL_DHT11) {
    ESP_LOGCONFIG(TAG, "  Model: DHT11");
  } else {
    ESP_LOGCONFIG(TAG, "  Model: DHT22 (or equivalent)");
  }

  LOG_UPDATE_INTERVAL(this);

  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
}

void DHT::update() {
  float temperature, humidity;
  bool success;
  if (this->model_ == DHT_MODEL_AUTO_DETECT) {
    this->model_ = DHT_MODEL_DHT22;
    success = this->read_sensor_(&temperature, &humidity, false);
    if (!success) {
      this->model_ = DHT_MODEL_DHT11;
      return;
    }
  } else {
    success = this->read_sensor_(&temperature, &humidity, true);
  }

  if (success) {
    ESP_LOGD(TAG, "Got Temperature=%.1f°C Humidity=%.1f%%", temperature, humidity);

    if (this->temperature_sensor_ != nullptr)
      this->temperature_sensor_->publish_state(temperature);
    if (this->humidity_sensor_ != nullptr)
      this->humidity_sensor_->publish_state(humidity);
    this->status_clear_warning();
  } else {
    const char *str = "";
    if (this->is_auto_detect_) {
      str = " and consider manually specifying the DHT model using the model option";
    }
    ESP_LOGW(TAG, "Invalid readings! Please check your wiring (pull-up resistor, pin number)%s.", str);
    if (this->temperature_sensor_ != nullptr)
      this->temperature_sensor_->publish_state(NAN);
    if (this->humidity_sensor_ != nullptr)
      this->humidity_sensor_->publish_state(NAN);
    this->status_set_warning();
  }
}

float DHT::get_setup_priority() const { return setup_priority::DATA; }
void DHT::set_dht_model(DHTModel model) {
  this->model_ = model;
  this->is_auto_detect_ = model == DHT_MODEL_AUTO_DETECT;
}
bool HOT IRAM_ATTR DHT::read_sensor_(float *temperature, float *humidity, bool report_errors) {
  *humidity = NAN;
  *temperature = NAN;

  int error_code = 0;
  int8_t i = 0;
  uint8_t data[5] = {0, 0, 0, 0, 0};

  this->pin_->digital_write(false);
  this->pin_->pin_mode(gpio::FLAG_OUTPUT);
  this->pin_->digital_write(false);

  if (this->model_ == DHT_MODEL_DHT11) {
    delayMicroseconds(18000);
  } else if (this->model_ == DHT_MODEL_SI7021) {
    delayMicroseconds(500);
    this->pin_->digital_write(true);
    delayMicroseconds(40);
  } else if (this->model_ == DHT_MODEL_DHT22_TYPE2) {
    delayMicroseconds(2000);
  } else if (this->model_ == DHT_MODEL_AM2302) {
    delayMicroseconds(1000);
  } else {
    delayMicroseconds(800);
  }
  this->pin_->pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);

  {
    InterruptLock lock;
    // Host pull up 20-40us then DHT response 80us
    // Start waiting for initial rising edge at the center when we
    // expect the DHT response (30us+40us)
    delayMicroseconds(70);

    uint8_t bit = 7;
    uint8_t byte = 0;

    for (i = -1; i < 40; i++) {
      uint32_t start_time = micros();

      // Wait for rising edge
      while (!this->pin_->digital_read()) {
        if (micros() - start_time > 90) {
          if (i < 0) {
            error_code = 1;
          } else {
            error_code = 2;
          }
          break;
        }
      }
      if (error_code != 0)
        break;

      start_time = micros();
      uint32_t end_time = start_time;

      // Wait for falling edge
      while (this->pin_->digital_read()) {
        if ((end_time = micros()) - start_time > 90) {
          if (i < 0) {
            error_code = 3;
          } else {
            error_code = 4;
          }
          break;
        }
      }
      if (error_code != 0)
        break;

      if (i < 0)
        continue;

      if (end_time - start_time >= 40) {
        data[byte] |= 1 << bit;
      }
      if (bit == 0) {
        bit = 7;
        byte++;
      } else
        bit--;
    }
  }
  if (!report_errors && error_code != 0)
    return false;

  switch (error_code) {
    case 1:
      ESP_LOGW(TAG, "Waiting for DHT communication to clear failed!");
      return false;
    case 2:
      ESP_LOGW(TAG, "Rising edge for bit %d failed!", i);
      return false;
    case 3:
      ESP_LOGW(TAG, "Requesting data from DHT failed!");
      return false;
    case 4:
      ESP_LOGW(TAG, "Falling edge for bit %d failed!", i);
      return false;
    case 0:
    default:
      break;
  }

  ESP_LOGVV(TAG,
            "Data: Hum=0b" BYTE_TO_BINARY_PATTERN BYTE_TO_BINARY_PATTERN
            ", Temp=0b" BYTE_TO_BINARY_PATTERN BYTE_TO_BINARY_PATTERN ", Checksum=0b" BYTE_TO_BINARY_PATTERN,
            BYTE_TO_BINARY(data[0]), BYTE_TO_BINARY(data[1]), BYTE_TO_BINARY(data[2]), BYTE_TO_BINARY(data[3]),
            BYTE_TO_BINARY(data[4]));

  uint8_t checksum_a = data[0] + data[1] + data[2] + data[3];
  // On the DHT11, two algorithms for the checksum seem to be used, either the one from the DHT22,
  // or just using bytes 0 and 2
  uint8_t checksum_b = this->model_ == DHT_MODEL_DHT11 ? (data[0] + data[2]) : checksum_a;

  if (checksum_a != data[4] && checksum_b != data[4]) {
    if (report_errors) {
      ESP_LOGW(TAG, "Checksum invalid: %u!=%u", checksum_a, data[4]);
    }
    return false;
  }

  if (this->model_ == DHT_MODEL_DHT11) {
    if (checksum_a == data[4]) {
      // Data format: 8bit integral RH data + 8bit decimal RH data + 8bit integral T data + 8bit decimal T data + 8bit
      // check sum - some models always have 0 in the decimal part
      const uint16_t raw_temperature = uint16_t(data[2]) * 10 + (data[3] & 0x7F);
      *temperature = raw_temperature / 10.0f;
      if ((data[3] & 0x80) != 0) {
        // negative
        *temperature *= -1;
      }

      const uint16_t raw_humidity = uint16_t(data[0]) * 10 + data[1];
      *humidity = raw_humidity / 10.0f;
    } else {
      // For compatibility with DHT11 models which might only use 2 bytes checksums, only use the data from these two
      // bytes
      *temperature = data[2];
      *humidity = data[0];
    }
  } else {
    uint16_t raw_humidity = (uint16_t(data[0] & 0xFF) << 8) | (data[1] & 0xFF);
    uint16_t raw_temperature = (uint16_t(data[2] & 0xFF) << 8) | (data[3] & 0xFF);

    if (this->model_ != DHT_MODEL_DHT22_TYPE2 && (raw_temperature & 0x8000) != 0)
      raw_temperature = ~(raw_temperature & 0x7FFF);

    if (raw_temperature == 1 && raw_humidity == 10) {
      if (report_errors) {
        ESP_LOGW(TAG, "Invalid temperature+humidity! Sensor reported 1°C and 1%% Hum");
      }
      return false;
    }

    *humidity = raw_humidity * 0.1f;
    if (*humidity > 100)
      *humidity = NAN;
    *temperature = int16_t(raw_temperature) * 0.1f;
  }

  if (*temperature == 0.0f && (*humidity == 1.0f || *humidity == 2.0f)) {
    if (report_errors) {
      ESP_LOGW(TAG, "DHT reports invalid data. Is the update interval too high or the sensor damaged?");
    }
    return false;
  }

  return true;
}

}  // namespace dht
}  // namespace esphome
