#pragma once

#if defined(CONFIG_USB_DEVICE_STACK) || defined(CONFIG_USB_DEVICE_STACK_NEXT)

namespace esphome::zephyr {

/// Start the USB device on whichever Zephyr USB device stack the build uses (the legacy one or
/// device_next). Safe to call from several components; only the first call starts the device.
/// Call from the main thread.
void usb_device_enable();

}  // namespace esphome::zephyr

#endif
