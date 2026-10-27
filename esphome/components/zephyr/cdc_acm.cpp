#if defined(CONFIG_CDC_ACM_DTE_RATE_CALLBACK_SUPPORT)
#include "cdc_acm.h"
#include "esphome/core/application.h"
#include <atomic>
#include <iterator>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/uart/cdc_acm.h>

#define DEVICE_AND_COMMA(node_id) DEVICE_DT_GET(node_id),

namespace esphome::zephyr {

static CdcAcm cdc_acm_instance;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
CdcAcm *const global_cdc_acm = &cdc_acm_instance;

#if DT_HAS_COMPAT_STATUS_OKAY(zephyr_cdc_acm_uart)
static const device *const CDC_DEVICES[] = {DT_FOREACH_STATUS_OKAY(zephyr_cdc_acm_uart, DEVICE_AND_COMMA)};
// Latest rate per port, written by the USB stack and taken by the main loop; 0 means none
static std::atomic<uint32_t> pending_rates[std::size(CDC_DEVICES)];  // NOLINT

void CdcAcm::schedule_driver_callbacks_() {
  // Keyed on CDC_DEVICES, not this, so the flush timeout below cannot replace it
  App.scheduler.set_timeout(CDC_DEVICES, 0, []() {
    for (const device *dev : CDC_DEVICES) {
      // only one global callback can be registered
      cdc_acm_dte_rate_callback_set(dev, CdcAcm::cdc_dte_rate_callback_);
    }
  });
}

void CdcAcm::flush_pending_rates_() {
  for (size_t i = 0; i < std::size(CDC_DEVICES); i++) {
    uint32_t rate = pending_rates[i].exchange(0);
    if (rate != 0)
      this->rate_callbacks_.call(CDC_DEVICES[i], rate);
  }
}

void CdcAcm::cdc_dte_rate_callback_(const struct device *device, uint32_t rate) {
  for (size_t i = 0; i < std::size(CDC_DEVICES); i++) {
    if (CDC_DEVICES[i] == device) {
      pending_rates[i].store(rate);
      // A newer request replaces a pending one; the flush still delivers every port's latest rate
      App.scheduler.set_timeout(global_cdc_acm, 0, []() { global_cdc_acm->flush_pending_rates_(); });
      return;
    }
  }
}
#else
void CdcAcm::schedule_driver_callbacks_() {}
#endif  // DT_HAS_COMPAT_STATUS_OKAY(zephyr_cdc_acm_uart)

}  // namespace esphome::zephyr

#endif
