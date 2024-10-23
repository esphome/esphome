#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/spi/spi.h"
#include "esphome/components/remote_base/rc_switch_protocol.h"
#include "esphome/components/voltage_sampler/voltage_sampler.h"

namespace esphome {
namespace cc1101 {

class CC1101 : public PollingComponent,
               public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW, spi::CLOCK_PHASE_LEADING,
                                     spi::DATA_RATE_1KHZ> {
 protected:
  InternalGPIOPin *gdo0_;
  voltage_sampler::VoltageSampler *gdo0_adc_;
  int bandwidth_;
  int frequency_;
  sensor::Sensor *rssi_sensor_;
  sensor::Sensor *lqi_sensor_;
  sensor::Sensor *temperature_sensor_;

  uint8_t partnum_;
  uint8_t version_;
  int last_rssi_;
  int last_lqi_;
  float last_temperature_;

  bool reset_();
  void strobe_(uint8_t cmd);
  uint8_t read_register_(uint8_t reg);
  uint8_t read_config_register_(uint8_t reg);
  uint8_t read_status_register_(uint8_t reg);
  void read_register_burst_(uint8_t reg, uint8_t *buffer, size_t length);
  void write_register_(uint8_t reg, uint8_t *value, size_t length);
  void write_register_(uint8_t reg, uint8_t value);
  void write_register_burst_(uint8_t reg, uint8_t *buffer, size_t length);
  // bool send_data_(const uint8_t* data, size_t length);

  // ELECHOUSE_CC1101 stuff

  bool mode_;
  uint8_t modulation_;
  uint8_t frend0_;
  uint8_t chan_;
  int8_t pa_;
  uint8_t last_pa_;
  uint8_t m4rxbw_;
  uint8_t m4dara_;
  uint8_t m2dcoff_;
  uint8_t m2modfm_;
  uint8_t m2manch_;
  uint8_t m2syncm_;
  uint8_t m1fec_;
  uint8_t m1pre_;
  uint8_t m1chsp_;
  uint8_t trxstate_;
  uint8_t clb_[4][2];
  uint8_t pa_table_[8];

  int get_rssi_();
  int get_lqi_();
  float get_temperature_();

  void set_mode_(bool s);
  void set_frequency_(int f);
  void set_modulation_(uint8_t m);
  void set_pa_(int8_t pa);
  void set_clb_(uint8_t b, uint8_t s, uint8_t e);
  void set_rxbw_(int bw);
  void set_state_(uint8_t state);
  bool wait_state_(uint8_t state);

  void split_mdmcfg2_();
  void split_mdmcfg4_();

 public:
  CC1101();

  void set_config_gdo0_pin(InternalGPIOPin *pin);
  void set_config_gdo0_adc_pin(voltage_sampler::VoltageSampler *pin);
  void set_config_bandwidth(int bandwidth);
  void set_config_frequency(int frequency);
  void set_config_rssi_sensor(sensor::Sensor *rssi_sensor);
  void set_config_lqi_sensor(sensor::Sensor *lqi_sensor);
  void set_config_temperature_sensor(sensor::Sensor *temperature_sensor);

  void setup() override;
  void update() override;
  void dump_config() override;

  void begin_tx();
  void end_tx();
};

template<typename... Ts> class BeginTxAction : public Action<Ts...>, public Parented<CC1101> {
 public:
  void play(Ts... x) override { this->parent_->begin_tx(); }
};

template<typename... Ts> class EndTxAction : public Action<Ts...>, public Parented<CC1101> {
 public:
  void play(Ts... x) override { this->parent_->end_tx(); }
};

template<typename... Ts> class CC1101RawAction : public remote_base::RCSwitchRawAction<Ts...>, public Parented<CC1101> {
 protected:
  void play(Ts... x) override {
    this->parent_->begin_tx();
    remote_base::RCSwitchRawAction<Ts...>::play(x...);
    this->parent_->end_tx();
  }

 public:
};

}  // namespace cc1101
}  // namespace esphome
