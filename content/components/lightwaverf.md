---
description: "LightWaveRF Switch Lights"
title: "LightWaveRF"
params:
  seo:
    description: LightWaveRF Switch Lights
    image: brightness-medium.svg
---

The `LightWaveRF` light platform creates a module to dump and send commands to light switches

{{< img src="lightwaverf.jpg" alt="Image" width="40.0%" class="align-center" >}}

LightwaveRF switches are very common in UK automation. They allow control of lights, sockets, relays and more via RF remote or via a hub.
Using an inexpensive RF transmitter and receiver you can control your devices via ESPHome.

```yaml
# Example configuration entry

# Specify the two pins to connect the receiver and transmitter
lightwaverf:
  read_pin: GPIOXX
  write_pin: GPIOXX
```

Note: To gather the RAW codes from the remote, setup the `read_pin` and observe in the logs the printing of the codes.

## Configuration variables

- **read_pin** (**Required**, [Pin Schema](/guides/configuration-types#pin-schema)): The pin that the receiver is connected to
- **write_pin** (**Required**, [Pin Schema](/guides/configuration-types#pin-schema)): the pin that the transmitter is connected to

.. lightwaverf.send_raw:

## `lightwaverf.send_raw` Action

Send the raw data that has been captured via the dump system

```yaml
on_...:
  then:
    - lightwaverf.send_raw:
        code:  [0x04, 0x00, 0x00, 0x00, 0x0f, 0x03, 0x0d, 0x09, 0x08, 0x08]
        name: "Sofa"
        repeat: 1
```

### Configuration variables

- **name** (*Optional*, string): The name to give for the action
- **code** (**Required**, list hex): The raw dump in an array of hex
- **repeat** (*Optional*, int): The number of times the message will be repeated
- **inverted** (*Optional*, boolean): Send the signal inverted

## Compatible Hardware

The RF transmitters/receivers listed below have been confirmed to work with the current code base. If you discover others that work, please let us know!

Compatible transmitter:

- MX-FS-03V

Compatible receiver:

- RXB6

## See Also

- {{< docref "/components/light" >}}
