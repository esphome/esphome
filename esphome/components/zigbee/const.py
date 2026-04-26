from dataclasses import dataclass

import esphome.codegen as cg
from esphome.const import (
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_DURATION,
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_FREQUENCY,
    DEVICE_CLASS_HUMIDITY,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_PRESSURE,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_VOLUME_FLOW_RATE,
    UNIT_AMPERE,
    UNIT_CELSIUS,
    UNIT_CENTIMETER,
    UNIT_DECIBEL,
    UNIT_HECTOPASCAL,
    UNIT_HERTZ,
    UNIT_HOUR,
    UNIT_KELVIN,
    UNIT_KILOMETER,
    UNIT_KILOWATT,
    UNIT_KILOWATT_HOURS,
    UNIT_LUX,
    UNIT_METER,
    UNIT_MICROGRAMS_PER_CUBIC_METER,
    UNIT_MILLIAMP,
    UNIT_MILLIGRAMS_PER_CUBIC_METER,
    UNIT_MILLIMETER,
    UNIT_MILLISECOND,
    UNIT_MILLIVOLT,
    UNIT_MINUTE,
    UNIT_OHM,
    UNIT_PARTS_PER_BILLION,
    UNIT_PARTS_PER_MILLION,
    UNIT_PASCAL,
    UNIT_PERCENT,
    UNIT_SECOND,
    UNIT_VOLT,
    UNIT_WATT,
    UNIT_WATT_HOURS,
)

zigbee_ns = cg.esphome_ns.namespace("zigbee")
ZigbeeComponent = zigbee_ns.class_("ZigbeeComponent", cg.Component)
ZigbeeAttribute = zigbee_ns.class_("ZigbeeAttribute", cg.Component)
BinaryAttrs = zigbee_ns.struct("BinaryAttrs")
AnalogAttrs = zigbee_ns.struct("AnalogAttrs")
AnalogAttrsOutput = zigbee_ns.struct("AnalogAttrsOutput")

report = zigbee_ns.enum("ZigbeeReportT")
REPORT = {
    "coordinator": report.ZIGBEE_REPORT_COORDINATOR,
    "enable": report.ZIGBEE_REPORT_ENABLE,
    "force": report.ZIGBEE_REPORT_FORCE,
}

CONF_ON_JOIN = "on_join"
CONF_WIPE_ON_BOOT = "wipe_on_boot"
CONF_REPORT = "report"
CONF_ROUTER = "router"
CONF_POWER_SOURCE = "power_source"
POWER_SOURCE = {
    "UNKNOWN": "ZB_ZCL_BASIC_POWER_SOURCE_UNKNOWN",
    "MAINS_SINGLE_PHASE": "ZB_ZCL_BASIC_POWER_SOURCE_MAINS_SINGLE_PHASE",
    "MAINS_THREE_PHASE": "ZB_ZCL_BASIC_POWER_SOURCE_MAINS_THREE_PHASE",
    "BATTERY": "ZB_ZCL_BASIC_POWER_SOURCE_BATTERY",
    "DC_SOURCE": "ZB_ZCL_BASIC_POWER_SOURCE_DC_SOURCE",
    "EMERGENCY_MAINS_CONST": "ZB_ZCL_BASIC_POWER_SOURCE_EMERGENCY_MAINS_CONST",
    "EMERGENCY_MAINS_TRANSF": "ZB_ZCL_BASIC_POWER_SOURCE_EMERGENCY_MAINS_TRANSF",
}

KEY_ZIGBEE = "zigbee"

# BACnet engineering units mapping (ZCL uses BACnet unit codes)
# See: https://github.com/zigpy/zha/blob/dev/zha/application/platforms/number/bacnet.py
BACNET_UNITS = {
    UNIT_CELSIUS: 62,
    UNIT_KELVIN: 63,
    UNIT_VOLT: 5,
    UNIT_MILLIVOLT: 124,
    UNIT_AMPERE: 3,
    UNIT_MILLIAMP: 2,
    UNIT_OHM: 4,
    UNIT_WATT: 47,
    UNIT_KILOWATT: 48,
    UNIT_WATT_HOURS: 18,
    UNIT_KILOWATT_HOURS: 19,
    UNIT_PASCAL: 53,
    UNIT_HECTOPASCAL: 133,
    UNIT_HERTZ: 27,
    UNIT_MILLIMETER: 30,
    UNIT_CENTIMETER: 118,
    UNIT_METER: 31,
    UNIT_KILOMETER: 193,
    UNIT_MILLISECOND: 159,
    UNIT_SECOND: 73,
    UNIT_MINUTE: 72,
    UNIT_HOUR: 71,
    UNIT_PARTS_PER_MILLION: 96,
    UNIT_PARTS_PER_BILLION: 97,
    UNIT_MICROGRAMS_PER_CUBIC_METER: 219,
    UNIT_MILLIGRAMS_PER_CUBIC_METER: 218,
    UNIT_LUX: 37,
    UNIT_DECIBEL: 199,
    UNIT_PERCENT: 98,
}
BACNET_UNIT_NO_UNITS = 95


@dataclass
class AnalogInputType:
    Temp_Degrees_C = 0x00
    Relative_Humidity_Percent = 0x01
    Pressure_Pascal = 0x02
    Flow_Liters_Per_Sec = 0x03
    Percentage = 0x04
    Parts_Per_Million = 0x05
    Rotational_Speed_RPM = 0x06
    Current_Amps = 0x07
    Frequency_Hz = 0x08
    Power_Watts = 0x09
    Power_Kilo_Watts = 0x0A
    Energy_Kilo_Watt_Hours = 0x0B
    Count = 0x0C
    Enthalpy_KJoules_Per_Kg = 0x0D
    Time_Seconds = 0x0E


ANALOG_INPUT_APPTYPE = {
    (DEVICE_CLASS_TEMPERATURE, UNIT_CELSIUS): AnalogInputType.Temp_Degrees_C,
    (DEVICE_CLASS_HUMIDITY, UNIT_PERCENT): AnalogInputType.Relative_Humidity_Percent,
    (DEVICE_CLASS_PRESSURE, UNIT_PASCAL): AnalogInputType.Pressure_Pascal,
    (DEVICE_CLASS_VOLUME_FLOW_RATE, "L/s"): AnalogInputType.Flow_Liters_Per_Sec,
    (DEVICE_CLASS_CURRENT, UNIT_AMPERE): AnalogInputType.Current_Amps,
    (DEVICE_CLASS_FREQUENCY, UNIT_HERTZ): AnalogInputType.Frequency_Hz,
    (DEVICE_CLASS_POWER, UNIT_WATT): AnalogInputType.Power_Watts,
    (DEVICE_CLASS_POWER, UNIT_KILOWATT): AnalogInputType.Power_Kilo_Watts,
    (DEVICE_CLASS_ENERGY, UNIT_KILOWATT_HOURS): AnalogInputType.Energy_Kilo_Watt_Hours,
    (DEVICE_CLASS_DURATION, UNIT_SECOND): AnalogInputType.Time_Seconds,
}
