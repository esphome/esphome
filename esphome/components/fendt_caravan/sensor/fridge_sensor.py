import esphome.codegen as cg
from esphome.components import number, select, switch, text_sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_MAX_VALUE,
    CONF_MIN_VALUE,
    CONF_OPTIONS,
    CONF_STEP,
)

from .. import FendtNumber, FendtSelect, FendtSwitch, FendtTextSensor, fendt_caravan_ns

FridgeDeviceSensor = fendt_caravan_ns.class_("FridgeDeviceSensor", cg.PollingComponent)

CONF_FRIDGE_DEVICE = "fridge_device"
CONF_FRIDGE_AVAILABLE = "fridge_available"
CONF_FRIDGE_STATUS = "fridge_status"
CONF_FRIDGE_MODE = "fridge_mode"
CONF_FRIDGE_SOURCE = "fridge_source"
CONF_FRIDGE_TYPE = "fridge_type"
CONF_FRIDGE_TEMP = "fridge_temperature"

FRIDGES = {
    CONF_FRIDGE_AVAILABLE,
    CONF_FRIDGE_STATUS,
    CONF_FRIDGE_MODE,
    CONF_FRIDGE_SOURCE,
    CONF_FRIDGE_TYPE,
    CONF_FRIDGE_TEMP,
}

FRIDGE_TYPES = {
    CONF_FRIDGE_AVAILABLE: text_sensor.text_sensor_schema(
        FendtTextSensor, icon="mdi:power-settings"
    ),
    CONF_FRIDGE_STATUS: switch.switch_schema(FendtSwitch, icon="mdi:fridge"),
    CONF_FRIDGE_MODE: select.select_schema(FendtSelect, icon="mdi:gauge").extend(
        {
            cv.Optional(
                CONF_OPTIONS, default=["Performance", "Quite", "Boost"]
            ): cv.ensure_list(cv.string_strict)
        }
    ),
    CONF_FRIDGE_SOURCE: text_sensor.text_sensor_schema(
        FendtTextSensor, icon="mdi:power-settings"
    ),
    CONF_FRIDGE_TYPE: text_sensor.text_sensor_schema(
        FendtTextSensor, icon="mdi:power-settings"
    ),
    CONF_FRIDGE_TEMP: number.number_schema(FendtNumber, icon="mdi:gauge-empty").extend(
        {
            cv.Optional(CONF_MIN_VALUE, default=1): cv.int_,
            cv.Optional(CONF_MAX_VALUE, default=5): cv.int_,
            cv.Optional(CONF_STEP, default=1): cv.int_,
        }
    ),
}

CONFIG_FRIDGE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ID): cv.declare_id(FridgeDeviceSensor),
        **{cv.Optional(type): schema for type, schema in FRIDGE_TYPES.items()},
    }
).extend(cv.polling_component_schema("60s"))
