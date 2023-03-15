#ifdef USE_ARDUINO

#include "esphome/core/log.h"
#include "optolink_device_info_sensor.h"

namespace esphome {
namespace optolink {

void OptolinkDeviceInfoSensor::setup() {
  datapoint_ = new Datapoint<conv4_1_UL>(get_name().c_str(), "optolink", 0x00f8, false);
  datapoint_->setCallback([this](const IDatapoint &dp, DPValue dp_value) {
    uint32_t value = dp_value.getU32();
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
}  // namespace optolink
}  // namespace esphome

#endif
