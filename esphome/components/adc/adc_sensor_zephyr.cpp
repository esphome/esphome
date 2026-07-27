
#include "adc_sensor.h"
#ifdef USE_ZEPHYR
#include "esphome/core/log.h"

#include <algorithm>
#include <cmath>

#ifdef USE_NRF52
#include "hal/nrf_saadc.h"
#endif

#ifdef USE_ZEPHYR_ADC_EMULATION
#include <zephyr/drivers/adc/adc_emul.h>
#endif

namespace esphome::adc {

static const char *const TAG = "adc.zephyr";

void ADCSensor::setup() {
  if (!adc_is_ready_dt(this->channel_)) {
    ESP_LOGE(TAG, "ADC controller device %s not ready", this->channel_->dev->name);
    return;
  }

  auto err = adc_channel_setup_dt(this->channel_);
  if (err < 0) {
    ESP_LOGE(TAG, "Could not setup channel %s (%d)", this->channel_->dev->name, err);
    return;
  }
}

#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
static const LogString *gain_to_str(enum adc_gain gain) {
  switch (gain) {
    case ADC_GAIN_1_6:
      return LOG_STR("1/6");
    case ADC_GAIN_1_5:
      return LOG_STR("1/5");
    case ADC_GAIN_1_4:
      return LOG_STR("1/4");
#ifdef USE_ZEPHYR_VARIANT_FAMILY_ESP32
    case ADC_GAIN_2_7:
      return LOG_STR("2/7");
#endif
    case ADC_GAIN_1_3:
      return LOG_STR("1/3");
    case ADC_GAIN_2_5:
      return LOG_STR("2/5");
    case ADC_GAIN_1_2:
      return LOG_STR("1/2");
    case ADC_GAIN_2_3:
      return LOG_STR("2/3");
    case ADC_GAIN_4_5:
      return LOG_STR("4/5");
    case ADC_GAIN_1:
      return LOG_STR("1");
    case ADC_GAIN_2:
      return LOG_STR("2");
    case ADC_GAIN_3:
      return LOG_STR("3");
    case ADC_GAIN_4:
      return LOG_STR("4");
    case ADC_GAIN_6:
      return LOG_STR("6");
    case ADC_GAIN_8:
      return LOG_STR("8");
    case ADC_GAIN_12:
      return LOG_STR("12");
    case ADC_GAIN_16:
      return LOG_STR("16");
    case ADC_GAIN_24:
      return LOG_STR("24");
    case ADC_GAIN_32:
      return LOG_STR("32");
    case ADC_GAIN_64:
      return LOG_STR("64");
    case ADC_GAIN_128:
      return LOG_STR("128");
  }
  return LOG_STR("undefined gain");
}

static const LogString *reference_to_str(enum adc_reference reference) {
  switch (reference) {
    case ADC_REF_VDD_1:
      return LOG_STR("VDD");
    case ADC_REF_VDD_1_2:
      return LOG_STR("VDD/2");
    case ADC_REF_VDD_1_3:
      return LOG_STR("VDD/3");
    case ADC_REF_VDD_1_4:
      return LOG_STR("VDD/4");
    case ADC_REF_INTERNAL:
      return LOG_STR("INTERNAL");
    case ADC_REF_EXTERNAL0:
      return LOG_STR("External, input 0");
    case ADC_REF_EXTERNAL1:
      return LOG_STR("External, input 1");
  }
  return LOG_STR("undefined reference");
}

#ifdef USE_NRF52
// ESP32's Zephyr ADC driver selects the channel via the channel node's `reg`
// (channel_id, already logged above) -- it never reads input_positive/negative
// at all, unlike Nordic's SAADC which requires a pin-to-AIN mapping.
static const LogString *input_to_str(uint8_t input) {
  switch (input) {
    case NRF_SAADC_INPUT_AIN0:
      return LOG_STR("AIN0");
    case NRF_SAADC_INPUT_AIN1:
      return LOG_STR("AIN1");
    case NRF_SAADC_INPUT_AIN2:
      return LOG_STR("AIN2");
    case NRF_SAADC_INPUT_AIN3:
      return LOG_STR("AIN3");
    case NRF_SAADC_INPUT_AIN4:
      return LOG_STR("AIN4");
    case NRF_SAADC_INPUT_AIN5:
      return LOG_STR("AIN5");
    case NRF_SAADC_INPUT_AIN6:
      return LOG_STR("AIN6");
    case NRF_SAADC_INPUT_AIN7:
      return LOG_STR("AIN7");
    case NRF_SAADC_INPUT_VDD:
      return LOG_STR("VDD");
    case NRF_SAADC_INPUT_VDDHDIV5:
      return LOG_STR("VDDHDIV5");
  }
  return LOG_STR("undefined input");
}
#endif  // USE_NRF52
#endif  // ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE

void ADCSensor::dump_config() {
  LOG_SENSOR("", "ADC Sensor", this);
  LOG_PIN("  Pin: ", this->pin_);
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
  ESP_LOGV(TAG,
           "  Name: %s\n"
           "  Channel: %d\n"
           "  vref_mv: %d\n"
           "  Resolution %d\n"
           "  Oversampling %d",
           this->channel_->dev->name, this->channel_->channel_id, this->channel_->vref_mv, this->channel_->resolution,
           this->channel_->oversampling);

  ESP_LOGV(TAG,
           "  Gain: %s\n"
           "  reference: %s\n"
           "  acquisition_time: %d\n"
           "  differential %s",
           LOG_STR_ARG(gain_to_str(this->channel_->channel_cfg.gain)),
           LOG_STR_ARG(reference_to_str(this->channel_->channel_cfg.reference)),
           this->channel_->channel_cfg.acquisition_time, YESNO(this->channel_->channel_cfg.differential));
#ifdef USE_NRF52
  if (this->channel_->channel_cfg.differential) {
    ESP_LOGV(TAG,
             "  Positive: %s\n"
             "  Negative: %s",
             LOG_STR_ARG(input_to_str(this->channel_->channel_cfg.input_positive)),
             LOG_STR_ARG(input_to_str(this->channel_->channel_cfg.input_negative)));
  } else {
    ESP_LOGV(TAG, "  Positive: %s", LOG_STR_ARG(input_to_str(this->channel_->channel_cfg.input_positive)));
  }
#endif  // USE_NRF52
#endif

  LOG_UPDATE_INTERVAL(this);
}

// Also compiled under USE_ZEPHYR_ADC_EMULATION so an emulated channel (e.g. on
// native_sim, which is not itself an esp32-family variant) can still exercise
// this gain-switching logic against scripted `emulation:` voltages.
#if defined(USE_ZEPHYR_VARIANT_FAMILY_ESP32) || defined(USE_ZEPHYR_ADC_EMULATION)
// Ports adc_sensor_esp32.cpp's autorange algorithm onto Zephyr's ADC API. The ESP32
// Zephyr driver (adc_esp32.c) already applies the same ESP-IDF eFuse calibration
// internally, so per-gain readings here are just as calibrated as ESP-IDF's.
static const adc_gain AUTORANGE_GAINS[] = {ADC_GAIN_1_4, ADC_GAIN_1_2, ADC_GAIN_4_5, ADC_GAIN_1};

float ADCSensor::sample_autorange_() {
#ifdef USE_ZEPHYR_ADC_EMULATION
  if (this->emulated_values_ != nullptr) {
    // Autorange requires samples: 1 (enforced in sensor.py's validate_config),
    // so each emulated group here has exactly one entry.
    adc_emul_const_value_set(this->channel_->dev, this->channel_->channel_id,
                             this->emulated_values_[this->emulated_group_index_]);
    this->emulated_group_index_ = (this->emulated_group_index_ + 1) % this->emulated_group_count_;
  }
#endif
  const int32_t full_scale = (1 << this->channel_->resolution) - 1;
  const int32_t half_scale = full_scale / 2;
  const int32_t ref_mv = (int32_t) adc_ref_internal(this->channel_->dev);

  auto read_gain = [this, ref_mv](adc_gain gain, int32_t *raw, float *mv) -> bool {
    adc_channel_cfg cfg = this->channel_->channel_cfg;
    cfg.gain = gain;
    if (adc_channel_setup(this->channel_->dev, &cfg) < 0) {
      return false;
    }

    int16_t buf = 0;
    struct adc_sequence sequence = {
        .buffer = &buf,
        .buffer_size = sizeof(buf),
    };
    if (adc_sequence_init_dt(this->channel_, &sequence) < 0 || adc_read(this->channel_->dev, &sequence) < 0) {
      return false;
    }
    *raw = buf < 0 ? 0 : buf;

    int32_t val_mv = *raw;
    if (adc_raw_to_millivolts(ref_mv, gain, this->channel_->resolution, &val_mv) < 0) {
      return false;
    }
    *mv = val_mv / 1000.0f;
    return true;
  };

  int32_t raw[4] = {full_scale, full_scale, full_scale, full_scale};
  float mv[4] = {0, 0, 0, 0};

  if (!read_gain(AUTORANGE_GAINS[0], &raw[0], &mv[0])) {
    ESP_LOGE(TAG, "Failed to read ADC in autorange mode");
    return NAN;
  }
  for (size_t i = 1; i < 4 && raw[i - 1] < full_scale; i++) {
    if (!read_gain(AUTORANGE_GAINS[i], &raw[i], &mv[i])) {
      ESP_LOGW(TAG, "Failed to read ADC in autorange mode at gain tier %zu", i);
      return NAN;
    }
  }

  // Weight peaks at half_scale for the two middle tiers; the widest tier
  // (index 0) only tapers toward 0 (never rails high by construction), and
  // the narrowest tier (index 3) only tapers toward full_scale.
  const uint32_t c0 = std::min(raw[0], half_scale);
  const uint32_t c1 = std::max<int32_t>(half_scale - std::abs(raw[1] - half_scale), 0);
  const uint32_t c2 = std::max<int32_t>(half_scale - std::abs(raw[2] - half_scale), 0);
  const uint32_t c3 = std::min(full_scale - raw[3], half_scale);
  const uint32_t csum = c0 + c1 + c2 + c3;

  ESP_LOGVV(TAG, "Autorange raw: %d/%d/%d/%d mv: %.6f/%.6f/%.6f/%.6f weights: %u/%u/%u/%u", raw[0], raw[1], raw[2],
            raw[3], mv[0], mv[1], mv[2], mv[3], c0, c1, c2, c3);

  if (csum == 0) {
    ESP_LOGE(TAG, "Invalid weight sum in autorange calculation");
    return NAN;
  }

  return (mv[0] * c0 + mv[1] * c1 + mv[2] * c2 + mv[3] * c3) / csum;
}
#endif  // USE_ZEPHYR_VARIANT_FAMILY_ESP32 || USE_ZEPHYR_ADC_EMULATION

float ADCSensor::sample() {
#if defined(USE_ZEPHYR_VARIANT_FAMILY_ESP32) || defined(USE_ZEPHYR_ADC_EMULATION)
  if (this->autorange_) {
    return this->sample_autorange_();
  }
#endif
  auto aggr = Aggregator<int32_t>(this->sampling_mode_);
  int err;
#ifdef USE_ZEPHYR_ADC_EMULATION
  const uint32_t *emulated_group =
      this->emulated_values_ != nullptr
          ? this->emulated_values_ + (static_cast<size_t>(this->emulated_group_index_) * this->emulated_group_size_)
          : nullptr;
#endif
  for (uint8_t sample = 0; sample < this->sample_count_; sample++) {
#ifdef USE_ZEPHYR_ADC_EMULATION
    if (emulated_group != nullptr) {
      adc_emul_const_value_set(this->channel_->dev, this->channel_->channel_id, emulated_group[sample]);
    }
#endif
    int16_t buf = 0;
    struct adc_sequence sequence = {
        .buffer = &buf,
        /* buffer size in bytes, not number of samples */
        .buffer_size = sizeof(buf),
    };
    int32_t val_raw;

    err = adc_sequence_init_dt(this->channel_, &sequence);
    if (err < 0) {
      ESP_LOGE(TAG, "Could sequence init %s (%d)", this->channel_->dev->name, err);
      return 0.0;
    }

    err = adc_read(this->channel_->dev, &sequence);
    if (err < 0) {
      ESP_LOGE(TAG, "Could not read %s (%d)", this->channel_->dev->name, err);
      return 0.0;
    }

    val_raw = (int32_t) buf;
    if (!this->channel_->channel_cfg.differential) {
      // https://github.com/adafruit/Adafruit_nRF52_Arduino/blob/0ed4d9ffc674ae407be7cacf5696a02f5e789861/cores/nRF5/wiring_analog_nRF52.c#L222
      if (val_raw < 0) {
        val_raw = 0;
      }
    }
    aggr.add_sample(val_raw);
  }

#ifdef USE_ZEPHYR_ADC_EMULATION
  if (emulated_group != nullptr) {
    this->emulated_group_index_ = (this->emulated_group_index_ + 1) % this->emulated_group_count_;
  }
#endif

  int32_t val_mv = aggr.aggregate();

  if (this->output_raw_) {
    return val_mv;
  }

  err = adc_raw_to_millivolts_dt(this->channel_, &val_mv);
  /* conversion to mV may not be supported, skip if not */
  if (err < 0) {
    ESP_LOGE(TAG, "Value in mV not available %s (%d)", this->channel_->dev->name, err);
    return 0.0;
  }

  return val_mv / 1000.0f;
}

}  // namespace esphome::adc
#endif
