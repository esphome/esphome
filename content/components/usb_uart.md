---
description: "Instructions for setting up a USB Host UART interface on an ESP32 in ESPHome"
title: "USB Host UART Interface"
params:
  seo:
    description: Instructions for setting up a USB Host UART interface on an ESP32 in ESPHome
    image: usb.svg
---

This component allows an ESP32-S3 or ESP32-S2 to host USB-serial peripheral devices. It uses the {{< docref "/components/usb_host" >}}
component to interface to the device as a USB-OTG host.

Currently supported devices are listed in the table below:

### Supported Devices

| Name       | VID    | PID    | Description                                             |
| ---------- | ------ | ------ | ------------------------------------------------------- |
| CH34X      | 0x1A86 | 0x55D5 | USB to serial adapter, multi-channel (up to 3 channels) |
| CH340      | 0x1A86 | 0x7523 | USB to serial adapter, single channel                   |
| ESP_JTAG | 0x303A | 0x1001 | ESP32 JTAG interface |
| STM32_VCP | 0x0483 | 0x5740 | STM32 Virtual COM Port |
| CDC_ACM | 0x0000 | 0x0000 | USB CDC ACM (Abstract Control Model) |
| CP210X | 0x10C4 | 0xEA60 | Silicon Labs USB to UART Bridge |

```yaml
# Example minimal configuration entry
usb_uart:
  - type: cp210x
    channels:
      - id: uch_1
        baud_rate: 9600
        buffer_size: 1024
```

## Configuration variables

- **id** (*Optional*, [ID](/guides/configuration-types#id)): The id to use for this component.
- **type** (**Required**, string): The type of USB-serial device to connect to. One of `ch34x`, `ch340`, `esp_jtag`, `stm32_vcp`, `cdc_acm`, `cp210x`.
- **channels** (**Required**, list): A list of channels to configure.
- **vid** (*Optional*, int): The vendor ID of the device. Use 0 as a wildcard. Each type has a default VID which will be overridden if this is set.
- **pid** (*Optional*, int): The product ID of the device. Use 0 as a wildcard. Each type has a default PID which will be overridden if this is set.

Setting both `vid` and `pid` to 0 will match any device.

## Channel configuration options

- **id** (*Optional*, [ID](/guides/configuration-types#id)): An id to assign to the channel. This id may be used anywhere a `uart` component is required.
- **baud_rate** (**Required**, int): The baud rate to use for the channel. This is optional (and ignored) for the `stm32_vcp`, `esp_jtag` and `cdc_acm` types.
- **buffer_size** (*Optional*, int): The size of the buffer to use for the channel. Defaults to 256 bytes.
- **stop_bits** (*Optional*, float): The number of stop bits to use. Defaults to 1. Other options are 1.5 and 2.
- **data_bits** (*Optional*, int): The number of data bits to use in the range 5-8. Defaults to 8.
- **parity** (*Optional*, string): The parity to use. One of `NONE`, `EVEN`, `ODD`, `MARK` and `SPACE`. Defaults to `NONE`.
- **dummy_receiver** (*Optional*, boolean): If set to true, the channel will consume any data received. This is useful for debugging purposes. Defaults to false.
- **debug** (*Optional*, boolean): If set to true, the channel will log all data sent and received. Defaults to false.

## Device types

The `cdc_acm` type is a generic USB CDC ACM (Abstract Control Model) device. This is a common USB device class for serial communication.
This driver does not strictly enforce the CDC-ACM configuration specification, so it may work with devices that do not properly implement that specification. It expects to find a single interrupt endpoint, a single bulk in endpoint, and a single bulk out endpoint.
The `cdc_acm`, `esp_jtag` and `stm32_vcp` types do not support changing baud rate, stop bits or number of data bits, as they implement a virtual channel not typically associated with a physical UART.

### See Also

- {{< docref "/components/usb_host" >}}
- {{< apiref "usb_uart/usb_uart.h" "usb_uart/usb_uart.h" >}}
