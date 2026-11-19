#pragma once

#include <cmath>
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/components/light/light_output.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/number/number.h"

#if __has_include(<esp_dmx.h>)
#include <esp_dmx.h>
#include "driver/uart.h"  // uart_is_driver_installed
#else
using dmx_port_t = int;

struct DmxConfigT {};
using dmx_config_t = DmxConfigT;

struct DmxPacketT {
  int err;
  int sc;
  size_t size;
  bool is_rdm;
};
using dmx_packet_t = DmxPacketT;

struct DmxPersonalityT {
  int id;
  const char *name;
};
using dmx_personality_t = DmxPersonalityT;

static constexpr size_t DMX_PACKET_SIZE = 513;
static constexpr int DMX_OK = 0;
static constexpr dmx_config_t DMX_CONFIG_DEFAULT{};

inline bool dmx_driver_install(dmx_port_t, const dmx_config_t *, const dmx_personality_t *, int) { return true; }
inline bool dmx_driver_delete(dmx_port_t) { return true; }
inline bool dmx_set_pin(dmx_port_t, int, int, int) { return true; }
inline size_t dmx_receive(dmx_port_t, dmx_packet_t *, int) { return 0; }
inline size_t dmx_read(dmx_port_t, void *, size_t) { return 0; }
inline size_t dmx_write(dmx_port_t, const void *, size_t) { return 0; }
inline size_t dmx_send_num(dmx_port_t, size_t) { return 0; }
inline bool uart_is_driver_installed(dmx_port_t) { return false; }
#endif

#ifndef DMX_STATIC_SENSOR_COUNT
static constexpr size_t DMX_STATIC_SENSOR_COUNT = 0;
#endif
#ifndef DMX_DYNAMIC_SENSOR_COUNT
static constexpr size_t DMX_DYNAMIC_SENSOR_COUNT = 0;
#endif

namespace esphome::dmx {

enum class DMXMode : uint8_t {
  RX = 0,
  TX = 1,
  BIDIRECTIONAL = 2,
};

struct StaticChannelSensor {
  uint16_t channel;
  sensor::Sensor *sensor;
};

struct DynamicChannelSensor {
  number::Number *channel_number;
  sensor::Sensor *sensor;
};

class DMXComponent : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_tx_pin(int pin) { this->tx_pin_ = pin; }
  void set_rx_pin(int pin) { this->rx_pin_ = pin; }
  void set_rts_pin(int pin) { this->rts_pin_ = pin; }
  void set_uart_num(int num) { this->uart_num_ = num; }
  void set_mode(DMXMode mode) { this->mode_ = mode; }
  void set_tx_interval(uint32_t ms) { this->tx_interval_ms_ = ms; }
  void set_rx_guard(uint32_t ms) { this->rx_guard_ms_ = ms; }

  // Received and transmitted universes are deliberately kept separate.
  uint8_t get_rx_channel(uint16_t channel) const;
  uint8_t get_tx_channel(uint16_t channel) const;

  // Backwards-compatible alias: historically get_channel() returned the
  // shared universe buffer, which was primarily used for received values.
  uint8_t get_channel(uint16_t channel) const { return this->get_rx_channel(channel); }

  void set_slot(uint16_t channel, uint8_t value);
  void set_slots(uint16_t first_channel, const uint8_t *values, size_t count);
  void enable_tx() {
    this->tx_enabled_ = true;
    this->tx_dirty_ = true;
  }

  SLOT_LIST_REQUIRES(DMX_STATIC_SENSOR_COUNT)
  void register_channel_sensor(uint16_t channel, sensor::Sensor *sens) {
    this->static_sensors_.push_back({channel, sens});
  }

  SLOT_LIST_REQUIRES(DMX_DYNAMIC_SENSOR_COUNT)
  void register_dynamic_channel_sensor(number::Number *channel_number, sensor::Sensor *sens) {
    this->dynamic_sensors_.push_back({channel_number, sens});
  }

 protected:
  void publish_inputs_();
  void send_universe_();

  int tx_pin_{-1};
  int rx_pin_{-1};
  int rts_pin_{-1};
  int uart_num_{2};

  dmx_port_t dmx_port_{};

  // Each buffer includes the DMX start code at index 0. Channel 1 is index 1.
  uint8_t rx_data_[DMX_PACKET_SIZE]{};
  uint8_t tx_data_[DMX_PACKET_SIZE]{};

  bool tx_enabled_{false};
  bool tx_dirty_{false};
  uint32_t last_tx_ms_{0};
  uint32_t last_rx_ms_{0};
  DMXMode mode_{DMXMode::RX};
  uint32_t tx_interval_ms_{25};
  uint32_t rx_guard_ms_{5};

  [[no_unique_address]] SlotList<StaticChannelSensor, DMX_STATIC_SENSOR_COUNT> static_sensors_;
  [[no_unique_address]] SlotList<DynamicChannelSensor, DMX_DYNAMIC_SENSOR_COUNT> dynamic_sensors_;
};

class DMXOutputNumber : public number::Number, public Component {
 public:
  void setup() override {}
  void dump_config() override;
  void set_parent(DMXComponent *parent) { this->parent_ = parent; }
  void set_channel(uint16_t channel) { this->channel_ = channel; }
  float get_setup_priority() const override { return setup_priority::DATA; }

 protected:
  void control(float value) override;

  DMXComponent *parent_{nullptr};
  uint16_t channel_{1};
};

enum class DMXLightType : uint8_t {
  DIMMER,
  RGB,
  RGBW,
};

class DMXLightOutput : public light::LightOutput {
 public:
  void set_parent(DMXComponent *parent) { this->parent_ = parent; }
  void set_first_channel(uint16_t channel) { this->first_channel_ = channel; }
  void set_light_type(DMXLightType type) { this->light_type_ = type; }

  light::LightTraits get_traits() override;
  void write_state(light::LightState *state) override;

 protected:
  DMXComponent *parent_{nullptr};
  uint16_t first_channel_{1};
  DMXLightType light_type_{DMXLightType::DIMMER};
};
}  // namespace esphome::dmx
