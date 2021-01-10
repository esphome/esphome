#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace teleinfo {
/*
 * 198 bytes should be enough to contain a full session in historical mode with
 * three phases. But go with 1024 just to be sure.
 */
static const uint8_t MAX_TAG_SIZE = 64;
static const uint16_t MAX_VAL_SIZE = 256;
static const uint16_t MAX_BUF_SIZE = 1024;

struct TeleinfoSensorElement {
  const char *tag;
  sensor::Sensor *sensor;
};

class TeleInfo : public PollingComponent, public uart::UARTDevice {
 public:
  TeleInfo(bool historical_mode);
  void register_teleinfo_sensor(const char *tag, sensor::Sensor *sensors);
  void loop() override;
  void setup() override;
  void update() override;
  void dump_config() override;
  std::vector<const TeleinfoSensorElement *> teleinfo_sensors_{};

 protected:
  uint32_t baud_rate_;
  int checksum_area_end_;
  int separator_;
  char buf_[MAX_BUF_SIZE];
  uint32_t buf_index_{0};
  char tag_[MAX_TAG_SIZE];
  char val_[MAX_VAL_SIZE];
  enum State {
    OFF,
    ON,
    START_FRAME_RECEIVED,
    END_FRAME_RECEIVED,
  } state_{OFF};
  bool read_chars_until_(bool drop, uint8_t c);
  bool check_crc_(const char *grp, const char *grp_end);
  void publish_value_(std::string tag, std::string val);
};
}  // namespace teleinfo
}  // namespace esphome
