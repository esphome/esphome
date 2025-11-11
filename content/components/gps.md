---
description: "Instructions for setting up GPS component in ESPHome."
title: "GPS Component"
params:
  seo:
    description: Instructions for setting up GPS component in ESPHome.
    image: crosshairs-gps.svg
---

The `gps` component allows you to connect GPS modules to your ESPHome project.
Any GPS module that uses the standardized NMEA communication protocol will work.

{{< img src="gps-full.jpg" alt="Image" caption="GPS Module. Image by [Adafruit](https://www.adafruit.com)" width="50.0%" class="align-center" >}}

For this component to work you need to have set up a [UART bus](/components/uart)
in your configuration - only the RX pin should be necessary.

```yaml
# Example configuration entry

# Declare GPS module
gps:
  latitude:
    name: "Latitude"
  longitude:
    name: "Longitude"
  altitude:
    name: "Altitude"

# GPS as time source
time:
  - platform: gps
```

The component is split up in platforms, by defining the GPS module
(as seen above).

In addition to retrieving GPS position data, the module can also be used as a
time platform to get the current date and time via the very accurate GPS clocks
without a network connection.

See {{< docref "time/gps" >}} for config options for the GPS time source.

## Configuration variables

- **latitude** (*Optional*): Include the Latitude as a sensor

  - All options from [Sensor](/components/sensor).

- **longitude** (*Optional*): Include the Longitude as a sensor

  - All options from [Sensor](/components/sensor).

- **speed** (*Optional*): Include the measured speed as a sensor

  - All options from [Sensor](/components/sensor).

- **course** (*Optional*): Include the measured course as a sensor

  - All options from [Sensor](/components/sensor).

- **altitude** (*Optional*): Include the measured altitude as a sensor

  - All options from [Sensor](/components/sensor).

- **satellites** (*Optional*): Include the number of tracking satellites being used as a sensor

  - All options from [Sensor](/components/sensor).

- **hdop** (*Optional*): Include the measured HDOP (Horizontal Dilution Of Precision) as a sensor

  - All options from [Sensor](/components/sensor).

- **update_interval** (*Optional*, [Time](/guides/configuration-types#time)): The interval of sensor updates. Defaults to `20s`.

## See Also

- [Sensor Filters](/components/sensor#sensor-filters)
- [TinyGPS++ library](https://github.com/esphome/TinyGPSPlus)
- {{< apiref "gps/gps.h" "gps/gps.h" >}}
