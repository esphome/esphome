#ifdef USE_ARDUINO

#include "optolink_sensor_base.h"
#include "optolink.h"

namespace esphome {
namespace optolink {

static const char *const TAG = "optolink.sensor_base";

void OptolinkSensorBase::setup_datapoint() {
  switch (div_ratio_) {
    case 0:
      datapoint_ = new Datapoint<convRaw>(get_component_name().c_str(), "optolink", address_, writeable_);
      datapoint_->setLength(bytes_);
      datapoint_->setCallback([this](const IDatapoint &dp, DPValue dp_value) {
        uint8_t buffer[bytes_];
        dp_value.getRaw(buffer);
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_INFO
        char print_buffer[bytes_ * 2 + 1];
        dp_value.getString(print_buffer, sizeof(print_buffer));
        ESP_LOGI(TAG, "recieved data for datapoint %s: %s", dp.getName(), print_buffer);
#endif
        value_changed((uint8_t *) buffer, bytes_);
      });
      break;
    case 1:
      switch (bytes_) {
        case 1:
          datapoint_ = new Datapoint<conv1_1_US>(get_component_name().c_str(), "optolink", address_, writeable_);
          datapoint_->setCallback([this](const IDatapoint &dp, DPValue dp_value) {
            ESP_LOGI(TAG, "recieved data for datapoint %s: %d", dp.getName(), dp_value.getU8());
            value_changed(dp_value.getU8());
          });
          break;
        case 2:
          datapoint_ = new Datapoint<conv2_1_US>(get_component_name().c_str(), "optolink", address_, writeable_);
          datapoint_->setCallback([this](const IDatapoint &dp, DPValue dp_value) {
            ESP_LOGI(TAG, "recieved data for datapoint %s: %d", dp.getName(), dp_value.getU16());
            value_changed(dp_value.getU16());
          });
          break;
        case 4:
          datapoint_ = new Datapoint<conv4_1_UL>(get_component_name().c_str(), "optolink", address_, writeable_);
          datapoint_->setCallback([this](const IDatapoint &dp, DPValue dp_value) {
            ESP_LOGI(TAG, "recieved data for datapoint %s: %d", dp.getName(), dp_value.getU32());
            value_changed((uint32_t) dp_value.getU32());
          });
          break;
        default:
          unfitting_value_type();
      }
      break;
    case 10:
      switch (bytes_) {
        case 1:
          datapoint_ = new Datapoint<conv1_10_F>(get_component_name().c_str(), "optolink", address_, writeable_);
          datapoint_->setCallback([this](const IDatapoint &dp, DPValue dp_value) {
            ESP_LOGI(TAG, "recieved data for datapoint %s: %f", dp.getName(), dp_value.getFloat());
            value_changed(dp_value.getFloat());
          });
          break;
        case 2:
          datapoint_ = new Datapoint<conv2_10_F>(get_component_name().c_str(), "optolink", address_, writeable_);
          datapoint_->setCallback([this](const IDatapoint &dp, DPValue dp_value) {
            ESP_LOGI(TAG, "recieved data for datapoint %s: %f", dp.getName(), dp_value.getFloat());
            value_changed(dp_value.getFloat());
          });
          break;
        default:
          unfitting_value_type();
      }
      break;
    case 100:
      switch (bytes_) {
        case 2:
          datapoint_ = new Datapoint<conv2_100_F>(get_component_name().c_str(), "optolink", address_, writeable_);
          datapoint_->setCallback([this](const IDatapoint &dp, DPValue dp_value) {
            ESP_LOGI(TAG, "recieved data for datapoint %s: %f", dp.getName(), dp_value.getFloat());
            value_changed(dp_value.getFloat());
          });
          break;
        default:
          unfitting_value_type();
      }
      break;
    case 1000:
      switch (bytes_) {
        case 4:
          datapoint_ = new Datapoint<conv4_1000_F>(get_component_name().c_str(), "optolink", address_, writeable_);
          datapoint_->setCallback([this](const IDatapoint &dp, DPValue dp_value) {
            ESP_LOGI(TAG, "recieved data for datapoint %s: %f", dp.getName(), dp_value.getFloat());
            value_changed(dp_value.getFloat());
          });
          break;
      }
      break;
    case 3600:
      switch (bytes_) {
        case 4:
          datapoint_ = new Datapoint<conv4_3600_F>(get_component_name().c_str(), "optolink", address_, writeable_);
          datapoint_->setCallback([this](const IDatapoint &dp, DPValue dp_value) {
            ESP_LOGI(TAG, "recieved data for datapoint %s: %f", dp.getName(), dp_value.getFloat());
            value_changed(dp_value.getFloat());
          });
          break;
      }
      break;
    default:
      unfitting_value_type();
  }
}

void OptolinkSensorBase::value_changed(float value) {
  ESP_LOGW(TAG, "unused value update by sensor %s", get_component_name().c_str());
}

void OptolinkSensorBase::value_changed(uint8_t value) {
  ESP_LOGW(TAG, "unused value update by sensor %s", get_component_name().c_str());
}

void OptolinkSensorBase::value_changed(uint16_t value) {
  ESP_LOGW(TAG, "unused value update by sensor %s", get_component_name().c_str());
}

void OptolinkSensorBase::value_changed(uint32_t value) {
  ESP_LOGW(TAG, "unused value update by sensor %s", get_component_name().c_str());
}

void OptolinkSensorBase::value_changed(std::string value) {
  ESP_LOGW(TAG, "unused value update by sensor %s", get_component_name().c_str());
}

void OptolinkSensorBase::value_changed(uint8_t *value, size_t length) {
  ESP_LOGW(TAG, "unused value update by sensor %s", get_component_name().c_str());
}

void OptolinkSensorBase::update_datapoint(DPValue dp_value) {
  if (!writeable_) {
    optolink_->set_error("trying to control not writable datapoint %s", get_component_name().c_str());
    ESP_LOGE(TAG, "trying to control not writable datapoint %s", get_component_name().c_str());
  } else if (datapoint_ != nullptr) {
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_INFO
    char buffer[100];
    dp_value.getString(buffer, sizeof(buffer));
    ESP_LOGI(TAG, "updating datapoint %s value: %s", datapoint_->getName(), buffer);
#endif
    optolink_->write_value(datapoint_, dp_value);
  }
}

void OptolinkSensorBase::update_datapoint(float value) {
  if (div_ratio_ > 1) {
    update_datapoint(DPValue(value));
  } else if (div_ratio_ == 1) {
    switch (bytes_) {
      case 1:
        update_datapoint(DPValue((uint8_t) value));
        break;
      case 2:
        update_datapoint(DPValue((uint16_t) value));
        break;
      case 4:
        update_datapoint(DPValue((uint32_t) value));
        break;
      default:
        unfitting_value_type();
        break;
    }
  } else {
    unfitting_value_type();
  }
}

void OptolinkSensorBase::update_datapoint(uint8_t value) {
  if (bytes_ == 1 && div_ratio_ == 1) {
    update_datapoint(DPValue(value));
  } else {
    unfitting_value_type();
  }
}

void OptolinkSensorBase::update_datapoint(uint16_t value) {
  if (bytes_ == 2 && div_ratio_ == 1) {
    update_datapoint(DPValue(value));
  } else {
    unfitting_value_type();
  }
}

void OptolinkSensorBase::update_datapoint(uint32_t value) {
  if (bytes_ == 4 && div_ratio_ == 1) {
    update_datapoint(DPValue(value));
  } else {
    unfitting_value_type();
  }
}

void OptolinkSensorBase::update_datapoint(uint8_t *value, size_t length) {
  if (bytes_ == length && div_ratio_ == 0) {
    update_datapoint(DPValue(value, length));
  } else {
    unfitting_value_type();
  }
}

void OptolinkSensorBase::unfitting_value_type() {
  optolink_->set_error("Unfitting byte/div_ratio combination for sensor/component %s", get_component_name().c_str());
  ESP_LOGE(TAG, "Unfitting byte/div_ratio combination for sensor/component %s", get_component_name().c_str());
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

void conv4_1000_F::encode(uint8_t *out, DPValue in) {
  int32_t tmp = floor((in.getFloat() * 1000) + 0.5);
  out[3] = tmp >> 24;
  out[2] = tmp >> 16;
  out[1] = tmp >> 8;
  out[0] = tmp & 0xFF;
}

DPValue conv4_1000_F::decode(const uint8_t *in) {
  int32_t tmp = in[3] << 24 | in[2] << 16 | in[1] << 8 | in[0];
  DPValue out(tmp / 1000.0f);
  return out;
}

}  // namespace optolink
}  // namespace esphome

#endif
