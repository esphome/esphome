import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import (
    CONF_ADDRESS,
    CONF_BYTES,
    CONF_DIV_RATIO,
    CONF_ID,
    CONF_MODE,
)
from . import optolink_ns, CONF_OPTOLINK_ID
from .sensor import SENSOR_BASE_SCHEMA

OptolinkTextSensor = optolink_ns.class_(
    "OptolinkTextSensor", text_sensor.TextSensor, cg.PollingComponent
)

TextSensorMode = optolink_ns.enum("TextSensorMode")
MODE = {
    "MAP": TextSensorMode.MAP,
    "RAW": TextSensorMode.RAW,
    "DAY_SCHEDULE": TextSensorMode.DAY_SCHEDULE,
}

DAY_OF_WEEK = {
    "MONDAY": 0,
    "TUESDAY": 1,
    "WEDNESDAY": 2,
    "THURSDAY": 3,
    "FRIDAY": 4,
    "SATURDAY": 5,
    "SUNDAY": 6,
}

CONF_DOW = "day_of_week"


def check_bytes():
    def validator_(config):
        bytes_needed = config[CONF_MODE] in ["MAP", "RAW"]
        bytes_defined = CONF_BYTES in config
        if bytes_needed and not bytes_defined:
            raise cv.Invalid(f"{CONF_BYTES} is required in mode MAP or RAW")
        if not bytes_needed and bytes_defined:
            raise cv.Invalid(f"{CONF_BYTES} is not allowed in mode DAY_SCHEDULE")
        return config

    return validator_


def check_dow():
    def validator_(config):
        if config[CONF_MODE] == "DAY_SCHEDULE" and CONF_DOW not in config:
            raise cv.Invalid(f"{CONF_DOW} is required in mode DAY_SCHEDULE")
        if config[CONF_MODE] != "DAY_SCHEDULE" and CONF_DOW in config:
            raise cv.Invalid(f"{CONF_DOW} is only allowed in mode DAY_SCHEDULE")
        return config

    return validator_


CONFIG_SCHEMA = cv.All(
    text_sensor.text_sensor_schema(OptolinkTextSensor)
    .extend(
        {
            cv.Optional(CONF_MODE, default="MAP"): cv.enum(MODE, upper=True),
            cv.Optional(CONF_BYTES): cv.int_range(min=1, max=9),
            cv.Optional(CONF_DOW): cv.enum(DAY_OF_WEEK, upper=True),
        }
    )
    .extend(SENSOR_BASE_SCHEMA),
    check_bytes(),
    check_dow(),
)


async def to_code(config):
    component = await cg.get_variable(config[CONF_OPTOLINK_ID])
    var = cg.new_Pvariable(config[CONF_ID], component)

    await cg.register_component(var, config)
    await text_sensor.register_text_sensor(var, config)

    cg.add(var.set_mode(config[CONF_MODE]))
    cg.add(var.set_address(config[CONF_ADDRESS]))
    cg.add(var.set_div_ratio(config[CONF_DIV_RATIO]))
    if CONF_BYTES in config:
        cg.add(var.set_bytes(config[CONF_BYTES]))
    if CONF_DOW in config:
        cg.add(var.set_day_of_week(config[CONF_DOW]))
