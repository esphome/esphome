#pragma once
#include "esphome.h"

class CustomSensor : public Component, public Sensor {
 public:
  void loop() override {
    // Test id() helper
    float value = id(my_sensor).state;
    id(my_global_string) = "Hello World";
    publish_state(42.0);
  }
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

class CustomNativeAPI : public Component, public CustomAPIDevice {
 public:
  void setup() override {
    register_service(&CustomNativeAPI::on_hello_world, "hello_world");
    register_service(&CustomNativeAPI::on_start_dryer, "start_dryer", {"value"});
    register_service(&CustomNativeAPI::on_many_values, "many_values", {"bool", "int", "float", "str1", "str2"});
    subscribe_homeassistant_state(&CustomNativeAPI::on_light_state, "light.my_light");
    subscribe_homeassistant_state(&CustomNativeAPI::on_brightness_state, "light.my_light", "brightness");
  }

  void on_hello_world() { ESP_LOGD("custom_api", "Hello World from native API service!"); }
  void on_start_dryer(int value) { ESP_LOGD("custom_api", "Value is %d", value); }
  void on_many_values(bool a_bool, int a_int, float a_float, std::string str1, std::string str2) {
    ESP_LOGD("custom_api", "Bool: %s", ONOFF(a_bool));
    ESP_LOGD("custom_api", "Int: %d", a_int);
    ESP_LOGD("custom_api", "Float: %f", a_float);
    ESP_LOGD("custom_api", "String1: %s", str1.c_str());
    ESP_LOGD("custom_api", "String2: %s", str2.c_str());
    ESP_LOGD("custom_api", "Connected: %s", YESNO(is_connected()));

    call_homeassistant_service("light.turn_on", {
                                                    {"entity_id", "light.my_light"},
                                                    {"brightness", "127"},
                                                });
    // no args
    call_homeassistant_service("homeassistant.restart");
  }
  void on_light_state(std::string state) { ESP_LOGD("custom_api", "Got state %s", state.c_str()); }
  void on_brightness_state(std::string state) { ESP_LOGD("custom_api", "Got attribute state %s", state.c_str()); }
};
