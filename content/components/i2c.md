---
description: "Instructions for setting up the I²C bus to communicate with 2-wire devices in ESPHome"
title: "I²C Bus"
params:
  seo:
    description: Instructions for setting up the I²C bus to communicate with 2-wire devices in ESPHome
    image: i2c.svg
---

{{< anchor "i2c" >}}

This component sets up the I²C bus for your ESP32 or ESP8266. In order for these components
to work correctly, you need to define the I²C bus in your configuration. Please note the ESP
will enable its internal 10kΩ pullup resistors for these pins, so you usually don't need to
put on external ones. You can use multiple devices on one I²C bus as each device is given a
unique address for communicating between it and the ESP. You can do this by hopping
wires from the two lines (SDA and SCL) from each device board to the next device board or by
connecting the wires from each device back to the two I²C pins on the ESP.

```yaml
# Example configuration entry for ESP32
i2c:
  sda: GPIOXX
  scl: GPIOXX
  scan: true
  id: bus_a
```

## Configuration variables

- **sda** (*Optional*, [Pin](/guides/configuration-types#pin)): The pin for the data line of the I²C bus.
  Defaults to the default of your board (usually GPIO21 for ESP32 and GPIO4 for ESP8266).

- **scl** (*Optional*, [Pin](/guides/configuration-types#pin)): The pin for the clock line of the I²C bus.
  Defaults to the default of your board (usually GPIO22 for ESP32 and
  GPIO5 for ESP8266).

- **scan** (*Optional*, boolean): If ESPHome should do a search of the I²C address space on startup.
  Defaults to `true`.

- **frequency** (*Optional*, float): Set the frequency the I²C bus should operate on.
  Defaults to `50kHz`. Values are `10kHz`, `50kHz`, `100kHz`, `200kHz`, ... `800kHz`

- **timeout** (*Optional*, [Time](/guides/configuration-types#time)): Set the I²C bus timeout.
  Defaults to the framework defaults (`100us` on `esp32` with `esp-idf`, `50ms` on `esp32` with `Arduino`,
  `1s` on `esp8266` and `1s` on `rp2040`  ). Maximum on `esp-idf` is 13ms.

- **id** (*Optional*, [ID](/guides/configuration-types#id)): Manually specify the ID for this I²C bus if you need multiple I²C buses.

> [!NOTE]
> If the device can support multiple I²C buses these buses need to be defined as below and sensors need to be setup specifying the correct bus:
>
> ```yaml
> # Example configuration entry
> i2c:
>   - id: bus_a
>     sda: GPIOXX
>     scl: GPIOXX
>     scan: true
>   - id: bus_b
>     sda: GPIOXX
>     scl: GPIOXX
>     scan: true
> # Sensors should be specified as follows
> sensor:
>   - platform: bme680
>     i2c_id: bus_b
>     address: 0x76
> # ...
> ```

For I²C multiplexing see {{< docref "/components/tca9548a" >}}.

## See Also

- {{< docref "/components/tca9548a" >}}
- {{< docref "/components/i2c_device" >}}
- {{< apiref "i2c/i2c.h" "i2c/i2c.h" >}}
