#include "ina3221.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome::ina3221 {

static const char *const TAG = "ina3221";

static const uint8_t INA3221_REGISTER_CONFIG = 0x00;
static const uint8_t INA3221_REGISTER_CHANNEL1_SHUNT_VOLTAGE = 0x01;
static const uint8_t INA3221_REGISTER_CHANNEL1_BUS_VOLTAGE = 0x02;
static const uint8_t INA3221_REGISTER_CHANNEL2_SHUNT_VOLTAGE = 0x03;
static const uint8_t INA3221_REGISTER_CHANNEL2_BUS_VOLTAGE = 0x04;
static const uint8_t INA3221_REGISTER_CHANNEL3_SHUNT_VOLTAGE = 0x05;
static const uint8_t INA3221_REGISTER_CHANNEL3_BUS_VOLTAGE = 0x06;

static const uint8_t INA3221_REGISTER_CHANNEL1_CRITICAL_ALERT = 0x07;
static const uint8_t INA3221_REGISTER_CHANNEL2_CRITICAL_ALERT = 0x08;
static const uint8_t INA3221_REGISTER_CHANNEL3_CRITICAL_ALERT = 0x09;
static const uint8_t INA3221_REGISTER_CHANNEL1_WARNING_ALERT = 0x0A;
static const uint8_t INA3221_REGISTER_CHANNEL2_WARNING_ALERT = 0x0B;
static const uint8_t INA3221_REGISTER_CHANNEL3_WARNING_ALERT = 0x0C;

static const uint8_t INA3221_REGISTER_SHUNT_VOLTAGE_SUM = 0x0D;
static const uint8_t INA3221_REGISTER_MASK_ENABLE = 0x0F;

static float get_conversion_time_ms(INA3221ConversionTime ct) {
  switch (ct) {
    case INA3221_CONVERSION_TIME_140US: return 0.14f;
    case INA3221_CONVERSION_TIME_204US: return 0.204f;
    case INA3221_CONVERSION_TIME_332US: return 0.332f;
    case INA3221_CONVERSION_TIME_588US: return 0.588f;
    case INA3221_CONVERSION_TIME_1100US: return 1.1f;
    case INA3221_CONVERSION_TIME_2116US: return 2.116f;
    case INA3221_CONVERSION_TIME_4156US: return 4.156f;
    case INA3221_CONVERSION_TIME_8244US: return 8.244f;
    default: return 1.1f;
  }
}

static int get_averaging_samples(INA3221Averaging avg) {
  switch (avg) {
    case INA3221_AVERAGING_1: return 1;
    case INA3221_AVERAGING_4: return 4;
    case INA3221_AVERAGING_16: return 16;
    case INA3221_AVERAGING_64: return 64;
    case INA3221_AVERAGING_128: return 128;
    case INA3221_AVERAGING_256: return 256;
    case INA3221_AVERAGING_512: return 512;
    case INA3221_AVERAGING_1024: return 1024;
    default: return 1;
  }
}

