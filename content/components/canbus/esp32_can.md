---
description: "Instructions for setting up the ESP32 CAN bus platform in ESPHome"
title: "ESP32 CAN"
params:
  seo:
    description: Instructions for setting up the ESP32 CAN bus platform in ESPHome
    image: canbus.svg
---

{{< anchor "esp32-can" >}}

The ESP32 has an integrated CAN controller and therefore doesn't necessarily need an external controller.
Some variants (ESP32-C5, ESP32-C6, ESP32-P4) have multiple CAN controllers - see [Multiple CAN Controllers](#multiple-can-controllers) below.
You only need to specify the RX and TX pins. Any GPIO will work.

```yaml
# Example configuration entry
canbus:
  - platform: esp32_can
    tx_pin: GPIOXX
    rx_pin: GPIOXX
    can_id: 4
    bit_rate: 50kbps
    on_frame:
      ...
```

## Configuration variables

- **rx_pin** (**Required**, [Pin](/guides/configuration-types#pin)): Receive pin.
- **tx_pin** (**Required**, [Pin](/guides/configuration-types#pin)): Transmit pin.
- **rx_queue_len** (*Optional*, int): Length of RX queue.
- **tx_queue_len** (*Optional*, int): Length of TX queue, 0 to disable.
- **tx_enqueue_timeout** (*Optional*, [Time](/guides/configuration-types#time)): Maximum time to wait when the TX queue is full before
  dropping the message (by default, this is set to the time it takes to send 10 CAN messages at the given bit rate).

- All other options from [Canbus](/components/canbus#config-canbus).

{{< anchor "esp32-can-bit-rate" >}}

The following table lists the bit rates supported by the component for ESP32 variants:

| bit_rate          | ESP32 | Other variants* |
| ----------------- | ----- | --------------- |
| 1KBPS             |       | x               |
| 5KBPS             |       | x               |
| 10KBPS            |       | x               |
| 12K5BPS           |       | x               |
| 16KBPS            |       | x               |
| 20KBPS            |       | x               |
| 25KBPS            | x     | x               |
| 31K25BPS          |       |                 |
| 33KBPS            |       |                 |
| 40KBPS            |       |                 |
| 50KBPS            | x     | x               |
| 80KBPS            |       |                 |
| 83K3BPS           |       |                 |
| 95KBPS            |       |                 |
| 100KBPS           | x     | x               |
| 125KBPS (Default) | x     | x               |
| 250KBPS           | x     | x               |
| 500KBPS           | x     | x               |
| 800KBPS           | x     | x               |
| 1000KBPS          | x     | x               |

Other variants: ESP32-C3, ESP32-C5, ESP32-C6, ESP32-H2, ESP32-P4, ESP32-S2, ESP32-S3

> [!NOTE]
> ESP32-C2 and ESP32-C61 do not have TWAI/CAN hardware and are not supported.

## Multiple CAN Controllers

Some ESP32 variants have multiple CAN (TWAI) controllers:

- **ESP32-C5**: 2 controllers
- **ESP32-C6**: 2 controllers
- **ESP32-P4**: 3 controllers

All other supported variants have a single controller. ESP32-C2 and ESP32-C61 do not have CAN hardware.

## Wiring options

5V CAN transceivers are cheap and generate compliant levels. If you power your
board with 5V this is the preferred option. R501 is important to reduce the 5V
logic level down to 3.3V, to avoid damaging the ESP32. You can alternatively
use a voltage divider here instead.

{{< img src="canbus_esp32_5v.png" alt="Image" class="align-center" >}}

If you prefer to only have a 3.3V power supply, special 3.3V CAN transceivers are available.

{{< img src="canbus_esp32_3v3.png" alt="Image" class="align-center" >}}

## See Also

- {{< docref "index/" >}}
- {{< apiref "canbus/canbus.h" "canbus/canbus.h" >}}
