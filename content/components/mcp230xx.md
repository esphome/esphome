---
description: "Instructions for setting up MCP23008, MCP23016 or MCP23017 digital port expander in ESPHome."
title: "MCP230xx I/O Expander"
params:
  seo:
    description: Instructions for setting up MCP23008, MCP23016 or MCP23017 digital port expander in ESPHome.
    image: mcp230xx.svg
---

The Microchip MCP230xx series of general purpose, parallel I/O expansion for I²C bus applications.

**Supported Variants :**

- [MCP23008 Component](#mcp23008-label)
- [MCP23016 Component](#mcp23016-label)
- [MCP23017 Component](#mcp23017-label)

{{< anchor "mcp23008-label" >}}

## MCP23008 Component

The MCP23008 component ([datasheet](https://ww1.microchip.com/downloads/aemDocuments/documents/APID/ProductDocuments/DataSheets/MCP23008-MCP23S08-Data-Sheet-DS20001919.pdf),
[Adafruit](https://www.adafruit.com/product/593)) has 8 GPIOs that can be configured independently.

```yaml
# Example configuration entry
mcp23008:
  - id: 'mcp23008_hub'
    address: 0x20

# Individual outputs
switch:
  - platform: gpio
    name: "MCP23008 Pin #0"
    pin:
      mcp23xxx: mcp23008_hub
      # Use pin number 0
      number: 0
      mode:
        output: true
      inverted: false

# Individual inputs
binary_sensor:
  - platform: gpio
    name: "MCP23008 Pin #1"
    pin:
      mcp23xxx: mcp23008_hub
      # Use pin number 1
      number: 1
      # One of INPUT or INPUT_PULLUP
      mode:
        input: true
      inverted: false
```

### Configuration variables

- **id** (**Required**, [ID](/guides/configuration-types#id)): The id to use for this MCP23008 component.
- **address** (*Optional*, int): The I²C address of the driver.
  Defaults to `0x20`.

- **open_drain_interrupt** (*Optional*, boolean): Configure the interrupt pin to open-drain mode.
  Useful when the MCP23008's power supply is greater than 3.3 volts. Note that this pin
  will require a pull-up resistor (to 3.3 volts) when this mode is enabled.

### Pin configuration variables

- **mcp23xxx** (**Required**, [ID](/guides/configuration-types#id)): The id of the MCP23008 component.
- **interrupt** (*Optional*): Set this pin to trigger the INT pin on the component. Can be one of `CHANGE`, `RISING`, `FALLING`.
- **number** (**Required**, int): The pin number.
- **inverted** (*Optional*, boolean): If all read and written values
  should be treated as inverted. Defaults to `false`.

- **mode** (*Optional*, string): A pin mode to set for the pin at. One of `INPUT` or `OUTPUT`.

{{< anchor "mcp23016-label" >}}

## MCP23016 Component

The MCP23016 component ([datasheet](https://ww1.microchip.com/downloads/aemDocuments/documents/APID/ProductDocuments/DataSheets/20090C.pdf))
has 16 GPIOs and can be configured the same way than the other variants.

> [!NOTE]
> The 'INPUT_PULLUP' mode is not supported on this device.

```yaml
# Example configuration entry
mcp23016:
  - id: 'mcp23016_hub'
    address: 0x20

# Individual outputs
switch:
  - platform: gpio
    name: "MCP23016 Pin #0"
    pin:
      mcp23016: mcp23016_hub
      # Use pin number 0
      number: 0
      mode:
        output: true
      inverted: false

# Individual inputs
binary_sensor:
  - platform: gpio
    name: "MCP23016 Pin #1"
    pin:
      mcp23016: mcp23016_hub
      # Use pin number 1
      number: 1
      mode:
        input: true
      inverted: false
```

### Configuration variables

- **id** (**Required**, [ID](/guides/configuration-types#id)): The id to use for this MCP23016 component.
- **address** (*Optional*, int): The I²C address of the driver.
  Defaults to `0x20`.

### Pin configuration variables

- **mcp23xxx** (**Required**, [ID](/guides/configuration-types#id)): The id of the MCP23016 component.
- All other options from [Pin Schema](/guides/configuration-types#pin-schema)

{{< anchor "mcp23017-label" >}}

## MCP23017 Component

The MCP23017 component allows you to use MCP23017 I/O expanders
([datasheet](https://ww1.microchip.com/downloads/aemDocuments/documents/APID/ProductDocuments/DataSheets/MCP23017-Data-Sheet-DS20001952.pdf),
[Adafruit](https://www.adafruit.com/product/732)) in ESPHome.
It uses the [I²C Bus](/components/i2c) for communication.

Once configured, you can use any of the 16 pins as
pins for your projects. Within ESPHome they emulate a real internal GPIO pin
and can therefore be used with many of ESPHome's components such as the GPIO
binary sensor or GPIO switch.

GPIO pins in the datasheet are labelled A0 to A7 and B0 to B7, these are mapped
consecutively in this component to numbers from 0 to 15.

```yaml
# Example configuration entry
mcp23017:
  - id: 'mcp23017_hub'
    address: 0x20

# Individual outputs
switch:
  - platform: gpio
    name: "MCP23017 Pin A0"
    pin:
      mcp23xxx: mcp23017_hub
      # Use pin A0
      number: 0
      mode:
        output: true
      inverted: false

# Individual inputs
binary_sensor:
  - platform: gpio
    name: "MCP23017 Pin B7"
    pin:
      mcp23xxx: mcp23017_hub
      # Use pin B7
      number: 15
      # One of INPUT or INPUT_PULLUP
      mode:
        input: true
        pullup: true
      inverted: false
```

### Configuration variables

- **id** (**Required**, [ID](/guides/configuration-types#id)): The id to use for this MCP23017 component.
- **address** (*Optional*, int): The I²C address of the driver.
  Defaults to `0x20`.

- **open_drain_interrupt** (*Optional*, boolean): Configure interrupt pins to open-drain mode.
  Useful when the MCP23017's power supply is greater than 3.3 volts. Note that these pins
  will require pull-up resistors (to 3.3 volts) when this mode is enabled.

### Pin configuration variables

- **mcp23xxx** (**Required**, [ID](/guides/configuration-types#id)): The id of the MCP23017 component.
- **interrupt** (*Optional*): Set this pin to trigger the port INT pin on the component. Can be one of `CHANGE`, `RISING`, `FALLING`.
- All other options from [Pin Schema](/guides/configuration-types#pin-schema)

## See Also

- [I²C Bus](/components/i2c)
- {{< docref "switch/gpio" >}}
- {{< docref "binary_sensor/gpio" >}}
- {{< apiref "API Reference (MCP23008)" "mcp23008/mcp23008.h" >}}
- {{< apiref "API Reference (MCP23016)" "mcp23016/mcp23016.h" >}}
- {{< apiref "API Reference (MCP23017)" "mcp23017/mcp23017.h" >}}
