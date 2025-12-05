---
description: "Instructions for setting up built-in analog voltage sensors."
title: "Analog To Digital Sensor"
params:
  seo:
    description: Instructions for setting up built-in analog voltage sensors.
    image: flash.svg
---

The Analog To Digital (`adc`  ) Sensor allows you to use the built-in
ADC in your device to measure a voltage on certain pins.

- ESP8266: Only pin A0 (GPIO17) can be used.
- ESP32: Available pins vary by variant, see [ESP32 pins and Hardware Details](#adc-esp32_pins).
- RP2040: GPIO26 through GPIO29 can be used.
- nRF52840: AIN0 through AIN7, VDD, VDDHDIV5 can be used.

{{< img src="adc-ui.png" alt="Image" width="80.0%" class="align-center" >}}

```yaml
# Example configuration entry
sensor:
  - platform: adc
    pin: GPIOXX
    name: "Living Room Brightness"
    update_interval: 60s
```

## Configuration variables

- **pin** (**Required**, [Pin](/guides/configuration-types#pin)): The pin to measure the voltage on.
  Or on the ESP8266 or Raspberry Pi Pico it could alternatively be set to `VCC`, see [Measuring VCC](#adc-vcc).

- **attenuation** (*Optional*): Only on ESP32. Specify the ADC
  attenuation to use. See [ESP32 Attenuation](#adc-esp32_attenuation). Defaults to `0db`.

- **raw** (*Optional*): Allows to read the raw ADC output without any conversion or calibration. See [Different ESP32-ADC behavior since 2021.11](#adc-raw). Defaults to `false`.
- **samples** (*Optional*): The amount of ADC readings to take per sensor update. On the ESP32 this value is ignored if `attenuation` is set to `auto`. Defaults to `1`.
- **sampling_mode** (*Optional*): Sampling method to use when multiple samples are taken.

  - `avg` average of all samples (**Default**)
  - `min` minimal value from all samples
  - `max` maximal value from all samples

- **update_interval** (*Optional*, [Time](/guides/configuration-types#time)): The interval
  to check the sensor. Defaults to `60s`.

- All other options from [Sensor](/components/sensor).

> [!NOTE]
> This component prints the voltage as seen by the chip pin. On the ESP8266, this is always 0.0V to 1.0V
> Some development boards like the Wemos D1 mini include external voltage divider circuitry to scale down
> a 3.3V input signal to the chip-internal 1.0V. If your board has this circuitry, add a multiply filter to
> get correct values:
>
> ```yaml
> sensor:
>   - platform: adc
>     # ...
>     filters:
>       - multiply: 3.3
> ```

{{< anchor "adc-esp32_attenuation" >}}

## ESP32 Attenuation

On the ESP32 the voltage measured with the ADC caps out at ~1.1V by default as the sensing range (attenuation of the ADC) is set to `0db` by default.
Measuring higher voltages requires setting `attenuation` to one of the following values: `0db`, `2.5db`, `6db`, `12db`.
There's more information [at the manufacturer's website](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32/api-reference/peripherals/adc.html#_CPPv425adc1_config_channel_atten14adc1_channel_t11adc_atten_t).

To simplify this, we provide the setting `attenuation: auto` for an automatic/seamless transition among scales. [Our implementation](https://github.com/esphome/esphome/blob/dev/esphome/components/adc/adc_sensor_esp32.cpp) combines all available ranges to allow the best resolution without having to compromise on a specific attenuation.

> [!NOTE]
> In our tests, the usable ADC range was from ~0.075V to ~3.12V (with the `attenuation: auto` setting), and anything outside that range capped out at either end.
> Even though the measurements are calibrated, the range *limits* are variable among chips due to differences in the internal voltage reference.

{{< anchor "adc-esp32_pins" >}}

## ESP32 pins and Hardware Details

| Variant  | ADC1            | ADC2                                                  |
| -------- | --------------- | ----------------------------------------------------- |
| ESP32    | GPIO32 - GPIO39 | GPIO0, GPIO2, GPIO4, GPIO12 - GPIO15, GPIO25 - GPIO27 |
| ESP32-C2 | GPIO0 - GPIO4   | GPIO5                                                 |
| ESP32-C3 | GPIO0 - GPIO4 | GPIO5 |
| ESP32-C5 | GPIO1 - GPIO6 | no `ADC2`                                             |
| ESP32-C6 | GPIO0 - GPIO6 | no `ADC2`                                             |
| ESP32-C61 | GPIO1, GPIO3 - GPIO5 | no `ADC2`                                      |
| ESP32-H2 | GPIO1 - GPIO5 | no `ADC2`                                             |
| ESP32-S2 | GPIO1 - GPIO10 | GPIO11 - GPIO20 |
| ESP32-S3 | GPIO1 - GPIO10 | GPIO11 - GPIO20 |
| ESP32-P4 | GPIO16 - GPIO23 | GPIO49 - GPIO54 |

Different ESP32 variants use different ADC calibration methods:

- Original ESP32 (non-variant) & ESP32-S2: Use line-fitting calibration
- ESP32-C3, ESP32-C5, ESP32-C6, ESP32-C61, ESP32-H2, ESP32-S3 & ESP32-P4: Use curve-fitting calibration

This is handled automatically by the code, but it's worth noting if you're debugging ADC readings or need to understand the calibration process.

> [!WARNING]
> On ESP32-C5, GPIO2 is a strapping pin used during boot. While it can be used as an ADC input, avoid connecting circuits that might interfere with the boot process.

{{< anchor "adc-raw" >}}

## Different ESP32-ADC behavior since 2021.11

The ADC output reads voltage very accurately since 2021.11 where manufacturer calibration was incorporated. Before this every ESP32 would read different voltages and be largely inaccurate/nonlinear. Users with a manually calibrated setup are encouraged to check their installations to ensure proper output.
For users that don't need a precise voltage reading, the "raw" output option allows to have the raw ADC values (0-4095 for ESP32) prior to manufacturer calibration. It is possible to get the old uncalibrated measurements with a filter multiplier:

```yaml
# To replicate old uncalibrated output, set raw:true and keep only one of the multiplier lines.
raw: true
filters:
  - multiply: 0.00026862 # 1.1/4095, for attenuation 0db
  - multiply: 0.00036630 # 1.5/4095, for attenuation 2.5db
  - multiply: 0.00053724 # 2.2/4095, for attenuation 6db
  - multiply: 0.00095238 # 3.9/4095, for attenuation 12db
  # your existing filters would go here
```

Note we don't recommend this method as it will change between chips, and newer ESP32 modules have different ranges (i.e. 0-8191); it is better to use the new calibrated voltages and update any existing filters accordingly.

{{< anchor "adc-vcc" >}}

## Measuring VCC

The following configuration block adds the sensor reflecting VCC on a supported hardware platform.
Please see specific sections below of what voltage is actually measured.

```yaml
sensor:
  - platform: adc
    pin: VCC
    name: "VCC Voltage"
```

### On ESP8266

On the ESP8266 you can even measure the voltage the *chip is getting*. This can be useful in situations
where you want to shut down the chip if the voltage is low when using a battery.

To measure the VCC voltage, set `pin:` to `VCC` and make sure nothing is connected to the `A0` pin.

> [!NOTE]
> To avoid confusion: It measures the voltage at the chip, and not at the VCC pin of the board. It should usually be around 3.3V.

### On Raspberry Pi Pico

On the Raspberry Pi Pico and Pico W, setting `pin:` to `VCC` allows you to measure the VSYS voltage through ADC3 (GPIO29).

The reading will reflect either:

- Direct power supply voltage when powered through the VSYS pin
- USB voltage (VBUS) minus diode drop when powered via USB

Our experiments indicate the diode drop being ~0.1V for Pico and ~0.25V for Pico W; you can use sensor filters to adjust the final value.

> [!NOTE]
> On Raspberry Pi Pico W the ADC GPIO29 pin for VSYS is shared with WiFi chip, so attempting to use it explicitly will likely hang the WiFi connection.
> It is recommended to use `VCC` as ADC pin in that case.

## RP2040 Internal Core Temperature

The RP2040 has an internal temperature sensor that can be used to measure the core temperature. This sensor is not available on the GPIO pins, but is available on the internal ADC.
The below code is how you can access the temperature and expose as a sensor. The filter values are taken from the RP2040 datasheet to calculate Voltage to Celcius.

```yaml
sensor:
  - platform: adc
    pin: TEMPERATURE
    name: "Core Temperature"
    unit_of_measurement: "°C"
    filters:
      - lambda: return 27 - (x - 0.706f) / 0.001721f;
```

## Multiple ADC Sensors

You can only use as many ADC sensors as your device can support. The ESP8266 only has one ADC and can only handle one sensor at a time. For example, on the ESP8266, you can measure the value of an analog pin (A0 on ESP8266) or VCC (see above) but NOT both simultaneously. Using both at the same time will result in incorrect sensor values.

## Measuring battery voltage on the Firebeetle ESP32-E

This board has a internal voltage divider and the battery voltage can easily be measured like this using 12dB attenuation
on GPIO34.

```yaml
- platform: adc
  name: "Battery voltage"
  pin: GPIO34
  accuracy_decimals: 2
  update_interval: 60s
  attenuation: 12dB
  samples: 10
  filters:
    - multiply: 2.0  # The voltage divider requires us to multiply by 2
```

This works on SKU:DFR0654. For more information see: [manufacturer's website](https://wiki.dfrobot.com/FireBeetle_Board_ESP32_E_SKU_DFR0654).

## See Also

- [Sensor Filters](/components/sensor#sensor-filters)
- {{< docref "ads1115/" >}}
- {{< docref "max6675/" >}}
- {{< apiref "adc/adc_sensor.h" "adc/adc_sensor.h" >}}
