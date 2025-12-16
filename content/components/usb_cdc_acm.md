---
description: "Instructions for setting up USB CDC-ACM virtual serial ports on ESP32 variants in ESPHome"
title: "USB CDC-ACM Interface"
params:
  seo:
    description: Instructions for setting up USB CDC-ACM virtual serial ports on ESP32 variants in ESPHome
    image: usb.svg
---

The USB CDC-ACM (Communications Device Class - Abstract Control Model) component enables supported devices to function
as USB virtual serial ports. When connected to a host computer, the microcontroller will appear as one or
more serial/COM ports, allowing serial communication with the application running on the microcontroller.

You must have {{< docref "/components/tinyusb" >}} in your device's configuration to use this component.

The following ESP32 microcontroller variants are currently supported:

- ESP32-P4
- ESP32-S2
- ESP32-S3

```yaml
# Example minimal configuration entry
usb_cdc_acm:
```

## Configuration variables

- **rx_buffer_size** (*Optional*, int): Size of the USB receive buffer in bytes. Range: 1-65535. Defaults to `256`.
- **tx_buffer_size** (*Optional*, int): Size of the USB transmit buffer in bytes. Range: 1-65535. Defaults to `256`.
- **interfaces** (*Optional*, list): List of CDC-ACM interface instances. Up to two are supported; at least one is
  required. Defaults to a single-item list which defines a single interface only.

## Interface configuration variables

Each interface in the `interfaces` list consists of the following:

- **id** (*Optional*, [ID](/guides/configuration-types#config-id)): The ID to use for this interface instance. This is
  used to refer to the interface in other components, platforms or lambdas.

## Multiple Interface Example

The USB CDC-ACM component supports up to two simultaneous virtual serial port interfaces on a single device. This
allows you to create multiple independent communication channels over a single physical USB connection.

```yaml
# Example configuration with two interfaces
usb_cdc_acm:
  interfaces:
    - id: cdc_acm_1
    - id: cdc_acm_2
```

In this configuration, the device will appear as two separate serial/COM ports to the host computer. Each interface
operates independently with its own data buffers.

## Buffer Size Considerations

The buffer sizes determine how much data can be temporarily stored during USB transfers:

- **Small buffers (256 bytes, default)**: Suitable for low-bandwidth applications and conserves RAM
- **Large buffers (512-1024 bytes)**: Recommended for high-throughput applications or when handling bursts of data

Increase buffer sizes if you experience data loss or need to handle larger data packets without frequent polling.

## See Also

- {{< docref "/components/tinyusb" >}}
- {{< apiref "usb_cdc_acm/usb_cdc_acm.h" "usb_cdc_acm/usb_cdc_acm.h" >}}
