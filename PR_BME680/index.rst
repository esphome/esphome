# BME680 Temperature, Pressure, Humidity, and Gas Resistance sensor

- [BME680 Datasheet](https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bme680-ds001.pdf)

## Usage

```yaml
# Example configuration.yaml
sensor:
  - platform: bme680
    temperature:
      name: "BME680 Temperature"
      id: bme680_temperature
    pressure:
      name: "BME680 Pressure"
      id: bme680_pressure
    humidity:
      name: "BME680 Humidity"
      id: bme680_humidity
    gas_resistance:
      name: "BME680 Gas Resistance"
      id: bme680_gas
    address: 0x77
    update_interval: 60s

i2c:
  sda: GPIO21
  scl: GPIO22
```

## Configuration variables

- **temperature** (*Optional*): The temperature sensor.
  - **name** (*Optional*, string): The name for the temperature sensor.
  - **id** (*Optional*, ID): Set the ID of this sensor for use in lambdas.
  - All other options from [Sensor](https://esphome.io/components/sensor/#configuration-variables).

- **pressure** (*Optional*): The pressure sensor.
  - **name** (*Optional*, string): The name for the pressure sensor.
  - **id** (*Optional*, ID): Set the ID of this sensor for use in lambdas.
  - All other options from [Sensor](https://esphome.io/components/sensor/#configuration-variables).

- **humidity** (*Optional*): The humidity sensor.
  - **name** (*Optional*, string): The name for the humidity sensor.
  - **id** (*Optional*, ID): Set the ID of this sensor for use in lambdas.
  - All other options from [Sensor](https://esphome.io/components/sensor/#configuration-variables).

- **gas_resistance** (*Optional*): The gas resistance sensor.
  - **name** (*Optional*, string): The name for the gas resistance sensor.
  - **id** (*Optional*, ID): Set the ID of this sensor for use in lambdas.
  - All other options from [Sensor](https://esphome.io/components/sensor/#configuration-variables).

- **address** (*Optional*, int): The I2C address of the BME680. Defaults to `0x77`.
- **update_interval** (*Optional*, [Time](https://esphome.io/faq/application timings)): The interval to check the sensor. Defaults to `60s`.

## Platforms

## Notes

This component is a community contribution and is not officially supported by Bosch.
For issues, please open an issue on the ESPHome GitHub repository.
