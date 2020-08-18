# EPEVER Solar charge controller component

## A fork of [esphome](https://github.com/esphome/esphome) adding support for montitoring a EPEVER controller

Tested using an EPEVER Tracer2210AN MPPT controller over modbus

## Hardware setup

I'm using a cheap RS 485 module connected to an ESP32

![RS 485 Modul](https://i.stack.imgur.com/plH1X.jpg)

See [How is this RS485 Module Working?](https://electronics.stackexchange.com/questions/244425/how-is-this-rs485-module-working) on stackexchange for more details

To connect the RS 485 Module to the conntroller I cut off one side of an ethernet cable and connected PIN 3 (or 4)  to A+, PIN 5 (or 6) to B+ and 7 (or 8 to Ground).  Ground is also connected to GND.
The interface with ESP32 is GPIO PIN 25 to TXD PIN 27 to RXD . 3.3V to VCC and GND to GND.
The pins used on the ESP32 side can be changed there is no special reason I chose 25/27 except that most of my ESP32 boards have them available

## Software setup

```
# Clone repo
git clone https://github.com/martgras/esphome.git

# Install ESPHome
cd esphome/
pip3 install -r requirements.txt -r requirements_test.txt
pip3 install -e .

esphome <path to your config.yaml> run

```

[Sample config:](https://github.com/martgras/esphome/blob/epever/esphome/components/epsolar/testconfig/epsolar-test.yaml)


## Current status
This is still work in progress. I have implemented reading most of the registers and included an option to sync the RTC clock of the controller to the ESP32 device time.  Make sure to setup an NTP server in you ESPHome configuration if your are using `  sync_rtc: true`

(Nearly) all registers for the EPEVER device are now implemented with default value in in [sensors.py](https://github.com/martgras/esphome/blob/epever/esphome/components/epsolar/sensor.py)

The different registers of the controller are mapped to sensors (float values) or binary sensors

Additional sensors can be defined in the sensors and binary_sensors section of the configuration (see sample config)

Note: due to limited stack size the device might crash if all registers are enabled See https://github.com/esphome/issues/issues/855

### Format

```python
    cv.Optional(CONF_REMOTE_BATTERY_TEMPERATURE): modbus_sensor_schema('read_input_registers', 0x311A, 0x1,0xFFFFF 'S_SINGLE', 0.01, UNIT_CELSIUS, ICON_THERMOMETER, 1),
    cv.Optional(CONF_BATTERY_STATUS): modbus_sensor_schema('read_input_registers', 0x3200, 0x0,oxFFFFF 'U_SINGLE', 1.0, UNIT_EMPTY, ICON_BATTERY, 0),
    cv.Optional(CONF_BATTERY_RESISTENCE_ERROR): modbus_binarysensor_schema('read_input_registers', 0x3200,0x0, 256),
```

Define an additional register in YAML
```yaml
    sensors:
      - id: length_of_night
        address: 0x9065
        offset: 0
        bitmask: default value is 0xFFFFF # some values are packed in a single response word. Bitmask can be used to extract the relevant parts
        name: 'Length of night'
        modbus_functioncode: read_holding_registers
        value_type: U_SINGLE
        scale_factor: 1.0
```

modbus_sensor_schema extends the sensors schema and adds these parameters:
  - type of register (read_input_registers ,read_discrete_inputs, read_input_registers )
  - start address of the first register in a range
  - offset from start address:  The modbus protocol allows reading a range of registers in one request. The component calculates the size of the range based on offset and size of the value type
  - type of value:
    - U_SINGLE (unsigned float from 1 register =16bit
    - S_SINGLE (signed float from one register)
    - U_DOUBLE (unsigned float from 2 registers = 32bit
    - S_DOUBLE
  - scale factor:  most values are returned as 16 bit integer values. To get the actual value the raw value is usually divided by 100.
  For example, if the raw data returned for input voltage is 1350 the actual valus is 13.5 (V). The scale_factor parameter is used for the conversion

For binary sensors, the parameters are
 - type of register
 - start address of the first register in a range
 - offset from start address
 - bitmask: some values are packed in a single response word. bitmask is used to convert to a bool value.
 For example, bit 8 of the register 0x3200 indicates an battery error. Therefore, the bitmask is 256.  The operation is `result = (raw value & bitmask != 0)`

See the MODBUS Specification: https://www.developpez.net/forums/attachments/p196506d1451307310/systemes/autres-systemes/automation/probleme-com-modbus-pl7-pro/controllerprotocolv2.3.pdf/ for details about the registers

## TODO

- [x] Move configuration from python to YAML
- [ ] After the last refactoring it might make sense to create seperate modbus component and derive EPSolar from that.  However not sure what the best implementation would be
- [x] Add missing registers (903F ... )
- [x] Add inverted option for binary sensors:~~ ` if (inverted ) { result =  (raw value & bitmask == 0) }` can be done already with a filter
- [x] Make caching configurable? Right now, values are only published if the have changed from the previous publish. Saves quite a but of traffic I'll keep it enabled


