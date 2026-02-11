## Summary

This PR adds support for the **BME680 environmental sensor** in ESPHome.

The BME680 is a digital 4-in-1 sensor that measures:
- Temperature (°C)
- Pressure (hPa)
- Humidity (%)
- Gas Resistance (VOC/IAQ)

## Features

- Temperature sensor with 0.1°C resolution
- Pressure sensor with 0.18 Pa resolution
- Humidity sensor with 0.008% resolution
- Gas resistance sensor for VOC/IAQ detection
- I2C interface support
- Configurable address (0x76/0x77)
- Automatic heater control for gas measurement

## Usage Example

```yaml
sensor:
  - platform: bme680
    temperature:
      name: "BME680 Temperature"
    pressure:
      name: "BME680 Pressure"
    humidity:
      name: "BME680 Humidity"
    gas_resistance:
      name: "BME680 Gas Resistance"
    address: 0x77
    update_interval: 60s

i2c:
  sda: GPIO21
  scl: GPIO22
```

## Files Changed

```
components/bme680/
├── __init__.py         # Component initialization
├── bme680.h            # Header file
├── bme680.cpp          # Implementation
├── sensor.py           # Sensor configuration
└── index.rst           # Documentation
```

## Testing

Tested with:
- ESP32 DevKit V1
- BME680 breakout board (0x77 address)
- Room temperature/humidity readings
- Pressure readings (~1013 hPa)
- Gas resistance (varies with air quality)

## References

- BME680 Datasheet: https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bme680-ds001.pdf
- BME68x Sensor API: https://github.com/BoschSensortec/BME68x_Sensor_API

---

This is my first ESPHome contribution. Happy to make any requested changes!