void INA3221Component::setup() {
  if (!this->write_byte_16(INA3221_REGISTER_CONFIG, 0x8000)) {
    this->mark_failed();
    return;
  }
  delay(1);

  int active_channels = (this->channels_[0].exists() ? 1 : 0) +
                        (this->channels_[1].exists() ? 1 : 0) +
                        (this->channels_[2].exists() ? 1 : 0);

  // Validate timing for SINGLE_SHOT mode
  if (this->mode_ == INA3221_MODE_SINGLE_SHOT && active_channels > 0) {
    float total_time_ms = (get_conversion_time_ms(this->bus_conversion_time_) +
                           get_conversion_time_ms(this->shunt_conversion_time_)) *
                          get_averaging_samples(this->averaging_) * active_channels;

    if (total_time_ms > this->get_update_interval()) {
      ESP_LOGE(TAG, "Single-Shot conversion time (%.1fms) exceeds update interval (%ums)! Please reduce averaging or conversion time.", 
               total_time_ms, this->get_update_interval());
      this->mark_failed();
      return;
    }
  }

  uint16_t config = 0;
  if (this->channels_[0].exists()) config |= 0b0100000000000000;
  if (this->channels_[1].exists()) config |= 0b0010000000000000;
  if (this->channels_[2].exists()) config |= 0b0001000000000000;
  
  config |= (this->averaging_ << 9);
  config |= (this->bus_conversion_time_ << 6);
  config |= (this->shunt_conversion_time_ << 3);
  config |= this->mode_; 

  if (!this->write_byte_16(INA3221_REGISTER_CONFIG, config)) {
    this->mark_failed();
    return;
  }

  for (int i = 0; i < 3; i++) {
    if (!std::isnan(this->channels_[i].critical_current_limit_)) {
      float limit_v = this->channels_[i].critical_current_limit_ * this->channels_[i].shunt_resistance_;
      int16_t reg_val = (int16_t)(limit_v * 1000000.0f / 40.0f);
      this->write_byte_16(INA3221_REGISTER_CHANNEL1_CRITICAL_ALERT + i, reg_val << 3);
    }
    if (!std::isnan(this->channels_[i].warning_current_limit_)) {
      float limit_v = this->channels_[i].warning_current_limit_ * this->channels_[i].shunt_resistance_;
      int16_t reg_val = (int16_t)(limit_v * 1000000.0f / 40.0f);
      this->write_byte_16(INA3221_REGISTER_CHANNEL1_WARNING_ALERT + i, reg_val << 3);
    }
  }

  if (this->has_summation_()) {
    uint16_t mask_reg = 0;
    if (this->channels_[0].exists()) mask_reg |= (1 << 14);
    if (this->channels_[1].exists()) mask_reg |= (1 << 13);
    if (this->channels_[2].exists()) mask_reg |= (1 << 12);
    this->write_byte_16(INA3221_REGISTER_MASK_ENABLE, mask_reg);
  }
}

void INA3221Component::dump_config() {
  ESP_LOGCONFIG(TAG, "INA3221:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
  }
  LOG_UPDATE_INTERVAL(this);

  LOG_SENSOR("  ", "Bus Voltage #1", this->channels_[0].bus_voltage_sensor_);
  LOG_SENSOR("  ", "Shunt Voltage #1", this->channels_[0].shunt_voltage_sensor_);
  LOG_SENSOR("  ", "Current #1", this->channels_[0].current_sensor_);
  LOG_SENSOR("  ", "Power #1", this->channels_[0].power_sensor_);
  LOG_SENSOR("  ", "Bus Voltage #2", this->channels_[1].bus_voltage_sensor_);
  LOG_SENSOR("  ", "Shunt Voltage #2", this->channels_[1].shunt_voltage_sensor_);
  LOG_SENSOR("  ", "Current #2", this->channels_[1].current_sensor_);
  LOG_SENSOR("  ", "Power #2", this->channels_[1].power_sensor_);
  LOG_SENSOR("  ", "Bus Voltage #3", this->channels_[2].bus_voltage_sensor_);
  LOG_SENSOR("  ", "Shunt Voltage #3", this->channels_[2].shunt_voltage_sensor_);
  LOG_SENSOR("  ", "Current #3", this->channels_[2].current_sensor_);
  LOG_SENSOR("  ", "Power #3", this->channels_[2].power_sensor_);

  LOG_SENSOR("  ", "Sum Shunt Voltage", this->sum_shunt_voltage_sensor_);
  LOG_SENSOR("  ", "Sum Current", this->sum_current_sensor_);
  LOG_SENSOR("  ", "Sum Power", this->sum_power_sensor_);
}

inline uint8_t ina3221_bus_voltage_register(int channel) { return 0x02 + channel * 2; }

inline uint8_t ina3221_shunt_voltage_register(int channel) { return 0x01 + channel * 2; }

void INA3221Component::update() {
  if (this->mode_ == INA3221_MODE_SINGLE_SHOT) {
    uint16_t config;
    if (this->read_byte_16(INA3221_REGISTER_CONFIG, &config)) {
      config = (config & 0xFFF8) | INA3221_MODE_SINGLE_SHOT;
      this->write_byte_16(INA3221_REGISTER_CONFIG, config);

      int active_channels = (this->channels_[0].exists() ? 1 : 0) +
                            (this->channels_[1].exists() ? 1 : 0) +
                            (this->channels_[2].exists() ? 1 : 0);

      float total_time_ms = (get_conversion_time_ms(this->bus_conversion_time_) +
                             get_conversion_time_ms(this->shunt_conversion_time_)) *
                            get_averaging_samples(this->averaging_) * active_channels;

      uint32_t wait_time = (uint32_t)(total_time_ms) + 5; 
      this->set_timeout("read", wait_time, [this]() { this->read_data_(); });
    }
  } else {
    this->read_data_();
  }
}

