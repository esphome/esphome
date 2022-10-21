import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import (
    DEVICE_CLASS_COLD,
    DEVICE_CLASS_HEAT,
    DEVICE_CLASS_PROBLEM,
)
from . import OpenThermComponent, CONF_OPENTHERM_ID

CONF_CH_ACTIVE = "ch_active"
CONF_DHW_ACTIVE = "dhw_active"
CONF_FLAME_ACTIVE = "flame_active"
CONF_COOLING_ACTIVE = "cooling_active"
CONF_FAULT = "fault"
CONF_DIAGNOSTIC = "diagnostic"

TYPES = [
    CONF_CH_ACTIVE,
    CONF_DHW_ACTIVE,
    CONF_COOLING_ACTIVE,
    CONF_FLAME_ACTIVE,
    CONF_FAULT,
    CONF_DIAGNOSTIC,
]

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(CONF_OPENTHERM_ID): cv.use_id(
                OpenThermComponent
            ),
            cv.Optional(CONF_CH_ACTIVE): binary_sensor.binary_sensor_schema(
                device_class=DEVICE_CLASS_HEAT,
            ),
            cv.Optional(CONF_DHW_ACTIVE): binary_sensor.binary_sensor_schema(
                device_class=DEVICE_CLASS_HEAT,
            ),
            cv.Optional(CONF_COOLING_ACTIVE): binary_sensor.binary_sensor_schema(
                device_class=DEVICE_CLASS_COLD,
            ),
            cv.Optional(CONF_FLAME_ACTIVE): binary_sensor.binary_sensor_schema(
                device_class=DEVICE_CLASS_HEAT,
            ),
            cv.Optional(CONF_FAULT): binary_sensor.binary_sensor_schema(
                device_class=DEVICE_CLASS_PROBLEM,
            ),
            cv.Optional(CONF_DIAGNOSTIC): binary_sensor.binary_sensor_schema(
                device_class=DEVICE_CLASS_PROBLEM,
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA)
)


async def setup_conf(config, key, hub):
    if key in config:
        conf = config[key]
        var = await binary_sensor.new_binary_sensor(conf)
        cg.add(getattr(hub, f"set_{key}_binary_sensor")(var))


async def to_code(config):
    hub = await cg.get_variable(config[CONF_OPENTHERM_ID])
    for key in TYPES:
        await setup_conf(config, key, hub)
