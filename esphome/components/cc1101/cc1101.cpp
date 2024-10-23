/*
  https://github.com/gabest11/esphome-cc1101

  This is a CC1101 transceiver component that works with esphome's remote_transmitter/remote_receiver.

  It can be compiled with Arduino and esp-idf framework and should support any esphome compatible board through the SPI
  Bus.

  On ESP8266, you can use a single pin instead of GD0O and GD02 (gdo0 is an optional parameter). If assigned, the pin
  direction will be reversed for the transfers.

  On ESP32, this will not work, you must connect two separate pins. TX to GDO0, RX to GDO2. If only TX works, they are
  probably switched.

  Transfers, except transmit_rc_switch_raw_cc1101, must be surrounded with cc1101.begin_tx and cc1101.end_tx.

  The source code is a mashup of the following github projects with some special esphome sauce:

  https://github.com/dbuezas/esphome-cc1101 (the original esphome component)
  https://github.com/nistvan86/esphome-q7rf (how to use esphome with spi)
  https://github.com/LSatan/SmartRC-CC1101-Driver-Lib (cc1101 setup code)

  TODO: RP2040? (USE_RP2040)
  TODO: Libretiny? (USE_LIBRETINY)
*/

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "cc1101.h"
#include "cc1101defs.h"
#include <climits>