void INA3221Component::read_data_() {
  float total_bus_voltage_v = 0.0f;
  float total_current_a = 0.0f;
  int active_channels = 0;
  // Only run software accumulation if these sensors actually exist
  bool calculate_totals = (this->sum_current_sensor_ != nullptr || this->sum_power_sensor_ != nullptr);

  for (int i = 0; i < 3; i++) {
    INA3221Channel &channel = this->channels_[i];
    float bus_voltage_v = NAN, current_a = NAN;
    uint16_t raw;
    
    if (channel.should_measure_bus_voltage()) {
      if (!this->read_byte_16(ina3221_bus_voltage_register(i), &raw)) {
        this->status_set_warning();
        return;
      }
      bus_voltage_v = int16_t(raw) / 1000.0f;
      if (channel.bus_voltage_sensor_ != nullptr)
        channel.bus_voltage_sensor_->publish_state(bus_voltage_v);
    }
    
    if (channel.should_measure_shunt_voltage()) {
      if (!this->read_byte_16(ina3221_shunt_voltage_register(i), &raw)) {
        this->status_set_warning();
        return;
      }
      const float shunt_voltage_v = int16_t(raw) * 40.0f / 8.0f / 1000000.0f;
      if (channel.shunt_voltage_sensor_ != nullptr)
        channel.shunt_voltage_sensor_->publish_state(shunt_voltage_v);
      current_a = shunt_voltage_v / channel.shunt_resistance_;
      if (channel.current_sensor_ != nullptr)
        channel.current_sensor_->publish_state(current_a);
    }
    
    if (channel.power_sensor_ != nullptr) {
      channel.power_sensor_->publish_state(bus_voltage_v * current_a);
    }

    if (calculate_totals && channel.exists()) {
      if (!std::isnan(bus_voltage_v))
        total_bus_voltage_v += bus_voltage_v;
      if (!std::isnan(current_a))
        total_current_a += current_a;
      active_channels++;
    }
  }

  if (this->has_summation_()) {
    uint16_t raw;
    if (this->read_byte_16(INA3221_REGISTER_SHUNT_VOLTAGE_SUM, &raw)) {
      const float sum_shunt_voltage_v = int16_t(raw) * 40.0f / 2.0f / 1000000.0f;
      if (this->sum_shunt_voltage_sensor_ != nullptr)
        this->sum_shunt_voltage_sensor_->publish_state(sum_shunt_voltage_v);

      if (this->sum_current_sensor_ != nullptr)
        this->sum_current_sensor_->publish_state(total_current_a);

      if (this->sum_power_sensor_ != nullptr && active_channels > 0) {
        float avg_bus = total_bus_voltage_v / active_channels;
        this->sum_power_sensor_->publish_state(avg_bus * total_current_a);
      }
    }
  }
}

void INA3221Component::set_shunt_resistance(int channel, float resistance_ohm) {
  this->channels_[channel].shunt_resistance_ = resistance_ohm;
}

bool INA3221Component::INA3221Channel::exists() {
  return this->bus_voltage_sensor_ != nullptr || this->shunt_voltage_sensor_ != nullptr ||
         this->current_sensor_ != nullptr || this->power_sensor_ != nullptr;
}
bool INA3221Component::INA3221Channel::should_measure_shunt_voltage() {
  return this->shunt_voltage_sensor_ != nullptr || this->current_sensor_ != nullptr || this->power_sensor_ != nullptr;
}
bool INA3221Component::INA3221Channel::should_measure_bus_voltage() {
  return this->bus_voltage_sensor_ != nullptr || this->power_sensor_ != nullptr;
}

bool INA3221Component::has_summation_() {
  return this->sum_shunt_voltage_sensor_ != nullptr || this->sum_current_sensor_ != nullptr ||
         this->sum_power_sensor_ != nullptr;
}

void INA3221Component::on_shutdown() {
  if (this->power_down_on_shutdown_) {
    ESP_LOGD(TAG, "Putting INA3221 to sleep...");
    this->write_byte_16(0x00, 0x7120);
  }
}

}  // namespace esphome::ina3221
