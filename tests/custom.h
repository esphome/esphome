#pragma once
#include "esphome.h"

class CustomSensor : public Component, public Sensor {
 public:
  void loop() override { publish_state(42.0); }
};

class CustomTextSensor : public Component, public TextSensor {
 public:
  void loop() override { publish_state("Hello World"); }
};

class CustomBinarySensor : public Component, public BinarySensor {
 public:
  void loop() override { publish_state(false); }
};

class CustomSwitch : public Switch {
 protected:
  void write_state(bool state) override { ESP_LOGD("custom_switch", "Setting %s", ONOFF(state)); }
};

class CustomComponent : public PollingComponent {
 public:
  void setup() override { ESP_LOGD("custom_component", "Setup"); }
  void update() override { ESP_LOGD("custom_component", "Update"); }
};

class CustomBinaryOutput : public BinaryOutput, public Component {
 protected:
  void write_state(bool state) override { ESP_LOGD("custom_output", "Setting %s", ONOFF(state)); }
};

class CustomFloatOutput : public FloatOutput, public Component {
 protected:
  void write_state(float state) override { ESP_LOGD("custom_output", "Setting %f", state); }
};
