---
description: "Instructions for setting up a USB Host interface on an ESP32 in ESPHome"
title: "USB Host Interface"
params:
  seo:
    description: Instructions for setting up a USB Host interface on an ESP32 in ESPHome
    image: usb.svg
---

The USB Host interface on the ESP32-S3 and ESP32-S2 is used to connect to USB peripheral devices. Multiple
devices may be configured, but only one can be connected at any time. By default the device must be directly
connected to the ESP32, but this can be changed by setting the `enable_hubs` option to `true`.

This component is used by the `usb_uart` component to allow the ESP32 to connect to USB-serial devices. It is also
possible to configure devices directly in this component, but this has no application other than for debug purposes.

```yaml
# Example configuration entry
usb_host:
  enable_hubs: true
  max_transfer_requests: 32  # For high-throughput devices like USB UART at 115200+ baud
  devices:
    - id: device_0
      vid: 0x1725
      pid: 0x1234
```

## Configuration variables

- **id** (*Optional*, [ID](/guides/configuration-types#id)): The id to use for this component.
- **enable_hubs** (*Optional*, boolean): Whether to include support for hubs. Defaults to `false`.
- **max_transfer_requests** (*Optional*, int): Maximum number of concurrent USB transfer requests. Range: 1-32. Defaults to `16`. Increase this value for high-throughput devices (e.g., USB UART at 115200+ baud) if you see "All X transfer slots in use" errors.
- **devices** (*Optional*, list): A list of devices to configure.

## Device configuration options

- **id** (*Optional*, [ID](/guides/configuration-types#id)): An id to assign to the device.
- **vid** (**Required**, int): The vendor ID of the device. Use 0 as a wildcard.
- **pid** (**Required**, int): The product ID of the device. Use 0 as a wildcard.

Setting both `vid` and `pid` to 0 will match any device.

If a device is configured and a device is connected that matches the configuration, the device will be
connected to the ESP32 and log entries will appear at the DEBUG level. If the log level is set to VERBOSE,
then the configuration descriptors of the device will be dumped. The device will remain connected until
it is disconnected or the ESP32 is reset.

If a device is plugged in that does not match any configured device, the device will be disconnected and
a log entry will appear at the DEBUG level.

### See Also

- {{< docref "/components/usb_uart" >}}
- {{< apiref "usb_host/usb_host.h" "usb_host/usb_host.h" >}}
