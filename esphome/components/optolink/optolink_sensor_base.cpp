#ifdef USE_ARDUINO

#include "optolink_sensor_base.h"
#include "optolink.h"

namespace esphome {
namespace optolink {

void OptolinkSensorBase::update_datapoint_(float value) {
  if (!writeable_) {
    optolink_->set_error("try to control not writable number %s", get_sensor_name().c_str());
    ESP_LOGE("OptolinkSensorBase", "try to control not writable number %s", get_sensor_name().c_str());
  } else if (datapoint_ != nullptr) {
    switch (bytes_) {
      case 1:
        switch (div_ratio_) {
          case 1:
            optolink_->write_value(datapoint_, DPValue((uint8_t) value));
            break;
          case 10:
            optolink_->write_value(datapoint_, DPValue((float) value));
            break;
          default:
            optolink_->set_error("Unknown byte/div_ratio combination for number %s", get_sensor_name().c_str());
            ESP_LOGE("OptolinkSensorBase", "Unknown byte/div_ratio combination for number %s",
                     get_sensor_name().c_str());
            break;
        }
        break;
      case 2:
        switch (div_ratio_) {
          case 1:
            optolink_->write_value(datapoint_, DPValue((uint16_t) value));
            break;
          case 10:
          case 100:
            optolink_->write_value(datapoint_, DPValue((float) value));
            break;
          default:
            optolink_->set_error("Unknown byte/div_ratio combination for number %s", get_sensor_name().c_str());
            ESP_LOGE("OptolinkSensorBase", "Unknown byte/div_ratio combination for number %s",
                     get_sensor_name().c_str());
            break;
        }
        break;
      case 4:
        switch (div_ratio_) {
          case 1:
            optolink_->write_value(datapoint_, DPValue((uint32_t) value));
            break;
          case 3600:
            optolink_->write_value(datapoint_, DPValue((float) value));
            break;
          default:
            optolink_->set_error("Unknown byte/div_ratio combination for number %s", get_sensor_name().c_str());
            ESP_LOGE("OptolinkSensorBase", "Unknown byte/div_ratio combination for number %s",
                     get_sensor_name().c_str());
            break;
        }
        break;
      default:
        optolink_->set_error("Unknown byte value for number %s", get_sensor_name().c_str());
        ESP_LOGE("OptolinkSensorBase", "Unknown byte value for number %s", get_sensor_name().c_str());
        break;
    }
  }
}

void OptolinkSensorBase::setup_datapoint_() {
  switch (bytes_) {
    case 1:
      switch (div_ratio_) {
        case 1:
          datapoint_ = new Datapoint<conv1_1_US>(get_sensor_name().c_str(), "optolink", address_, writeable_);
          datapoint_->setCallback([this](const IDatapoint &dp, DPValue dp_value) {
            ESP_LOGD("OptolinkSensorBase", "Datapoint %s - %s: %d", dp.getGroup(), dp.getName(), dp_value.getU8());
            value_changed(dp_value.getU8());
          });
          break;
        case 10:
          datapoint_ = new Datapoint<conv1_10_F>(get_sensor_name().c_str(), "optolink", address_, writeable_);
          datapoint_->setCallback([this](const IDatapoint &dp, DPValue dp_value) {
            ESP_LOGD("OptolinkSensorBase", "Datapoint %s - %s: %f", dp.getGroup(), dp.getName(), dp_value.getFloat());
            value_changed(dp_value.getFloat());
          });
          break;
        default:
          optolink_->set_error("Unknown byte/div_ratio combination for sensor %s", get_sensor_name().c_str());
          ESP_LOGE("OptolinkSensorBase", "Unknown byte/div_ratio combination for sensor %s", get_sensor_name().c_str());
          break;
      }
      break;
    case 2:
      switch (div_ratio_) {
        case 1:
          datapoint_ = new Datapoint<conv2_1_US>(get_sensor_name().c_str(), "optolink", address_, writeable_);
          datapoint_->setCallback([this](const IDatapoint &dp, DPValue dp_value) {
            ESP_LOGD("OptolinkSensorBase", "Datapoint %s - %s: %d", dp.getGroup(), dp.getName(), dp_value.getU16());
            value_changed(dp_value.getU16());
          });
          break;
        case 10:
          datapoint_ = new Datapoint<conv2_10_F>(get_sensor_name().c_str(), "optolink", address_, writeable_);
          datapoint_->setCallback([this](const IDatapoint &dp, DPValue dp_value) {
            ESP_LOGD("OptolinkSensorBase", "Datapoint %s - %s: %f", dp.getGroup(), dp.getName(), dp_value.getFloat());
            value_changed(dp_value.getFloat());
          });
          break;
        case 100:
          datapoint_ = new Datapoint<conv2_100_F>(get_sensor_name().c_str(), "optolink", address_, writeable_);
          datapoint_->setCallback([this](const IDatapoint &dp, DPValue dp_value) {
            ESP_LOGD("OptolinkSensorBase", "Datapoint %s - %s: %f", dp.getGroup(), dp.getName(), dp_value.getFloat());
            value_changed(dp_value.getFloat());
          });
          break;
        default:
          optolink_->set_error("Unknown byte/div_ratio combination for sensor %s", get_sensor_name().c_str());
          ESP_LOGE("OptolinkSensorBase", "Unknown byte/div_ratio combination for sensor %s", get_sensor_name().c_str());
          break;
      }
      break;
    case 4:
      switch (div_ratio_) {
        case 1:
          datapoint_ = new Datapoint<conv4_1_UL>(get_sensor_name().c_str(), "optolink", address_, writeable_);
          datapoint_->setCallback([this](const IDatapoint &dp, DPValue dp_value) {
            ESP_LOGD("OptolinkSensorBase", "Datapoint %s - %s: %d", dp.getGroup(), dp.getName(), dp_value.getU32());
            value_changed(dp_value.getU32());
          });
          break;
        case 3600:
          datapoint_ = new Datapoint<conv4_3600_F>(get_sensor_name().c_str(), "optolink", address_, writeable_);
          datapoint_->setCallback([this](const IDatapoint &dp, DPValue dp_value) {
            ESP_LOGD("OptolinkSensorBase", "Datapoint %s - %s: %f", dp.getGroup(), dp.getName(), dp_value.getFloat());
            value_changed(dp_value.getFloat());
          });
          break;
        default:
          optolink_->set_error("Unknown byte/div_ratio combination for sensor %s", get_sensor_name().c_str());
          ESP_LOGE("OptolinkSensorBase", "Unknown byte/div_ratio combination for sensor %s", get_sensor_name().c_str());
          break;
      }
      break;
    default:
      optolink_->set_error("Unknown byte value for sensor %s", get_sensor_name().c_str());
      ESP_LOGE("OptolinkSensorBase", "Unknown byte value for sensor %s", get_sensor_name().c_str());
      break;
  }
}

void conv2_100_F::encode(uint8_t *out, DPValue in) {
  int16_t tmp = floor((in.getFloat() * 100) + 0.5);
  out[1] = tmp >> 8;
  out[0] = tmp & 0xFF;
}

DPValue conv2_100_F::decode(const uint8_t *in) {
  int16_t tmp = in[1] << 8 | in[0];
  DPValue out(tmp / 100.0f);
  return out;
}

}  // namespace optolink
}  // namespace esphome

#endif
