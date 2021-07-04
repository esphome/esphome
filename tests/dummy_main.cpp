// Dummy main.cpp file for the PlatformIO project in the git repository.
// Primarily used to get IDE integration working (so the contents here don't
// matter at all, as long as it compiles).
// Not used during runtime nor for CI.

#include <esphome/core/application.h>
#include <esphome/components/logger/logger.h>
#include <esphome/components/wifi/wifi_component.h>
#include <esphome/components/ota/ota_component.h>
#include <esphome/components/gpio/switch/gpio_switch.h>

using namespace esphome;

void setup() {
  App.pre_setup("livingroom", __DATE__ ", " __TIME__, false);
  auto *log = new logger::Logger(115200, 512, logger::UART_SELECTION_UART0);
  log->pre_setup();
  App.register_component(log);

  auto *wifi = new wifi::WiFiComponent();
  App.register_component(wifi);
  wifi::WiFiAP ap;
  ap.set_ssid("Test SSID");
  ap.set_password("password1");
  wifi->add_sta(ap);

  auto *ota = new ota::OTAComponent();
  ota->set_port(8266);

  auto *gpio = new gpio::GPIOSwitch();
  gpio->set_name("GPIO Switch");
  gpio->set_pin(new GPIOPin(8, OUTPUT, false));
  App.register_component(gpio);
  App.register_switch(gpio);

  App.setup();
}

void loop() { App.loop(); }
