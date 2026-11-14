#pragma once

#include <cstdint>

#include "esphome/core/gpio.h"

namespace esphome::tuya {

/// What the low_power protocol needs from the platform that powers the Wi-Fi module.
///
/// A platform with power management implements this and passes it to Tuya::set_low_power_host().
/// Without a host the protocol still runs, but always answers the MCU's sleep request (CMD38) with "busy".
class TuyaLowPowerHost {
 public:
  /// Tell the platform that an external MCU decides when the module may sleep.
  virtual void require_external_mcu() = 0;
  /// Whether the MCU protocol is idle and in sync, so sleep is allowed from its side.
  virtual void set_mcu_synchronized(bool synchronized) = 0;
  /// Monotonic count of received bytes lost or damaged by the MCU UART; 0 if the platform cannot tell.
  virtual uint32_t uart_rx_error_count() const = 0;
  /// An update is being written, so the module must stay awake.
  virtual bool ota_busy() const = 0;

  /// Whether the platform can put the module into standby on the MCU's request.
  virtual bool standby_supported() const = 0;
  /// The standby wake input the platform listens on, or -1.
  virtual int standby_wake_pin() const = 0;
  /// Whether standby is configured to wait for the MCU's consent.
  virtual bool standby_requires_external_sync() const = 0;
  /// Whether all network services could be stopped now.
  virtual bool standby_may_quiesce() const = 0;
  /// Whether all network services have stopped.
  virtual bool standby_services_quiesced() const = 0;
  /// Ask network services to stop (true) or resume (false) before a standby.
  virtual void request_preflight(bool requested) = 0;
  virtual bool preflight_requested() const = 0;
  /// Ask the platform to enter (true) or not to enter (false) standby.
  virtual void request_standby(bool requested) = 0;
  virtual bool standby_requested() const = 0;
  /// Whether the platform is taking the network down on purpose for a standby.
  virtual bool network_quiescing() const = 0;
  /// Whether the network stays connected during standby, so a timer or a received packet
  /// can wake the module while the MCU still sleeps.
  virtual bool standby_keeps_network() const { return false; }
  /// Maps the wake lines named in the MCU's product info (pin numbers of the module the MCU firmware
  /// was written for) to this module and uses the wake input where the configuration names none.
  /// With create_mcu_wake_output, also sets up the output to the MCU's wake line (see mcu_wake_output()).
  /// Returns the mapped wake input in standby_wake_pin() numbering, or -1 when it cannot be mapped.
  virtual int configure_wake_lines(int /*wake_gpio*/, int /*mcu_wake_gpio*/, bool /*mcu_wake_active_high*/,
                                   bool /*create_mcu_wake_output*/) {
    return -1;
  }
  /// Platform-specific bit mask of what currently keeps the module awake, for diagnostics; 0 if unknown.
  virtual uint32_t standby_blockers() const { return 0; }
  /// The output to the MCU's wake line set up by configure_wake_lines(), or nullptr.
  virtual GPIOPin *mcu_wake_output() { return nullptr; }

 protected:
  ~TuyaLowPowerHost() = default;
};

}  // namespace esphome::tuya
