#pragma once
#include "esphome/core/component.h"

namespace esphome {
namespace dfu {
class DeviceFirmwareUpdate : public Component {
	void loop() override;
};
}
}
