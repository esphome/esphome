#pragma once
#include "esphome/core/component.h"
#include "esphome/components/wifi/wifi_component.h"
#include "esphome/components/wifi_signal/wifi_signal_sensor.h"
#include "esphome/components/uart/uart.h"
#include "midea_frame.h"

namespace esphome {
namespace midea_dongle {

enum MideaApplianceType : uint8_t { DEHUMIDIFIER = 0x00, AIR_CONDITIONER = 0xAC, BROADCAST = 0xFF };
enum MideaMessageType : uint8_t { DEVICE_CONTROL = 0x02, DEVICE_QUERY = 0x03, DEVICE_NETWORK = 0x0D };

struct MideaAppliance {
  /// Calling on update event
  virtual void on_update() = 0;
  /// Calling on frame receive event
  virtual void on_frame(Frame &frame) = 0;
};

class MideaDongle : public PollingComponent, public uart::UARTDevice {
 public:
  MideaDongle() : PollingComponent(1000) {}
  float get_setup_priority() const override { return setup_priority::LATE; }
  void update() override;
  void loop() override;
  void set_appliance(MideaAppliance *app) { this->appliance_ = app; }
  void set_wifi_sensor(wifi_signal::WiFiSignalSensor *wfs) { this->wifi_sensor_ = wfs; }
  void write_frame(const Frame &frame) { this->write_array(frame.data(), frame.size()); }

 protected:
  wifi_signal::WiFiSignalSensor *wifi_sensor_{nullptr};
  MideaAppliance *appliance_;
  NotifyFrame notify_;
  // Buffer
  uint8_t buf_[36];
  // Index
  uint8_t idx_{0};
  // Reverse receive counter
  uint8_t cnt_{2};
  bool need_notify_{false};

  // Reset receiver state
  void reset_() {
    this->idx_ = 0;
    this->cnt_ = 2;
  }
};

}  // namespace midea_dongle
}  // namespace esphome
