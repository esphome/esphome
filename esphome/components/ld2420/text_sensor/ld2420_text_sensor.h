#pragma once

#include "../ld2420.h"
#include "esphome/components/text_sensor/text_sensor.h"

namespace esphome::ld2420 {

class LD2420TextSensor final : public LD2420Listener, public text_sensor::TextSensor {
 public:
  SUB_TEXT_SENSOR(fw_version)
  void on_fw_version(const char *fw) override { PUBLISH_STATE(this->fw_version_text_sensor_, fw); }
};

}  // namespace esphome::ld2420
