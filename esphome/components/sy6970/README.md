# Overview
This component supports the sy6970 BMS chip.

[SY6970 Datasheet.pdf](https://github.com/Xinyuan-LilyGO/LilyGo-AMOLED-Series/blob/master/datasheet/SY6970%20Datasheet.pdf)

It is known to be used in the Lilygo [t-displayS3-pro](https://lilygo.cc/products/t-display-s3-pro).

## Working configuration for the t-displayS3-pro (v1)

```yaml
i2c:
  - id: bus_a
    scan: True
    sda: GPIO5
    scl: GPIO6

sy6970:
  id: pmu
  address: 0x6A
  # Optional - defaults to True
  enable_status_led: True

sensor:
  - platform: sy6970
    sy6970_id: pmu
    update_interval: 1s
    vbus_voltage:
      name: VBUS Voltage
    battery_voltage:
      name: Battery Voltage
    system_voltage:
      name: System Voltage
    charge_current:
      name: Charge Current
    precharge_current:
      name: Precharge Current

text_sensor:
  - platform: sy6970
    sy6970_id: pmu
    update_interval: 1s
    bus_status:
      name: "Power Source Type"
      # Reports: "No Input", "USB SDP", "USB CDP", "USB DCP",
      #          "HVDCP", "Adapter", "Non-Standard Adapter", "OTG"
    charge_status:
      name: "Charging Status"
      # Reports: "Not Charging", "Pre-charge", "Fast Charge", "Charge Done"
    ntc_status:
      name: "Battery Temperature"
      # Reports: "Normal", "Warm", "Cool", "Cold", "Hot"

binary_sensor:
  - platform: sy6970
    sy6970_id: pmu
    update_interval: 1s
    charging:
      name: "Battery Charging"
    vbus_connected:
      name: VBUS Connected
    charge_done:
      name: Charge Done
```

## Implementation
* Core component: Direct I2C register implementation without external library dependencies
* Sensor platform:
  * VBUS/battery/system voltage,
  * charge/precharge current monitoring
* Binary sensor platform:
  * VBUS connection,
  * charging state,
  * charge completion detection
* Text sensor platform:
  * Bus status (USB SDP/CDP/DCP, HVDCP, adapter type),
  * charge status,
  * NTC temperature status
* Configuration API:
  * Input current limit,
  * charge voltage/current settings,
  * charge enable/disable,
  * status LED control,
  * ADC measurement control


## Register Implementation
The component directly implements the SY6970 register protocol:

* REG_0B: Bus and charge status (bits 7:5 for bus type, 3:2 for charge state)
* REG_0E: Battery voltage (base 2304mV, 20mV steps)
* REG_11: VBUS voltage (base 2600mV, 100mV steps)
* REG_12: Charge current (50mA steps)
* REG_00-07: Configuration registers for current limits, charge parameters, safety timers
