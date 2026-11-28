#include "esphome/components/thermopro_tp970/thermopro_tp970.h"

#include <cassert>
#include <cmath>

namespace esphome::thermopro_tp970::testing {

class TestProbe : public ThermoProTP970 {
 public:
  using ThermoProTP970::handle_;
  using ThermoProTP970::streaming_;
};

void run() {
  for (auto event : {ESP_GATTC_DISCONNECT_EVT, ESP_GATTC_CLOSE_EVT}) {
    TestProbe probe;
    sensor::Sensor internal, ambient, battery, count;
    probe.set_internal_temperature_sensor(&internal);
    probe.set_ambient_temperature_sensor(&ambient);
    probe.set_battery_level_sensor(&battery);
    probe.set_notification_count_sensor(&count);
    now_ms = 100;
    probe.setup();
    assert(std::isnan(internal.state));
    assert(!probe.streaming_);
    uint8_t bytes[]{0x10, 0x00, 0x36, 0x00, 0x2D, 0x0A, 0x36, 0x00};
    esp_ble_gattc_cb_param_t param{};
    auto notify = [&]() {
      probe.gattc_event_handler(ESP_GATTC_SEARCH_CMPL_EVT, 0, &param);
      param.notify = {7, 1, bytes, sizeof(bytes)};
      probe.gattc_event_handler(ESP_GATTC_NOTIFY_EVT, 0, &param);
      assert(internal.state == 24);
      assert(ambient.state == 24);
      assert(probe.streaming_);
      assert(!probe.warning);
    };
    notify();
    assert(count.state == 1);
    probe.gattc_event_handler(event, 0, &param);
    assert(probe.handle_ == 0);
    assert(std::isnan(internal.state));
    assert(std::isnan(ambient.state));
    assert(std::isnan(battery.state));
    assert(!probe.streaming_);
    assert(probe.warning);
    notify();
    assert(count.state == 2);
    now_ms += 14999;
    probe.interval();
    assert(probe.streaming_);
    now_ms++;
    probe.interval();
    assert(!probe.streaming_);
    assert(std::isnan(internal.state));
    assert(std::isnan(ambient.state));
    assert(std::isnan(battery.state));
    notify();
    assert(count.state == 3);
  }
}

}  // namespace esphome::thermopro_tp970::testing

int main() { esphome::thermopro_tp970::testing::run(); }
