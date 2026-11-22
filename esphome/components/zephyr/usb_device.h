#pragma once

namespace esphome::zephyr {

#if defined(CONFIG_USB_DEVICE_STACK) || defined(CONFIG_USB_DEVICE_STACK_NEXT)
/// Start the USB device on whichever Zephyr USB device stack the build uses (the legacy one or
/// device_next). Safe to call from several components; only the first call starts the device.
/// Call from the main thread. Returns 0 or a negative error code.
int usb_device_enable();
#endif

}  // namespace esphome::zephyr
