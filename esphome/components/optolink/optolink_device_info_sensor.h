#pragma once

#include "esphome/components/text_sensor/text_sensor.h"
#include "optolink.h"
#include "optolink_sensor_base.h"
#include <VitoWiFi.h>

namespace esphome {
namespace optolink {

class OptolinkDeviceInfoSensor : public esphome::text_sensor::TextSensor, public esphome::PollingComponent {
 public:
  OptolinkDeviceInfoSensor(std::string name, Optolink *optolink) {
    optolink_ = optolink;
    set_name(name);
    set_update_interval(1800000);
    set_entity_category(esphome::ENTITY_CATEGORY_DIAGNOSTIC);
  }

 protected:
  void setup() override {
    datapoint_ = new Datapoint<conv4_1_UL>(get_name().c_str(), "optolink", 0x00f8, false);
    datapoint_->setCallback([this](const IDatapoint &dp, DPValue dpValue) {
      uint32_t value = dpValue.getU32();
      ESP_LOGD("OptolinkTextSensor", "Datapoint %s - %s: %d", dp.getGroup(), dp.getName(), value);
      uint8_t *bytes = (uint8_t *) &value;
      uint16_t tmp = esphome::byteswap(*((uint16_t *) bytes));
      std::string geraetekennung = esphome::format_hex_pretty(&tmp, 1);
      std::string hardware_revision = esphome::format_hex_pretty((uint8_t *) bytes + 2, 1);
      std::string software_index = esphome::format_hex_pretty((uint8_t *) bytes + 3, 1);
      publish_state("Device ID: " + geraetekennung + "|Hardware Revision: " + hardware_revision +
                    "|Software Index: " + software_index);
    });
  }
  void update() override { optolink_->read_value(datapoint_); }

 private:
  Optolink *optolink_;
  IDatapoint *datapoint_;
};

}  // namespace optolink
}  // namespace esphome
