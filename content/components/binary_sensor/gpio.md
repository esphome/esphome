---
description: "Instructions for setting up GPIO binary sensors with ESPHome."
title: "GPIO Binary Sensor"
params:
  seo:
    description: Instructions for setting up GPIO binary sensors with ESPHome.
    image: gpio.svg
---

{{< anchor "gpio-binary-sensor" >}}

The GPIO Binary Sensor platform allows you to use any input pin on your
device as a binary sensor. By default, it uses hardware interrupts for
efficient state change detection with minimal CPU usage.

{{< img src="gpio-ui.png" alt="Image" width="80.0%" class="align-center" >}}

```yaml
# Example configuration entry
binary_sensor:
  - platform: gpio
    pin: D2
    name: "Living Room Window"
    device_class: window

# Example with interrupt configuration
binary_sensor:
  - platform: gpio
    pin: GPIO13
    name: "Motion Sensor"
    # use_interrupt: true  # Default - uses interrupts
    interrupt_type: RISING  # Only detect low-to-high transitions

# Example with polling mode (legacy behavior)
binary_sensor:
  - platform: gpio
    pin: GPIO14
    name: "Legacy Sensor"
    use_interrupt: false  # Use polling instead of interrupts

# Example with shared pin (automatic polling mode)
binary_sensor:
  - platform: gpio
    pin:
      number: GPIO15
      allow_other_uses: true  # Pin is shared with other components
    name: "Pump Status"
    # Interrupts will be automatically disabled for compatibility
```

## Configuration variables

- **pin** (**Required**, [Pin Schema](/guides/configuration-types#pin-schema)): The pin to monitor.
- **use_interrupt** (*Optional*, boolean): Use hardware interrupts instead of polling for better
  performance and lower CPU usage. Defaults to `true` for most platforms, but defaults to `false`
  for LibreTiny-based platforms (BK72xx, RTL87xx, LN882x) due to hardware limitations. Only supported
  on internal GPIO pins.

- **interrupt_type** (*Optional*, string): The type of interrupt to use. One of:

  - `ANY` (default): Trigger on any edge change (high to low or low to high)
  - `RISING`  : Trigger only on rising edge (low to high)
  - `FALLING`  : Trigger only on falling edge (high to low)

- All other options from [Binary Sensor](/components/binary_sensor#config-binary_sensor).

## Interrupt Mode vs Polling Mode

The GPIO binary sensor supports two modes of operation:

**Interrupt Mode** (default, `use_interrupt: true`  ):

- Uses hardware interrupts to detect pin state changes
- Extremely efficient - up to 98% lower CPU usage
- Updates are processed once per loop cycle (same as polling mode)
- Transitions shorter than the loop interval are ignored for backwards compatibility with polling mode
- Only processes when the pin actually changes state
- Recommended for most use cases

**Polling Mode** (`use_interrupt: false`  ):

- Continuously reads the pin state in the main loop
- Higher CPU usage but simpler implementation
- Transitions shorter than the loop interval are ignored
- Use only when interrupts are not supported or for compatibility

> [!NOTE]
> Interrupt mode is only available on internal GPIO pins. External GPIO
> expanders (like PCF8574) will automatically fall back to polling mode.
>
> LibreTiny-based platforms (BK72xx, RTL87xx, LN882x) default to polling mode
> due to hardware limitations with edge interrupts. You can explicitly enable
> interrupt mode if needed, but it may not work reliably on all pins.

> [!NOTE]
> When a pin is configured with `allow_other_uses: true` (for sharing with
> other components), interrupts are automatically disabled to prevent conflicts.
> This ensures compatibility with components like `duty_cycle` sensors that
> need to monitor pin state changes. The sensor will use polling mode instead.

## Activating internal pullups

If you're hooking up a button without an external pullup or see lots of ON/OFF events
in the log output all the time, this often means the GPIO pin is floating.

For these cases you need to manually enable the pull-up (or pull-down) resistors on the ESP,
you can do so with the [Pin Schema](/guides/configuration-types#pin-schema).

```yaml
binary_sensor:
  - platform: gpio
    pin:
      number: D2
      mode:
        input: true
        pullup: true
    name: ...
```

## Inverting Values

Use the `inverted` property of the [Pin Schema](/guides/configuration-types#pin-schema) to invert the binary
sensor:

```yaml
# Example configuration entry
binary_sensor:
  - platform: gpio
    pin:
      number: D2
      inverted: true
    name: ...
```

## Debouncing Values

Some binary sensors are a bit unstable and quickly transition between the ON and OFF state while
they're pressed. To fix this and debounce the signal, use the [binary sensor filters](/components/binary_sensor#binary_sensor-filters):

```yaml
# Example configuration entry
binary_sensor:
  - platform: gpio
    pin: D2
    name: ...
    filters:
      - delayed_on: 10ms
```

Above example will only make the signal go high if the button has stayed high for more than 10ms.
Alternatively, below configuration will make the binary sensor publish an ON value immediately, but
will wait 10ms before publishing an OFF value:

```yaml
# Example configuration entry
binary_sensor:
  - platform: gpio
    pin: D2
    name: ...
    filters:
      - delayed_off: 10ms
```

## See Also

- {{< docref "/components/binary_sensor" >}}
- [Pin Schema](/guides/configuration-types#pin-schema)
- {{< apiref "gpio/binary_sensor/gpio_binary_sensor.h" "gpio/binary_sensor/gpio_binary_sensor.h" >}}
