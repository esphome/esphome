#include "ir_rf_proxy.h"

#include <cinttypes>

#include "esphome/core/log.h"

namespace esphome::ir_rf_proxy {

static const char *const TAG = "ir_rf_proxy";

// ========== IrRfProxy (Infrared platform) ==========

#ifdef USE_INFRARED

void IrRfProxy::dump_config() {
  ESP_LOGCONFIG(TAG,
                "IR Proxy '%s'\n"
                "  Supports Transmitter: %s\n"
                "  Supports Receiver: %s",
                this->get_name().c_str(), YESNO(this->get_supports_transmitter()),
                YESNO(this->get_supports_receiver()));

  if (this->is_rf()) {
    ESP_LOGCONFIG(TAG, "  Hardware Type: RF (%.3f MHz)", this->frequency_khz_ / 1e3f);
  } else {
    ESP_LOGCONFIG(TAG, "  Hardware Type: Infrared");
  }
}

#endif  // USE_INFRARED

// ========== RfProxy (Radio Frequency platform) ==========

#ifdef USE_RADIO_FREQUENCY

void RfProxy::setup() {
  ir_rf_base::IrRfEntity::setup();
  // remote_transmitter/receiver always uses OOK (on-off keying)
  this->traits_.add_supported_modulation(radio_frequency::RadioFrequencyModulation::RADIO_FREQUENCY_MODULATION_OOK);
}

void RfProxy::dump_config() {
  ESP_LOGCONFIG(TAG,
                "RF Proxy '%s'\n"
                "  Supports Transmitter: %s\n"
                "  Supports Receiver: %s",
                this->get_name().c_str(), YESNO(this->get_supports_transmitter()),
                YESNO(this->get_supports_receiver()));

  const auto &traits = this->traits_;
  if (traits.get_frequency_min_hz() > 0) {
    if (traits.get_frequency_min_hz() == traits.get_frequency_max_hz()) {
      ESP_LOGCONFIG(TAG, "  Frequency: %.3f MHz (fixed)", traits.get_frequency_min_hz() / 1e6f);
    } else {
      ESP_LOGCONFIG(TAG, "  Frequency Range: %.3f - %.3f MHz", traits.get_frequency_min_hz() / 1e6f,
                    traits.get_frequency_max_hz() / 1e6f);
    }
  }
}

bool RfProxy::control(const radio_frequency::RadioFrequencyCall &call) {
  // RF: no IR carrier modulation.  Any RF front-end coordination (state turnaround, retuning)
  // happens via the radio_frequency entity's on_control trigger and remote_transmitter's
  // on_transmit/on_complete triggers — wired up in user YAML.
  return this->transmit_raw_(call, 0);
}

#endif  // USE_RADIO_FREQUENCY

}  // namespace esphome::ir_rf_proxy
