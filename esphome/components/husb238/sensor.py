import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_NAME,
    CONF_CURRENT,
    CONF_VOLTAGE,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_VOLTAGE,
    STATE_CLASS_MEASUREMENT,
    UNIT_AMPERE,
    UNIT_VOLT,
)
from . import Husb238Component, CONF_HUSB238_ID

CODEOWNERS = ["@latonita"]
DEPENDENCIES = ["husb238"]

CONF_SELECTED_VOLTAGE = "selected_voltage"

TYPES = [CONF_CURRENT, CONF_VOLTAGE, CONF_SELECTED_VOLTAGE]

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(CONF_HUSB238_ID): cv.use_id(Husb238Component),
            cv.Optional(CONF_CURRENT): cv.maybe_simple_value(
                sensor.sensor_schema(
                    unit_of_measurement=UNIT_AMPERE,
                    accuracy_decimals=2,
                    device_class=DEVICE_CLASS_CURRENT,
                    state_class=STATE_CLASS_MEASUREMENT,
                ),
                key=CONF_NAME,
            ),
            cv.Optional(CONF_VOLTAGE): cv.maybe_simple_value(
                sensor.sensor_schema(
                    unit_of_measurement=UNIT_VOLT,
                    accuracy_decimals=0,
                    device_class=DEVICE_CLASS_VOLTAGE,
                    state_class=STATE_CLASS_MEASUREMENT,
                ),
                key=CONF_NAME,
            ),
            cv.Optional(CONF_SELECTED_VOLTAGE): cv.maybe_simple_value(
                sensor.sensor_schema(
                    unit_of_measurement=UNIT_VOLT,
                    accuracy_decimals=0,
                    device_class=DEVICE_CLASS_VOLTAGE,
                    state_class=STATE_CLASS_MEASUREMENT,
                ),
                key=CONF_NAME,
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA)
)


async def setup_conf(config, key, hub):
    if sensor_config := config.get(key):
        sens = await sensor.new_sensor(sensor_config)
        cg.add(getattr(hub, f"set_{key}_sensor")(sens))


async def to_code(config):
    hub = await cg.get_variable(config[CONF_HUSB238_ID])
    for key in TYPES:
        await setup_conf(config, key, hub)
