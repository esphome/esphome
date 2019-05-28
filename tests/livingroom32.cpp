#include <esphome.h>

using namespace esphome;

void setup() {
  App.set_name("livingroom32");
  App.init_log();

  App.init_wifi("YOUR_SSID", "YOUR_PASSWORD");
  App.init_mqtt("MQTT_HOST", "USERNAME", "PASSWORD");
  App.init_ota()->start_safe_mode();

  // LEDC is only available on ESP32! for the ESP8266, take a look at App.make_esp8266_pwm_output().
  auto *red = App.make_ledc_output(32);  // on pin 32
  auto *green = App.make_ledc_output(33);
  auto *blue = App.make_ledc_output(34);
  App.make_rgb_light("Livingroom Light", red, green, blue);

  App.make_dht_sensor("Livingroom Temperature", "Livingroom Humidity", 12);
  App.make_status_binary_sensor("Livingroom Node Status");
  App.make_restart_switch("Livingroom Restart");

  App.setup();
}

void loop() { App.loop(); }
