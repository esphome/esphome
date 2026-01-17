import esphome.codegen as cg
from esphome.components import sensor, switch, text_sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
)

from .. import FendtSensor, FendtSwitch, FendtTextSensor, fendt_caravan_ns

ControlUnitDeviceSensor = fendt_caravan_ns.class_(
    "ControlUnitDeviceSensor", cg.PollingComponent
)

CONF_CONTROL_UNIT_DEVICE = "control_unit_device"
CONF_MAIN_SWITCH = "main_switch"
CONF_TEMPERATURE_IN = "temperature_in"
CONF_TEMPERATURE_OUT = "temperature_out"
CONF_POWER_STATUS = "power_status"
CONF_LIGHT_STATUS = "light_status"
CONF_SOFTWARE_VERSION = "software_version"
CONF_FLOOR_HEATER = "floor_heater"

CONTROL_UNITS = {
    CONF_MAIN_SWITCH,
    CONF_TEMPERATURE_IN,
    CONF_TEMPERATURE_OUT,
    CONF_POWER_STATUS,
    CONF_LIGHT_STATUS,
    CONF_SOFTWARE_VERSION,
    CONF_FLOOR_HEATER,
}

UNIT_TYPES = {
    CONF_MAIN_SWITCH: switch.switch_schema(
        FendtSwitch, default_restore_mode="ALWAYS_OFF", icon="mdi:switch"
    ),
    CONF_TEMPERATURE_IN: sensor.sensor_schema(
        FendtSensor,
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=1,
        state_class=STATE_CLASS_MEASUREMENT,
        device_class=DEVICE_CLASS_TEMPERATURE,
    ),
    CONF_TEMPERATURE_OUT: sensor.sensor_schema(
        FendtSensor,
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=1,
        state_class=STATE_CLASS_MEASUREMENT,
        device_class=DEVICE_CLASS_TEMPERATURE,
    ),
    CONF_POWER_STATUS: text_sensor.text_sensor_schema(
        FendtTextSensor, icon="mdi:power-plug"
    ),
    CONF_LIGHT_STATUS: switch.switch_schema(
        FendtSwitch, default_restore_mode="RESTORE_DEFAULT_OFF", icon="mdi:lamp"
    ),
    CONF_SOFTWARE_VERSION: text_sensor.text_sensor_schema(
        FendtTextSensor, icon="mdi:application-braces-outline"
    ),
    CONF_FLOOR_HEATER: switch.switch_schema(
        FendtSwitch, default_restore_mode="RESTORE_DEFAULT_OFF", icon="mdi:heat-wave"
    ),
}

CONFIG_CONTROL_UNIT_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ID): cv.declare_id(ControlUnitDeviceSensor),
        **{cv.Optional(type): schema for type, schema in UNIT_TYPES.items()},
    }
).extend(cv.polling_component_schema("60s"))