#ifdef USE_ARDUINO
#include <Arduino.h>
#else  // USE_ESP_IDF
#include <driver/gpio.h>
int32_t map(int32_t x, int32_t in_min, int32_t in_max, int32_t out_min, int32_t out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}
#endif
int32_t mapfloat(float x, float in_min, float in_max, float out_min, float out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

namespace esphome {
namespace cc1101 {

static const char *const TAG = "cc1101";

// 300 - 348, 387 - 464              -30   -20   -15   -10     0     5     7    10
static const uint8_t PA_TABLE_315[8]{0x12, 0x0D, 0x1C, 0x34, 0x51, 0x85, 0xCB, 0xC2};
static const uint8_t PA_TABLE_433[8]{0x12, 0x0E, 0x1D, 0x34, 0x60, 0x84, 0xC8, 0xC0};
// 779 - 899.99                       -30   -20   -15   -10    -6     0     5     7    10    12
static const uint8_t PA_TABLE_868[10]{0x03, 0x17, 0x1D, 0x26, 0x37, 0x50, 0x86, 0xCD, 0xC5, 0xC0};
// 900 - 928                          -30   -20   -15   -10    -6     0     5     7    10    11
static const uint8_t PA_TABLE_915[10]{0x03, 0x0E, 0x1E, 0x27, 0x38, 0x8E, 0x84, 0xCC, 0xC3, 0xC0};

CC1101::CC1101() {
  this->gdo0_ = nullptr;
  this->gdo0_adc_ = nullptr;
  this->bandwidth_ = 200;
  this->frequency_ = 433920;
  this->rssi_sensor_ = nullptr;
  this->lqi_sensor_ = nullptr;
  this->temperature_sensor_ = nullptr;

  this->partnum_ = 0;
  this->version_ = 0;
  this->last_rssi_ = INT_MIN;
  this->last_lqi_ = INT_MIN;
  this->last_temperature_ = NAN;

  this->mode_ = false;
  this->modulation_ = 2;
  this->chan_ = 0;
  this->pa_ = 12;
  this->last_pa_ = -1;
  this->m4rxbw_ = 0;
  this->trxstate_ = 0;

  this->clb_[0][0] = 24;
  this->clb_[0][1] = 28;
  this->clb_[1][0] = 31;
  this->clb_[1][1] = 38;
  this->clb_[2][0] = 65;
  this->clb_[2][1] = 76;
  this->clb_[3][0] = 77;
  this->clb_[3][1] = 79;

  memset(this->pa_table_, 0, sizeof(pa_table_));
  this->pa_table_[1] = 0xc0;
}

void CC1101::set_config_gdo0_pin(InternalGPIOPin *pin) { gdo0_ = pin; }

void CC1101::set_config_gdo0_adc_pin(voltage_sampler::VoltageSampler *pin) { gdo0_adc_ = pin; }

void CC1101::set_config_bandwidth(int bandwidth) { bandwidth_ = bandwidth; }

void CC1101::set_config_frequency(int frequency) { frequency_ = frequency; }

void CC1101::set_config_rssi_sensor(sensor::Sensor *rssi_sensor) { rssi_sensor_ = rssi_sensor; }

void CC1101::set_config_lqi_sensor(sensor::Sensor *lqi_sensor) { lqi_sensor_ = lqi_sensor; }

void CC1101::set_config_temperature_sensor(sensor::Sensor *temperature_sensor) {
  temperature_sensor_ = temperature_sensor;
}

void CC1101::setup() {
  if (this->gdo0_ != nullptr) {
#ifdef USE_ESP8266
    // ESP8266 GDO0 generally input, switched to output for TX
    // ESP32 GDO0 output, GDO2 input
    // if there is an ADC, GDO0 is input only while reading temperature
    this->gdo0_->setup();
    this->gdo0_->pin_mode(gpio::FLAG_INPUT);
#endif
  }

  this->spi_setup();

  if (!this->reset_()) {
    mark_failed();
    ESP_LOGE(TAG, "Failed to reset CC1101 modem. Check connection.");
    return;
  }

  // ELECHOUSE_cc1101.Init();

  this->write_register_(CC1101_FSCTRL1, 0x06);

  this->set_mode_(false);
  this->set_frequency_(this->frequency_);

  this->write_register_(CC1101_MDMCFG1, 0x02);
  this->write_register_(CC1101_MDMCFG0, 0xF8);
  this->write_register_(CC1101_CHANNR, this->chan_);
  this->write_register_(CC1101_DEVIATN, 0x47);
  this->write_register_(CC1101_FREND1, 0x56);
  this->write_register_(CC1101_MCSM0, 0x18);
  this->write_register_(CC1101_FOCCFG, 0x16);
  this->write_register_(CC1101_BSCFG, 0x1C);
  this->write_register_(CC1101_AGCCTRL2, 0xC7);
  this->write_register_(CC1101_AGCCTRL1, 0x00);
  this->write_register_(CC1101_AGCCTRL0, 0xB2);
  this->write_register_(CC1101_FSCAL3, 0xE9);
  this->write_register_(CC1101_FSCAL2, 0x2A);
  this->write_register_(CC1101_FSCAL1, 0x00);
  this->write_register_(CC1101_FSCAL0, 0x1F);
  this->write_register_(CC1101_FSTEST, 0x59);
  this->write_register_(CC1101_TEST2, 0x81);
  this->write_register_(CC1101_TEST1, 0x35);
  this->write_register_(CC1101_TEST0, 0x09);
  this->write_register_(CC1101_PKTCTRL1, 0x04);
  this->write_register_(CC1101_ADDR, 0x00);
  this->write_register_(CC1101_PKTLEN, 0x00);

  // ELECHOUSE_cc1101.setRxBW_(_bandwidth);

  this->set_rxbw_(this->bandwidth_);

  // ELECHOUSE_cc1101.setMHZ(_freq);

  this->set_frequency_(this->frequency_);  // TODO: already set

  //

  this->set_state_(CC1101_SRX);

  //

  ESP_LOGI(TAG, "CC1101 initialized.");
}

void CC1101::update() {
  if (this->rssi_sensor_ != nullptr) {
    int rssi = this->get_rssi_();
    ESP_LOGV(TAG, "rssi = %d", rssi);
    if (rssi != this->last_rssi_) {
      this->rssi_sensor_->publish_state(rssi);
      this->last_rssi_ = rssi;
    }
  }

  if (this->lqi_sensor_ != nullptr) {
    int lqi = this->get_lqi_() & 0x7f;  // msb = CRC ok or not set
    ESP_LOGV(TAG, "lqi = %d", lqi);
    if (lqi != this->last_lqi_) {
      this->lqi_sensor_->publish_state(lqi);
      this->last_lqi_ = lqi;
    }
  }

  if (this->temperature_sensor_ != nullptr && this->gdo0_ != nullptr && this->gdo0_adc_ != nullptr) {
    float temperature = this->get_temperature_();
    ESP_LOGV(TAG, "temperature = %.2f", temperature);
    if (temperature != NAN && temperature != this->last_temperature_) {
      this->temperature_sensor_->publish_state(temperature);
      this->last_temperature_ = temperature;
    }
  }
}

void CC1101::dump_config() {
  ESP_LOGCONFIG(TAG, "CC1101 partnum %02X version %02X:", this->partnum_, this->version_);
  LOG_PIN("  CC1101 CS Pin: ", this->cs_);
  LOG_PIN("  CC1101 GDO0: ", this->gdo0_);
  ESP_LOGCONFIG(TAG, "  CC1101 Bandwith: %d KHz", this->bandwidth_);
  ESP_LOGCONFIG(TAG, "  CC1101 Frequency: %d KHz", this->frequency_);
  LOG_SENSOR("  ", "RSSI", this->rssi_sensor_);
  LOG_SENSOR("  ", "LQI", this->lqi_sensor_);
  LOG_SENSOR("  ", "Temperature sensor", this->temperature_sensor_);
}

bool CC1101::reset_() {
  ESP_LOGD(TAG, "Issued CC1101 reset sequence.");

  this->set_state_(CC1101_SRES);

  // Read part number and version

  this->partnum_ = this->read_status_register_(CC1101_PARTNUM);
  this->version_ = this->read_status_register_(CC1101_VERSION);

  ESP_LOGI(TAG, "CC1101 found with partnum: %02X and version: %02X", this->partnum_, this->version_);

  return this->version_ > 0;
}

void CC1101::strobe_(uint8_t cmd) {
  this->enable();
  this->write_byte(cmd);
  this->disable();
}

uint8_t CC1101::read_register_(uint8_t reg) {
  this->enable();
  this->write_byte(reg);
  uint8_t value = this->transfer_byte(0);
  this->disable();
  return value;
}

uint8_t CC1101::read_config_register_(uint8_t reg) { return this->read_register_(reg | CC1101_READ_SINGLE); }

uint8_t CC1101::read_status_register_(uint8_t reg) { return this->read_register_(reg | CC1101_READ_BURST); }

void CC1101::read_register_burst_(uint8_t reg, uint8_t *buffer, size_t length) {
  this->enable();
  this->write_byte(reg | CC1101_READ_BURST);
  this->read_array(buffer, length);
  this->disable();
}
void CC1101::write_register_(uint8_t reg, uint8_t *value, size_t length) {
  this->enable();
  this->write_byte(reg);
  this->transfer_array(value, length);
  this->disable();
}

void CC1101::write_register_(uint8_t reg, uint8_t value) {
  uint8_t arr[1] = {value};
  this->write_register_(reg, arr, 1);
}

void CC1101::write_register_burst_(uint8_t reg, uint8_t *buffer, size_t length) {
  this->write_register_(reg | CC1101_WRITE_BURST, buffer, length);
}
/*
bool CC1101::send_data_(const uint8_t* data, size_t length)
{
  uint8_t buffer[length];

  memcpy(buffer, data, lenght);

  this->send_cmd_(CC1101_SIDLE);
  this->send_cmd_(CC1101_SFRX);
  this->send_cmd_(CC1101_SFTX);

  this->write_register_burst_(CC1101_TXFIFO, buffer, length);

  this->strobe_(CC1101_STX);

  return this->wait_state(CC1101_STX);
}
*/

// ELECHOUSE_CC1101 stuff

int CC1101::get_rssi_() {
  int rssi;
  rssi = this->read_status_register_(CC1101_RSSI);
  if (rssi >= 128)
    rssi -= 256;
  return (rssi / 2) - 74;
}

int CC1101::get_lqi_() { return this->read_status_register_(CC1101_LQI); }

float CC1101::get_temperature_() {
  if (this->gdo0_ == nullptr || this->gdo0_adc_ == nullptr) {
    ESP_LOGE(TAG, "cannot read temperature if GDO0_ADC is not set");
    return NAN;
  }

  uint8_t trxstate = this->trxstate_;

#ifndef USE_ESP8266
  this->gdo0_->pin_mode(gpio::FLAG_INPUT);
#endif

  // datasheet 11.2, 26

  this->set_state_(CC1101_SIDLE);

  this->write_register_(CC1101_IOCFG0, 0x80);
  this->write_register_(CC1101_PTEST, 0xBF);

  float voltage = 0.0f;
  int successful_samples = 0;

  for (uint8_t i = 0, num_samples = 3; i < num_samples; i++) {
    float voltage_reading = this->gdo0_adc_->sample();
    ESP_LOGV(TAG, "ADC voltage_reading = %f", voltage_reading);
    if (std::isfinite(voltage_reading) && voltage_reading > 0.6 && voltage_reading < 1.0) {
      voltage += voltage_reading;
      successful_samples++;
    }
  }

  this->write_register_(CC1101_PTEST, 0x7F);

  if (this->mode_) {
    this->write_register_(CC1101_IOCFG0, 0x06);
  } else {
    this->write_register_(CC1101_IOCFG0, 0x0D);
  }

#ifndef USE_ESP8266
  this->gdo0_->pin_mode(gpio::FLAG_OUTPUT);
#endif

  switch (trxstate) {
    case CC1101_STX:
      this->set_state_(CC1101_STX);
      break;
    case CC1101_SRX:
      this->set_state_(CC1101_SRX);
      break;
    default:
      this->set_state_(CC1101_SIDLE);
      break;
  }

  if (successful_samples == 0) {
    return NAN;
  }

  float v = voltage * 1000 / static_cast<float>(successful_samples);

  if (v >= 651 && v < 747) {
    return mapfloat(v, 651, 747, -40, 0);
  } else if (v >= 747 && v < 847) {
    return mapfloat(v, 747, 847, 0, 40);
  } else if (v >= 847 && v < 945) {
    return mapfloat(v, 847, 945, 40, 80);
  }

  return NAN;
}

void CC1101::set_mode_(bool s) {
  this->mode_ = s;

  if (s) {
    this->write_register_(CC1101_IOCFG2, 0x0B);
    this->write_register_(CC1101_IOCFG0, 0x06);
    this->write_register_(CC1101_PKTCTRL0, 0x05);
    this->write_register_(CC1101_MDMCFG3, 0xF8);
    this->write_register_(CC1101_MDMCFG4, 11 + this->m4rxbw_);
  } else {
    this->write_register_(CC1101_IOCFG2, 0x0D);
    this->write_register_(CC1101_IOCFG0, 0x0D);
    this->write_register_(CC1101_PKTCTRL0, 0x32);
    this->write_register_(CC1101_MDMCFG3, 0x93);
    this->write_register_(CC1101_MDMCFG4, 7 + this->m4rxbw_);
  }

  this->set_modulation_(this->modulation_);
}

void CC1101::set_modulation_(uint8_t m) {
  if (m > 4)
    m = 4;

  this->modulation_ = m;

  this->split_mdmcfg2_();

  switch (m) {
    case 0:
      this->m2modfm_ = 0x00;
      this->frend0_ = 0x10;
      break;  // 2-FSK
    case 1:
      this->m2modfm_ = 0x10;
      this->frend0_ = 0x10;
      break;  // GFSK
    case 2:
      this->m2modfm_ = 0x30;
      this->frend0_ = 0x11;
      break;  // ASK
    case 3:
      this->m2modfm_ = 0x40;
      this->frend0_ = 0x10;
      break;  // 4-FSK
    case 4:
      this->m2modfm_ = 0x70;
      this->frend0_ = 0x10;
      break;  // MSK
  }

  this->write_register_(CC1101_MDMCFG2, this->m2dcoff_ + this->m2modfm_ + this->m2manch_ + this->m2syncm_);
  this->write_register_(CC1101_FREND0, this->frend0_);

  this->set_pa_(this->pa_);
}

void CC1101::set_pa_(int8_t pa) {
  this->pa_ = pa;

  int a;

  if (this->frequency_ >= 300000 && this->frequency_ <= 348000) {
    if (pa <= -30) {
      a = PA_TABLE_315[0];
    } else if (pa > -30 && pa <= -20) {
      a = PA_TABLE_315[1];
    } else if (pa > -20 && pa <= -15) {
      a = PA_TABLE_315[2];
    } else if (pa > -15 && pa <= -10) {
      a = PA_TABLE_315[3];
    } else if (pa > -10 && pa <= 0) {
      a = PA_TABLE_315[4];
    } else if (pa > 0 && pa <= 5) {
      a = PA_TABLE_315[5];
    } else if (pa > 5 && pa <= 7) {
      a = PA_TABLE_315[6];
    } else {
      a = PA_TABLE_315[7];
    }
    this->last_pa_ = 1;
  } else if (this->frequency_ >= 378000 && this->frequency_ <= 464000) {
    if (pa <= -30) {
      a = PA_TABLE_433[0];
    } else if (pa > -30 && pa <= -20) {
      a = PA_TABLE_433[1];
    } else if (pa > -20 && pa <= -15) {
      a = PA_TABLE_433[2];
    } else if (pa > -15 && pa <= -10) {
      a = PA_TABLE_433[3];
    } else if (pa > -10 && pa <= 0) {
      a = PA_TABLE_433[4];
    } else if (pa > 0 && pa <= 5) {
      a = PA_TABLE_433[5];
    } else if (pa > 5 && pa <= 7) {
      a = PA_TABLE_433[6];
    } else {
      a = PA_TABLE_433[7];
    }
    this->last_pa_ = 2;
  } else if (this->frequency_ >= 779000 && this->frequency_ < 900000) {
    if (pa <= -30) {
      a = PA_TABLE_868[0];
    } else if (pa > -30 && pa <= -20) {
      a = PA_TABLE_868[1];
    } else if (pa > -20 && pa <= -15) {
      a = PA_TABLE_868[2];
    } else if (pa > -15 && pa <= -10) {
      a = PA_TABLE_868[3];
    } else if (pa > -10 && pa <= -6) {
      a = PA_TABLE_868[4];
    } else if (pa > -6 && pa <= 0) {
      a = PA_TABLE_868[5];
    } else if (pa > 0 && pa <= 5) {
      a = PA_TABLE_868[6];
    } else if (pa > 5 && pa <= 7) {
      a = PA_TABLE_868[7];
    } else if (pa > 7 && pa <= 10) {
      a = PA_TABLE_868[8];
    } else {
      a = PA_TABLE_868[9];
    }
    this->last_pa_ = 3;
  } else if (this->frequency_ >= 900000 && this->frequency_ <= 928000) {
    if (pa <= -30) {
      a = PA_TABLE_915[0];
    } else if (pa > -30 && pa <= -20) {
      a = PA_TABLE_915[1];
    } else if (pa > -20 && pa <= -15) {
      a = PA_TABLE_915[2];
    } else if (pa > -15 && pa <= -10) {
      a = PA_TABLE_915[3];
    } else if (pa > -10 && pa <= -6) {
      a = PA_TABLE_915[4];
    } else if (pa > -6 && pa <= 0) {
      a = PA_TABLE_915[5];
    } else if (pa > 0 && pa <= 5) {
      a = PA_TABLE_915[6];
    } else if (pa > 5 && pa <= 7) {
      a = PA_TABLE_915[7];
    } else if (pa > 7 && pa <= 10) {
      a = PA_TABLE_915[8];
    } else {
      a = PA_TABLE_915[9];
    }
    this->last_pa_ = 4;
  } else {
    ESP_LOGE(TAG, "CC1101 set_pa(%d) frequency out of range: %d", pa, this->frequency_);
    return;
  }

  if (this->modulation_ == 2) {
    this->pa_table_[0] = 0;
    this->pa_table_[1] = a;
  } else {
    this->pa_table_[0] = a;
    this->pa_table_[1] = 0;
  }

  this->write_register_burst_(CC1101_PATABLE, this->pa_table_, sizeof(this->pa_table_));
}

void CC1101::set_frequency_(int f) {
  this->frequency_ = f;

  uint8_t freq2 = 0;
  uint8_t freq1 = 0;
  uint8_t freq0 = 0;

  float mhz = (float) f / 1000;

  while (true) {
    if (mhz >= 26) {
      mhz -= 26;
      freq2++;
    } else if (mhz >= 0.1015625) {
      mhz -= 0.1015625;
      freq1++;
    } else if (mhz >= 0.00039675) {
      mhz -= 0.00039675;
      freq0++;
    } else
      break;
  }

  /*
  // TODO: impossible, freq0 being uint8_t, also 0.1015625/0.00039675 = 255.9861373660996, it would never reach 256
  if(freq0 > 255)
  {
    freq1 += 1;
    freq0 -= 256;
  }
  */

  this->write_register_(CC1101_FREQ2, freq2);
  this->write_register_(CC1101_FREQ1, freq1);
  this->write_register_(CC1101_FREQ0, freq0);

  // calibrate

  mhz = (float) f / 1000;

  if (mhz >= 300 && mhz <= 348) {
    this->write_register_(CC1101_FSCTRL0, map(mhz, 300, 348, this->clb_[0][0], this->clb_[0][1]));

    if (mhz < 322.88) {
      this->write_register_(CC1101_TEST0, 0x0B);
    } else {
      this->write_register_(CC1101_TEST0, 0x09);

      uint8_t s = this->read_status_register_(CC1101_FSCAL2);

      if (s < 32) {
        this->write_register_(CC1101_FSCAL2, s + 32);
      }

      if (this->last_pa_ != 1)
        this->set_pa_(this->pa_);
    }
  } else if (mhz >= 378 && mhz <= 464) {
    this->write_register_(CC1101_FSCTRL0, map(mhz, 378, 464, this->clb_[1][0], this->clb_[1][1]));

    if (mhz < 430.5) {
      this->write_register_(CC1101_TEST0, 0x0B);
    } else {
      this->write_register_(CC1101_TEST0, 0x09);

      uint8_t s = this->read_status_register_(CC1101_FSCAL2);

      if (s < 32) {
        this->write_register_(CC1101_FSCAL2, s + 32);
      }

      if (this->last_pa_ != 2)
        this->set_pa_(this->pa_);
    }
  } else if (mhz >= 779 && mhz <= 899.99) {
    this->write_register_(CC1101_FSCTRL0, map(mhz, 779, 899, this->clb_[2][0], this->clb_[2][1]));

    if (mhz < 861) {
      this->write_register_(CC1101_TEST0, 0x0B);
    } else {
      this->write_register_(CC1101_TEST0, 0x09);

      uint8_t s = this->read_status_register_(CC1101_FSCAL2);

      if (s < 32) {
        this->write_register_(CC1101_FSCAL2, s + 32);
      }

      if (this->last_pa_ != 3)
        this->set_pa_(this->pa_);
    }
  } else if (mhz >= 900 && mhz <= 928) {
    this->write_register_(CC1101_FSCTRL0, map(mhz, 900, 928, this->clb_[3][0], this->clb_[3][1]));
    this->write_register_(CC1101_TEST0, 0x09);

    uint8_t s = this->read_status_register_(CC1101_FSCAL2);

    if (s < 32) {
      this->write_register_(CC1101_FSCAL2, s + 32);
    }

    if (this->last_pa_ != 4)
      this->set_pa_(this->pa_);
  }
}

void CC1101::set_clb_(uint8_t b, uint8_t s, uint8_t e) {
  if (b < 4) {
    this->clb_[b][0] = s;
    this->clb_[b][1] = e;
  }
}

void CC1101::set_rxbw_(int bw) {
  this->bandwidth_ = bw;

  float f = (float) this->bandwidth_;

  int s1 = 3;
  int s2 = 3;

  for (int i = 0; i < 3 && f > 101.5625f; i++) {
    f /= 2;
    s1--;
  }

  for (int i = 0; i < 3 && f > 58.1f; i++) {
    f /= 1.25f;
    s2--;
  }

  this->split_mdmcfg4_();

  this->m4rxbw_ = (s1 << 6) | (s2 << 4);

  this->write_register_(CC1101_MDMCFG4, this->m4rxbw_ + this->m4dara_);
}

void CC1101::set_state_(uint8_t state) {
  if (state == CC1101_STX || state == CC1101_SRX || state == CC1101_SPWD) {
    this->set_state_(CC1101_SIDLE);
  }

  ESP_LOGV(TAG, "set_state_(0x%02X)", state);

  this->trxstate_ = state;

  if (state == CC1101_SRES) {
    // datasheet 19.1.2
    // this->disable(); // esp-idf calls end_transaction and asserts, because no begin_transaction was called
    this->cs_->digital_write(false);
    delayMicroseconds(5);
    // this->enable();
    this->cs_->digital_write(true);
    delayMicroseconds(10);
    // this->disable();
    this->cs_->digital_write(false);
    delayMicroseconds(41);
  }

  this->strobe_(state);
  this->wait_state_(state);
}

bool CC1101::wait_state_(uint8_t state) {
  static constexpr int TIMEOUT_LIMIT = 5000;
  int timeout = TIMEOUT_LIMIT;

  while (timeout > 0) {
    uint8_t s = this->read_status_register_(CC1101_MARCSTATE) & 0x1f;
    if (state == CC1101_SIDLE) {
      if (s == CC1101_MARCSTATE_IDLE)
        break;
    } else if (state == CC1101_SRX) {
      if (s == CC1101_MARCSTATE_RX || s == CC1101_MARCSTATE_RX_END || s == CC1101_MARCSTATE_RXTX_SWITCH)
        break;
    } else if (state == CC1101_STX) {
      if (s == CC1101_MARCSTATE_TX || s == CC1101_MARCSTATE_TX_END || s == CC1101_MARCSTATE_TXRX_SWITCH)
        break;
    } else {
      break;  // else if TODO
    }

    timeout--;

    delayMicroseconds(1);
  }

  if (timeout < TIMEOUT_LIMIT) {
    ESP_LOGW(TAG, "wait_state_(0x%02X) timeout = %d/%d", state, timeout, TIMEOUT_LIMIT);
    delayMicroseconds(100);
  }

  return timeout > 0;
}

void CC1101::split_mdmcfg2_() {
  uint8_t calc = this->read_status_register_(CC1101_MDMCFG2);

  this->m2dcoff_ = calc & 0x80;
  this->m2modfm_ = calc & 0x70;
  this->m2manch_ = calc & 0x08;
  this->m2syncm_ = calc & 0x07;
}

void CC1101::split_mdmcfg4_() {
  uint8_t calc = this->read_status_register_(CC1101_MDMCFG4);

  this->m4rxbw_ = calc & 0xf0;
  this->m4dara_ = calc & 0x0f;
}

void CC1101::begin_tx() {
  this->set_state_(CC1101_STX);

  if (this->gdo0_ != nullptr) {
#ifdef USE_ESP8266
#ifdef USE_ARDUINO
    noInterrupts();  // NOLINT
#else                // USE_ESP_IDF
    portDISABLE_INTERRUPTS()
#endif
    this->gdo0_->pin_mode(gpio::FLAG_OUTPUT);
#endif
  }
}

void CC1101::end_tx() {
  if (this->gdo0_ != nullptr) {
#ifdef USE_ESP8266
#ifdef USE_ARDUINO
    interrupts();  // NOLINT
#else              // USE_ESP_IDF
    portENABLE_INTERRUPTS()
#endif
    this->gdo0_->pin_mode(gpio::FLAG_INPUT);
#endif
  }

  this->set_state_(CC1101_SRX);
}

}  // namespace cc1101
}  // namespace esphome
