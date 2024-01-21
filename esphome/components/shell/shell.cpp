#include "shell.h"
#include <zephyr/usb/usb_device.h>

struct device;

namespace esphome {
namespace shell {

void Shell::setup() {
#if DT_NODE_HAS_COMPAT(DT_CHOSEN(zephyr_shell_uart), zephyr_cdc_acm_uart)
	const device *dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_shell_uart));
	if (!device_is_ready(dev) || usb_enable(NULL)) {
		return;
	}
#else
#error "shell is not set in device tree"
#endif
}

}  // namespace shell
}  // namespace esphome
