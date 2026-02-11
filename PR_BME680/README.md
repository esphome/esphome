# BME680 Sensor Component for ESPHome

## Overview

This PR adds support for the BME680 environmental sensor in ESPHome.

The BME680 is a digital 4-in-1 sensor that measures:
- Temperature
- Pressure
- Humidity
- Gas Resistance (VOC)

## Files Changed

```
components/bme680/
├── __init__.py          # Component initialization
├── bme680.h             # Header file
├── bme680.cpp           # Implementation
├── sensor.py            # Sensor platform configuration
└── index.rst            # Documentation
```

## Usage Example

```yaml
# Example configuration
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

## Hardware Connection

| BME680 | ESP32 |
|--------|-------|
| VCC    | 3.3V  |
| GND    | GND   |
| SDA    | GPIO21|
| SCL    | GPIO22|

## Testing

After flashing, the sensor should report:
- Temperature: ~25°C (room temperature)
- Pressure: ~1013 hPa (atmospheric pressure)
- Humidity: ~50% (room humidity)
- Gas Resistance: Variable (depends on air quality)

## Notes

- Default I2C address: 0x77 (can be changed to 0x76 by connecting SDO to GND)
- The sensor requires a warm-up period for accurate gas readings
- Gas resistance reading may take 5-10 minutes to stabilize

## References

- [BME680 Datasheet](https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bme680-ds001.pdf)
- [ESPHome Documentation](https://esphome.io/)
- [BME680 Library](https://github.com/BoschSensortec/BME68x_Sensor_API)

---

## Checklist

- [x] Component code implemented
- [x] Configuration schema defined
- [x] Documentation written
- [x] Tested with hardware
- [x] Follows ESPHome coding style
