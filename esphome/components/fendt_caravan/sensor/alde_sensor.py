import esphome.codegen as cg
from esphome.components import number, select, switch, text_sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_MAX_VALUE,
    CONF_MIN_VALUE,
    CONF_OPTIONS,
    CONF_STEP,
    DEVICE_CLASS_TEMPERATURE,
    ENTITY_CATEGORY_CONFIG,
    UNIT_CELSIUS,
)

from .. import FendtNumber, FendtSelect, FendtSwitch, FendtTextSensor, fendt_caravan_ns

AldeDeviceSensor = fendt_caravan_ns.class_("AldeDeviceSensor", cg.PollingComponent)

CONF_ALDE_DEVICE = "alde_device"
CONF_ALDE_AVAILABLE = "alde_available"
CONF_ALDE_HEATER_STATUS = "alde_heater_satus"
CONF_ALDE_HEATER_TEMP = "alde_heater_temperature"
CONF_ALDE_HEATER_WATER = "alde_heater_water"
CONF_ALDE_HEATER_WATER_TEMP = "alde_heater_water_temperature"
CONF_ALDE_HEATER_ELECTRIC = "alde_heater_electric"
CONF_ALDE_HEATER_GAS = "alde_heater_gas"

ALDES = {
    CONF_ALDE_AVAILABLE,
    CONF_ALDE_HEATER_STATUS,
    CONF_ALDE_HEATER_TEMP,
    CONF_ALDE_HEATER_WATER,
    CONF_ALDE_HEATER_WATER_TEMP,
    CONF_ALDE_HEATER_ELECTRIC,
    CONF_ALDE_HEATER_GAS,
}


ALDE_TYPES = {
    CONF_ALDE_AVAILABLE: text_sensor.text_sensor_schema(
        FendtTextSensor, icon="mdi:air-filter"
    ),
    CONF_ALDE_HEATER_STATUS: switch.switch_schema(
        FendtSwitch, default_restore_mode="RESTORE_DEFAULT_OFF", icon="mdi:heat-wave"
    ),
    CONF_ALDE_HEATER_TEMP: number.number_schema(
        FendtNumber,
        device_class=DEVICE_CLASS_TEMPERATURE,
        unit_of_measurement=UNIT_CELSIUS,
    ).extend(
        {
            cv.Optional(CONF_MIN_VALUE, default=5): cv.float_,
            cv.Optional(CONF_MAX_VALUE, default=30): cv.float_,
            cv.Optional(CONF_STEP, default=0.5): cv.float_,
        }
    ),
    CONF_ALDE_HEATER_WATER: switch.switch_schema(
        FendtSwitch, default_restore_mode="RESTORE_DEFAULT_OFF", icon="mdi:water-boiler"
    ),
    CONF_ALDE_HEATER_WATER_TEMP: switch.switch_schema(
        FendtSwitch,
        default_restore_mode="RESTORE_DEFAULT_OFF",
        icon="mdi:thermometer-alert",
    ),
    CONF_ALDE_HEATER_ELECTRIC: select.select_schema(
        FendtSelect, entity_category=ENTITY_CATEGORY_CONFIG, icon="mdi:power-settings"
    ).extend(
        {
            cv.Optional(
                CONF_OPTIONS, default=["Off", "1 kW", "2 kW", "3 kW"]
            ): cv.ensure_list(cv.string_strict)
        }
    ),
    CONF_ALDE_HEATER_GAS: switch.switch_schema(
        FendtSwitch, default_restore_mode="RESTORE_DEFAULT_OFF", icon="mdi:water-boiler"
    ),
}

CONFIG_ALDE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ID): cv.declare_id(AldeDeviceSensor),
        **{cv.Optional(type): schema for type, schema in ALDE_TYPES.items()},
    }
).extend(cv.polling_component_schema("60s"))
