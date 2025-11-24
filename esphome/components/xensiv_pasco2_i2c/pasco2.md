---
title: "Infineon Xensiv PAS CO2 Sensor"
description: "Instructions for setting up Infineon Xensiv PAS CO2 sensors with ESPHome"
---

The `xensiv_pasco2_i2c` component allows you to use Infineon's Xensiv PAS CO2 sensor with ESPHome.
This photoacoustic spectroscopy-based CO2 sensor provides accurate carbon dioxide concentration
measurements from 0 to 32,000 ppm. The [I²C Bus](/components/i2c) is required to be set up in your
configuration for this sensor to work.

**Compatible Hardware:**
- [EVAL-CO2-5V-MINIBOARD](https://www.infineon.com/evaluation-board/EVAL-CO2-5V-MINIBOARD) (5V version)
- [EVAL-PASCO2-MINIBOARD](https://www.infineon.com/evaluation-board/EVAL-PASCO2-MINIBOARD) (12V version)

**Documentation:**
- [PAS CO2 5V Datasheet](https://www.infineon.com/document-promo/infineon-pasco2v15-datasheet-en_fcad9bf5-ca1e-4c2b-bf84-6041fe0219d8)
- [PAS CO2 12V Datasheet](https://www.infineon.com/document-promo/infineon-pasco2v01-datasheet-en_4fb77315-f724-408e-b491-a1702031c2c1)
- [5V Miniboard User Manual](https://www.infineon.com/document-promo/infineon-user-manual-eval-co2-5v-miniboard-usermanual-en_4635183f-758d-41ab-a6cb-88ff8d72f0bf)

```yaml
# Example configuration entry
sensor:
  - platform: xensiv_pasco2_i2c
    co2:
      name: "CO2"
```

## Configuration Variables

### Platform Configuration

- **id** (*Optional*, [ID](/guides/configuration-types#id)): Manually specify the ID used for code
  generation.
- **address** (*Optional*, int): The I²C address of the sensor. Defaults to `0x28`. The sensor has a
  fixed hardware address that cannot be changed.
- **interrupt_pin** (*Optional*, [Pin Schema](/guides/configuration-types#pin-schema)): The GPIO pin
  connected to the sensor's INT pin. Required for continuous operation mode. The sensor uses an
  active-low interrupt signal.
- **sensor_rate** (*Optional*, [Time](/guides/configuration-types#time)): The measurement interval
  for continuous mode. Valid range: 5 seconds to 4095 seconds. Defaults to `10s`. Accepts formats
  like `10s`, `1min`, `60s`.
- **operation_mode** (*Optional*, string): Sensor operation mode. One of `continuous`, `single_shot`.
  Defaults to `continuous`.
- **pressure_compensation** (*Optional*, pressure): Atmospheric pressure reference for improved
  accuracy. Accepts values with units like `1013.25hPa`, `101325Pa`, etc. If not specified, the
  sensor uses its default reference pressure of 1015 hPa.

### Sensor Configuration

- **co2** (**Required**): The CO2 sensor configuration.
  - **name** (**Required**, string): The name for the CO2 sensor.
  - All other options from [Sensor](/components/sensor).

## Operation Modes

### Continuous Mode (Default)

In continuous mode, the sensor automatically takes measurements at the specified `sensor_rate` and
triggers an interrupt when new data is ready. This is the recommended mode for most applications.

Requirements:
- `interrupt_pin` must be configured
- `operation_mode: continuous`

### Single-Shot Mode

In single-shot mode, measurements are only taken when explicitly triggered. This mode is useful for
battery-powered applications or when measurements are needed on-demand.

> [!NOTE]
> Each measurement may take up to about 2 seconds to complete.

## Hardware Differences

### 5V Version (EVAL-CO2-5V-MINIBOARD)

- Operating voltage: 5V
- Lower power consumption
- Recommended for embedded applications

### 12V Version (EVAL-PASCO2-MINIBOARD)

- Operating voltage: 12V
- Higher sensitivity
- Suitable for industrial applications

Both versions use the same I²C interface and are fully compatible with this component.

## Triggering Single-Shot Measurements

You can trigger a single-shot CO2 measurement using a lambda action:

```yaml
sensor:
  - platform: xensiv_pasco2_i2c
    id: pasco2
    co2:
      name: "CO2"

button:
  - platform: template
    name: "Measure CO2 Now"
    on_press:
      - lambda: |-
          id(pasco2).measure_now();
```

This is useful in `single_shot` operation mode or for triggering additional measurements in
continuous mode.

## Pressure Compensation

For improved accuracy, you can configure atmospheric pressure compensation. The sensor's CO2
measurement accuracy is affected by ambient air pressure:

```yaml
sensor:
  - platform: xensiv_pasco2_i2c
    co2:
      name: "CO2"
    pressure_compensation: 1013.25hPa  # Standard sea level pressure
```

You can also dynamically update the pressure compensation at runtime if you have a separate
pressure sensor.
You can update the pressure compensation in two ways:

**Option 1: Using a Lambda Method Call**

Call the `set_pressure_compensation()` method in a lambda action whenever your pressure sensor updates:

```yaml
sensor:
    - platform: bmp280
        pressure:
            name: "Atmospheric Pressure"
            id: atm_pressure
            on_value:
                - lambda: |-
                        // Update PASCO2 pressure compensation in Pascals
                        id(co2_sensor).set_pressure_compensation(x * 100);  // x is in hPa, convert to Pa
```

**Option 2: Linking Pressure Sensor Directly**

Set the `pressure_compensation` field to the pressure sensor's ID. ESPHome will automatically handle updates and unit conversion:

```yaml
sensor:
    - platform: xensiv_pasco2_i2c
        id: co2_sensor
        co2:
            name: "CO2"
        pressure_compensation: atm_pressure
```

> [!NOTE]
> When using a sensor ID for `pressure_compensation`, ESPHome will automatically convert the pressure
> value to Pascals and update the PASCO2 sensor whenever the pressure sensor reports a new value.

## Advanced Example

```yaml
i2c:
  sda: GPIOXX
  scl: GPIOXX

sensor:
  - platform: xensiv_pasco2_i2c
    id: co2_sensor
    co2:
      name: "CO2"
      filters:
        - sliding_window_moving_average:
            window_size: 5
            send_every: 1
    address: 0x28
    interrupt_pin: GPIOXX
    sensor_rate: 60s
    operation_mode: continuous
    pressure_compensation: 1013.25hPa

button:
  - platform: template
    name: "Measure CO2 Now"
    on_press:
      - lambda: |-
          id(co2_sensor).measure_now();
```

## Troubleshooting

### Sensor Not Responding

- Verify I²C address (default: `0x28`)
- Check I²C wiring (SDA, SCL, GND, VCC)
- Ensure correct voltage (5V or 12V depending on module)
- Check ESPHome logs for "I2C communication test failed" error

### No Interrupt Triggering

- Verify interrupt pin is correctly connected to the sensor's INT pin
- Check that `operation_mode` is set to `continuous`
- Ensure `interrupt_pin` is configured in the YAML
- The sensor uses an **active-low** interrupt signal (triggers on falling edge)

### Inaccurate Readings

- Allow 2-3 minutes warm-up time after power-on
- Ensure adequate ventilation around the sensor
- Check that `sensor_rate` is appropriate for your application (minimum 5s, maximum 4095s)
- Consider using `pressure_compensation` if not at sea level or if ambient pressure varies
- Use sensor filters like sliding window moving average to smooth readings

### Sensor Ready Check Fails

- This is normal during initial setup - the sensor needs time to stabilize after soft reset
- The component will continue operation and verify sensor readiness during first measurement
- Check logs for "Sensor is ready and operational" or "Sensor ready check inconclusive" messages

## See Also

- [Sensor Filters](/components/sensor#sensor-filters)
- [I²C Bus Component](/components/i2c)
- [Infineon Xensiv PAS CO2 Product Page (5V)](https://www.infineon.com/evaluation-board/EVAL-CO2-5V-MINIBOARD)
- [Infineon Xensiv PAS CO2 Product Page (12V)](https://www.infineon.com/evaluation-board/EVAL-PASCO2-MINIBOARD)
- {{< apiref "xensiv_pasco2_i2c/xensiv_pasco2_i2c.h" "xensiv_pasco2_i2c.h" >}}
