---
description: "Instructions for setting up TinyUSB in ESPHome"
title: "TinyUSB"
params:
  seo:
    description: Instructions for setting up TinyUSB in ESPHome
    image: usb.svg
---

The `tinyusb` component implements a foundation for USB device functionality. It is currently supported on the
following ESP32 microcontroller variants:

- ESP32-P4
- ESP32-S2
- ESP32-S3

The component simply initializes the TinyUSB driver, allowing the microcontroller to act as a USB device when connected
to a USB host.

> [!NOTE]
> This component:
>
> - does **not** implement any specific device functionality; it is simply a foundation for other components to do so.
> - cannot be used with the {{< docref "/components/usb_host" >}}; operation as both a host and a device simultaneously
>   is not possible.

```yaml
# Example minimal configuration entry
tinyusb:
```

## Configuration variables

- **id** (*Optional*, [ID](/guides/configuration-types#config-id)): Manually specify the ID for this component.
- **usb_product_id** (*Optional*, int): USB product identifier. Defaults to `0x4001`.
- **usb_vendor_id** (*Optional*, int): USB vendor identifier. Defaults to `0x303A` (Espressif Systems).
- **usb_lang_id** (*Optional*, int): USB language identifier. Defaults to `0x0409` (English - United States).
- **usb_manufacturer_str** (*Optional*, string): Manufacturer string descriptor. Defaults to `"ESPHome"`.
- **usb_product_str** (*Optional*, string): Product name string descriptor. Defaults to `"ESPHome"`.
- **usb_serial_str** (*Optional*, string): Serial number string descriptor. If not specified, the device's MAC address
  will be used.

## Notes

### Vendor and Product IDs

When specifying custom `usb_vendor_id` and `usb_product_id` values, be aware that:

- USB Vendor IDs are officially assigned by the USB Implementers Forum (USB-IF).
- Using unassigned or third-party vendor/product ID combinations may result in unexpected (host) behavior.
- The default vendor ID `0x303A` is assigned to Espressif Systems.
- For hobbyist and development purposes, you may use test IDs, but these should not be used in production devices.

### Language Identifiers

The `usb_lang_id` field uses USB Language IDs as defined by the USB specification. Common values include:

- `0x0409` - English (United States) - Default
- `0x0809` - English (United Kingdom)
- `0x0407` - German (Germany)
- `0x040C` - French (France)

A more complete list can be found [here](https://github.com/brookebasile/USB-langids/blob/master/USB_LANGIDs.pdf).

## See Also

- [TinyUSB Documentation](https://docs.tinyusb.org/)
- {{< apiref "tinyusb/tinyusb_component.h" "tinyusb/tinyusb_component.h" >}}
