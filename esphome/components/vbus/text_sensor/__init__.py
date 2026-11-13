"""Optional human-readable results for explicit CS Plus setting requests."""

import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import CONF_FILTERS, ENTITY_CATEGORY_DIAGNOSTIC
import esphome.final_validate as fv
from esphome.types import ConfigType

from .. import (
    CONF_CONTROLLER_ID,
    ResolDeltaSolCSPlus,
    validate_setting_controller,
    vbus_ns,
)

DEPENDENCIES = ["vbus"]
CONF_LAST_WRITE_RESULT = "last_write_result"
VBusWriteResultTextSensor = vbus_ns.class_(
    "VBusWriteResultTextSensor", text_sensor.TextSensor
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_CONTROLLER_ID): cv.use_id(ResolDeltaSolCSPlus),
        cv.Required(CONF_LAST_WRITE_RESULT): text_sensor.text_sensor_schema(
            VBusWriteResultTextSensor,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
    }
)

FINAL_VALIDATE_SCHEMA = cv.All(
    validate_setting_controller,
    fv.unique_platform_key(
        domain="text_sensor",
        platform="vbus",
        keys=(CONF_CONTROLLER_ID,),
        message="Configure last_write_result only once per controller",
    ),
)


async def to_code(config: ConfigType) -> None:
    cg.add_define("USE_VBUS_WRITE_RESULT")
    hub = await cg.get_variable(config[CONF_CONTROLLER_ID])
    sensors = text_sensor.sub_text_sensors(config)
    await sensors(
        CONF_LAST_WRITE_RESULT,
        hub.set_last_write_result_text_sensor,
        bool(config[CONF_LAST_WRITE_RESULT].get(CONF_FILTERS)),
    )
