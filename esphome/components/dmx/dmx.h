

#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
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
