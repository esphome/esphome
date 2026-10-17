#if defined(CONFIG_CDC_ACM_DTE_RATE_CALLBACK_SUPPORT)
#include "cdc_acm.h"
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/uart/cdc_acm.h>

#define DEVICE_AND_COMMA(node_id) DEVICE_DT_GET(node_id),

namespace esphome::zephyr {

CdcAcm::CdcAcm() { global_cdc_acm = this; }

void CdcAcm::setup() {
#if DT_HAS_COMPAT_STATUS_OKAY(zephyr_cdc_acm_uart)
  const struct device *cdc_dev[] = {DT_FOREACH_STATUS_OKAY(zephyr_cdc_acm_uart, DEVICE_AND_COMMA)};
  for (auto &idx : cdc_dev) {
    // only one global callback can be registered
    cdc_acm_dte_rate_callback_set(idx, CdcAcm::cdc_dte_rate_callback_);
  }
#endif  // DT_HAS_COMPAT_STATUS_OKAY(zephyr_cdc_acm_uart)
}

void CdcAcm::cdc_dte_rate_callback_(const struct device *device, uint32_t rate) {
  global_cdc_acm->defer([device, rate]() { global_cdc_acm->rate_callbacks_.call(device, rate); });
}

CdcAcm *global_cdc_acm;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esphome::zephyr

#elif defined(CONFIG_USB_DEVICE_STACK_NEXT)
#include "cdc_acm.h"
#include <zephyr/drivers/uart.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/usb/usbd_msg.h>

namespace esphome::zephyr {

void CdcAcm::usbd_msg_cb_(struct usbd_context *ctx, const struct usbd_msg *msg) {
  if (msg->type != USBD_MSG_CDC_ACM_LINE_CODING || global_cdc_acm == nullptr) {
    return;
  }
  uint32_t rate;
  if (uart_line_ctrl_get(msg->dev, UART_LINE_CTRL_BAUD_RATE, &rate) != 0) {
    return;
  }
  const struct device *dev = msg->dev;
  global_cdc_acm->defer([dev, rate]() { global_cdc_acm->rate_callbacks_.call(dev, rate); });
}

CdcAcm::CdcAcm() { global_cdc_acm = this; }

void CdcAcm::setup() {
  STRUCT_SECTION_FOREACH(usbd_context, ctx) { usbd_msg_register_cb(ctx, CdcAcm::usbd_msg_cb_); }
}

CdcAcm *global_cdc_acm;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esphome::zephyr

#endif
