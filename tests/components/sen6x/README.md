# SEN6x Component Tests

This directory contains comprehensive test configurations for the SEN6x sensor platform, covering all variants and their supported sensors.

## Test Coverage

### SEN6x Variants Tested

1. **SEN62** - PM, RH & T only
   - Test file: `sen62_test.yaml`
   - Platform test: `test.sen62.esp8266-ard.yaml`

2. **SEN63C** - PM, RH & T, CO2
   - Test file: `sen63c_test.yaml`
   - Platform test: `test.sen63c.esp32-idf.yaml`

3. **SEN65** - PM, RH & T, VOC, NOx
   - Test file: `sen65_test.yaml`
   - Platform test: `test.sen65.esp8266-ard.yaml`

4. **SEN66** - PM, RH & T, VOC, NOx, CO2
   - Test file: `sen66_test.yaml`
   - Platform test: `test.sen66.rp2040-ard.yaml`

5. **SEN68** - PM, RH & T, VOC, NOx, HCHO
   - Test file: `sen68_test.yaml`
   - Platform test: `test.sen68.esp32-idf.yaml`

6. **SEN69C** - PM, RH & T, VOC, NOx, HCHO, CO2 (full featured)
   - Test file: `sen69c_test.yaml`
   - Platform test: `test.sen69c.esp8266-ard.yaml`

### Sensors Tested

#### Common Sensors (All Variants)
- **PM1.0, PM2.5, PM4.0, PM10.0** - Particulate matter concentrations
- **Temperature** - Ambient temperature (°C)
- **Humidity** - Relative humidity (%)

#### Gas Sensors (Variant-Specific)
- **VOC** - Volatile Organic Compounds index (SEN65, SEN66, SEN68, SEN69C)
- **NOx** - Nitrogen Oxides index (SEN65, SEN66, SEN68, SEN69C)
- **CO2** - Carbon Dioxide (ppm) (SEN63C, SEN66, SEN69C)
- **HCHO** - Formaldehyde (ppb) (SEN68, SEN69C) - Now configured as `formaldehyde:`

### Configuration Options Tested

- **Algorithm Tuning**: VOC and NOx algorithm parameters
- **Temperature Compensation**: Offset and slope adjustments
- **Auto Cleaning Interval**: Fan cleaning cycle configuration
- **Acceleration Mode**: RHT response speed (low, medium, high)
- **Baseline Storage**: VOC baseline persistence
- **I2C Address**: Default 0x6B

### Platform Coverage

- **ESP8266 (Arduino)**: Multiple variants tested
- **ESP32 (IDF)**: Multiple variants tested
- **RP2040 (Arduino)**: Single variant tested

## Running Tests

To run tests for a specific variant:

```bash
# Test SEN62 on ESP8266
esphome run test.sen62.esp8266-ard.yaml

# Test SEN69C on ESP32
esphome run test.sen69c.esp32-idf.yaml

# Test SEN66 on RP2040
esphome run test.sen66.rp2040-ard.yaml
```

## Configuration Example

```yaml
sensor:
  - platform: sen6x
    temperature:
      name: Temperature
    humidity:
      name: Humidity
    pm_2_5:
      name: PM2.5
    formaldehyde:
      name: Formaldehyde
      accuracy_decimals: 1
    address: 0x6B
```

## Test Validation

Each test configuration validates:

1. **Sensor Compatibility**: Only appropriate sensors are configured for each variant
2. **Algorithm Tuning**: VOC and NOx tuning parameters where applicable
3. **Accuracy Decimals**: Proper precision settings for each sensor type
4. **Platform Integration**: I2C bus configuration and device addressing
5. **Feature Support**: All supported features are exercised in at least one test

## Expected Behavior

- Component should successfully compile for all test configurations
- Sensor validation should reject incompatible sensor combinations
- All supported sensors should publish readings with correct units
- Algorithm tuning parameters should be properly applied
- Baseline storage should work for VOC-enabled variants
