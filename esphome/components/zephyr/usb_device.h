#pragma once

namespace esphome::zephyr {

#if defined(CONFIG_USB_DEVICE_STACK) || defined(CONFIG_USB_DEVICE_STACK_NEXT)
/// Start the USB device on whichever Zephyr USB device stack the build uses (the legacy one or
/// device_next). Safe to call from several components; only the first call starts the device,
/// later calls return its result. Call from the main thread.
/// Does not log, because the logger calls it before logging is ready.
/// @return 0 on success, a negative errno value on failure.
int usb_device_enable();
#else
inline int usb_device_enable() { return 0; }
#endif

}  // namespace esphome::zephyr
