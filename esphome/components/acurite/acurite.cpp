#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/core/hal.h"
#include "acurite.h"

namespace esphome {
namespace acurite {

static const char *const TAG = "acurite";

// standard channel mapping for the majority of acurite devices
static const char CHANNEL_LUT[4] = {'C', 'X', 'B', 'A'};

// the lightning distance lookup table was taken from the AS3935 datasheet
static const int8_t AS3935_LUT[32] = {2,  2,  2,  2,  5,  6,  6,  8,  10, 10, 12, 12, 14, 14, 14, 17,
                                      17, 20, 20, 20, 24, 24, 27, 27, 31, 31, 31, 34, 37, 37, 40, 40};

bool AcuRiteComponent::validate_(uint8_t *data, uint8_t len, int8_t except) {
  ESP_LOGV(TAG, "Validating data: %s", format_hex(data, len).c_str());

  // checksum
  uint8_t sum = 0;
  for (int32_t i = 0; i < len - 1; i++) {
    sum += data[i];
  }
  if (sum != data[len - 1]) {
    ESP_LOGV(TAG, "Checksum failure %02x vs %02x", sum, data[len - 1]);
    return false;
  }

  // parity (excludes id and crc)
  for (int32_t i = 2; i < len - 1; i++) {
    uint8_t sum = 0;
    for (int32_t b = 0; b < 8; b++) {
      sum ^= data[i] >> b;
    }
    if ((sum & 1) != 0 && i != except) {
      ESP_LOGV(TAG, "Parity failure on byte %d", i);
      return false;
    }
  }
  return true;
}

void AcuRiteComponent::decode_temperature_(uint8_t *data, uint8_t len) {
  if (len == 7 && (data[2] & 0x3F) == 0x04 && this->validate_(data, 7, -1)) {
    char channel = CHANNEL_LUT[data[0] >> 6];
    uint16_t id = ((data[0] & 0x3F) << 8) | (data[1] & 0xFF);
    uint16_t battery = (data[2] >> 6) & 1;
    float humidity = data[3] & 0x7F;
    float temp = ((float) (((data[4] & 0x0F) << 7) | (data[5] & 0x7F)) - 1000) / 10.0;
    ESP_LOGD(TAG, "Temperature: ch %c, id %04x, bat %d, temp %.1f, rh %.1f", channel, id, battery, temp, humidity);
    for (auto *device : this->devices_) {
      if (device->get_id() == id) {
        device->update_battery(battery);
        device->update_temperature(temp);
        device->update_humidity(humidity);
      }
    }
  }
}

void AcuRiteComponent::decode_rainfall_(uint8_t *data, uint8_t len) {
  if (len == 8 && (data[2] & 0x3F) == 0x30 && this->validate_(data, 8, -1)) {
    static const char CHANNEL_LUT[4] = {'A', 'B', 'C', 'X'};
    char channel = CHANNEL_LUT[data[0] >> 6];
    uint16_t id = ((data[0] & 0x3F) << 8) | (data[1] & 0xFF);
    uint16_t battery = (data[2] >> 6) & 1;
    uint32_t count = ((data[4] & 0x7F) << 14) | ((data[5] & 0x7F) << 7) | ((data[6] & 0x7F) << 0);
    ESP_LOGD(TAG, "Rainfall:    ch %c, id %04x, bat %d, count %d", channel, id, battery, count);
    for (auto *device : this->devices_) {
      if (device->get_id() == id) {
        device->update_battery(battery);
        device->update_rainfall(count);
      }
    }
  }
}

void AcuRiteComponent::decode_lightning_(uint8_t *data, uint8_t len) {
  if (len == 9 && (data[2] & 0x3F) == 0x2F && this->validate_(data, 9, -1)) {
    char channel = CHANNEL_LUT[data[0] >> 6];
    uint16_t id = ((data[0] & 0x3F) << 8) | (data[1] & 0xFF);
    uint16_t battery = (data[2] >> 6) & 1;
    float humidity = data[3] & 0x7F;
    float temp = ((float) (((data[4] & 0x1F) << 7) | (data[5] & 0x7F)) - 1800) * 0.1 * 5.0 / 9.0;
    float distance = AS3935_LUT[data[7] & 0x1F];
    uint16_t count = ((data[6] & 0x7F) << 1) | ((data[7] >> 6) & 1);
    uint16_t rfi = (data[7] >> 5) & 1;
    ESP_LOGD(TAG, "Lightning:   ch %c, id %04x, bat %d, temp %.1f, rh %.1f, count %d, dist %.1f, rfi %d", channel, id,
             battery, temp, humidity, count, distance, rfi);
    for (auto *device : this->devices_) {
      if (device->get_id() == id) {
        device->update_battery(battery);
        device->update_temperature(temp);
        device->update_humidity(humidity);
        device->update_lightning(count);
        device->update_distance(distance);
      }
    }
  }
}

void AcuRiteComponent::decode_atlas_(uint8_t *data, uint8_t len) {
  if (len == 10 && this->validate_(data, 10, -1)) {
    uint8_t msg = data[2] & 0x3F;
    if (msg == 0x05 || msg == 0x06 || msg == 0x07 || msg == 0x25 || msg == 0x26 || msg == 0x27) {
      char channel = CHANNEL_LUT[data[0] >> 6];
      uint16_t id = ((data[0] & 0x03) << 8) | (data[1] & 0xFF);
      uint16_t battery = (data[2] >> 6) & 1;
      float speed = (float) (((data[3] & 0x7F) << 1) | ((data[4] & 0x40) >> 6)) * 1.60934f;
      int16_t lightning = -1;
      int16_t distance = -1;
      if (msg == 0x25 || msg == 0x26 || msg == 0x27) {
        lightning = ((data[7] & 0x7F) << 2) | ((data[8] & 0x60) >> 5);
        distance = AS3935_LUT[data[8] & 0x1F];
      }
      if (msg == 0x05 || msg == 0x25) {
        float temp = ((float) (((data[4] & 0x0F) << 7) | (data[5] & 0x7F)) - 720) * 0.1f * 5.0f / 9.0f;
        float humidity = data[6] & 0x7F;
        ESP_LOGD(TAG, "Atlas 7in1:  ch %c, id %04x, bat %d, speed %.1f, lightning %d, distance %d, temp %.1f, rh %.1f",
                 channel, id, battery, speed, lightning, distance, temp, humidity);
        for (auto *device : this->devices_) {
          if (device->get_id() == id) {
            device->update_battery(battery);
            device->update_speed(speed);
            device->update_temperature(temp);
            device->update_humidity(humidity);
            if (lightning >= 0) {
              device->update_lightning(lightning);
              device->update_distance(distance);
            }
          }
        }
      } else if (msg == 0x06 || msg == 0x26) {
        float direction = ((data[4] & 0x1F) << 5) | ((data[5] & 0x7C) >> 2);
        uint32_t rain = ((data[5] & 0x03) << 7) | (data[6] & 0x7F);
        ESP_LOGD(TAG, "Atlas 7in1:  ch %c, id %04x, bat %d, speed %.1f, lightning %d, distance %d, dir %.1f, rain %d",
                 channel, id, battery, speed, lightning, distance, direction, rain);
        for (auto *device : this->devices_) {
          if (device->get_id() == id) {
            device->update_battery(battery);
            device->update_speed(speed);
            device->update_direction(direction);
            device->update_rainfall(rain);
            if (lightning >= 0) {
              device->update_lightning(lightning);
              device->update_distance(distance);
            }
          }
        }
      } else if (msg == 0x07 || msg == 0x27) {
        uint32_t uv = (data[4] & 0x0F);
        uint32_t lux = (((data[5] & 0x7F) << 7) | (data[6] & 0x7F)) * 10;
        ESP_LOGD(TAG, "Atlas 7in1:  ch %c, id %04x, bat %d, speed %.1f, lightning %d, distance %d, uv %d, lux %d",
                 channel, id, battery, speed, lightning, distance, uv, lux);
        for (auto *device : this->devices_) {
          if (device->get_id() == id) {
            device->update_battery(battery);
            device->update_speed(speed);
            device->update_uv(uv);
            device->update_lux(lux);
            if (lightning >= 0) {
              device->update_lightning(lightning);
              device->update_distance(distance);
            }
          }
        }
      }
    }
  }
}

void AcuRiteComponent::decode_notos_(uint8_t *data, uint8_t len) {
  // the wind speed conversion value was derived by sending all possible raw values
  // my acurite, the conversion here will match almost exactly
  if (len == 8 && (data[2] & 0x3F) == 0x20 && this->validate_(data, 8, 6)) {
    char channel = CHANNEL_LUT[data[0] >> 6];
    uint16_t id = ((data[0] & 0x3F) << 8) | (data[1] & 0xFF);
    uint16_t battery = (data[2] >> 6) & 1;
    float humidity = data[3] & 0x7F;
    float temp = ((float) (((data[4] & 0x1F) << 7) | (data[5] & 0x7F)) - 1800) * 0.1f * 5.0f / 9.0f;
    float speed = (float) (data[6] & 0x7F) * 2.5734f;
    ESP_LOGD(TAG, "Notos 3in1:  ch %c, id %04x, bat %d, temp %.1f, rh %.1f, speed %.1f", channel, id, battery, temp,
             humidity, speed);
    for (auto *device : this->devices_) {
      if (device->get_id() == id) {
        device->update_battery(battery);
        device->update_temperature(temp);
        device->update_humidity(humidity);
        device->update_speed(speed);
      }
    }
  }
}

void AcuRiteComponent::decode_iris_(uint8_t *data, uint8_t len) {
  // the wind speed and direction conversion value were derived by sending all possible
  // raw values my acurite, the conversion here will match almost exactly
  if (len == 8 && this->validate_(data, 8, -1)) {
    uint8_t msg = data[2] & 0x3F;
    if (msg == 0x31 || msg == 0x38) {
      static const float DIR_LUT[16] = {315.0f, 247.5f, 292.5f, 270.0f, 337.5f, 225.0f, 0.0f,  202.5f,
                                        67.5f,  135.0f, 90.0f,  112.5f, 45.0f,  157.5f, 22.5f, 180.0f};
      char channel = CHANNEL_LUT[data[0] >> 6];
      uint16_t id = ((data[0] & 0x0F) << 8) | (data[1] & 0xFF);
      uint16_t battery = (data[2] >> 6) & 1;
      float speed = (float) (((data[3] & 0x1F) << 3) | ((data[4] & 0x70) >> 4)) * 0.839623f;
      if (msg == 0x31) {
        float direction = DIR_LUT[data[4] & 0x0F];
        uint32_t count = ((data[5] & 0x7F) << 7) | (data[6] & 0x7F);
        ESP_LOGD(TAG, "Iris 5in1:   ch %c, id %04x, bat %d, speed %.1f, dir %.1f, rain %d", channel, id, battery, speed,
                 direction, count);
        for (auto *device : this->devices_) {
          if (device->get_id() == id) {
            device->update_battery(battery);
            device->update_speed(speed);
            device->update_direction(direction);
            device->update_rainfall(count);
          }
        }
      } else {
        float temp = ((float) (((data[4] & 0x0F) << 7) | (data[5] & 0x7F)) - 720) * 0.1f * 5.0f / 9.0f;
        float humidity = data[6] & 0x7F;
        ESP_LOGD(TAG, "Iris 5in1:   ch %c, id %04x, bat %d, speed %.1f, temp %.1f, rh %.1f", channel, id, battery,
                 speed, temp, humidity);
        for (auto *device : this->devices_) {
          if (device->get_id() == id) {
            device->update_battery(battery);
            device->update_speed(speed);
            device->update_temperature(temp);
            device->update_humidity(humidity);
          }
        }
      }
    }
  }
}

bool AcuRiteComponent::on_receive(remote_base::RemoteReceiveData data) {
  uint32_t syncs = 0;
  uint32_t bits = 0;
  uint8_t bytes[10];

  ESP_LOGV(TAG, "Received raw data with length %d", data.size());

  // demodulate AcuRite OOK data
  for (auto i : data.get_raw_data()) {
    bool sync = std::abs(600 - std::abs(i)) < 100;
    bool zero = std::abs(200 - std::abs(i)) < 100;
    bool one = std::abs(400 - std::abs(i)) < 100;
    bool level = (i >= 0);
    if ((one || zero) && syncs > 2) {
      if (level) {
        // detect bits using on state
        bytes[bits / 8] <<= 1;
        bytes[bits / 8] |= one ? 1 : 0;
        bits += 1;

        // try to decode on whole bytes
        if ((bits & 7) == 0) {
          this->decode_temperature_(bytes, bits / 8);
          this->decode_rainfall_(bytes, bits / 8);
          this->decode_lightning_(bytes, bits / 8);
          this->decode_atlas_(bytes, bits / 8);
          this->decode_notos_(bytes, bits / 8);
          this->decode_iris_(bytes, bits / 8);
        }

        // reset if buffer is full
        if (bits >= sizeof(bytes) * 8) {
          bits = 0;
          syncs = 0;
        }
      }
    } else if (sync && bits == 0) {
      // count sync using off state
      if (!level) {
        syncs++;
      }
    } else {
      // reset if state is invalid
      bits = 0;
      syncs = 0;
    }
  }
  return true;
}

void AcuRiteComponent::setup() { this->remote_receiver_->register_listener(this); }

}  // namespace acurite
}  // namespace esphome
