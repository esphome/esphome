from esphome.automation import parent_ref
import esphome.codegen as cg
from esphome.components import text_sensor
from esphome.components.const import CONF_ERROR_CODE
import esphome.config_validation as cv
from esphome.const import CONF_MODEL
from esphome.types import ConfigType

from .. import (
    AQUA_SENSOR_DATA,
    CONF_SYSTA_BUS_ID,
    CONF_SYSTASOLAR_AQUA,
    DUMP,
    DUMP_TAG,
    MESSAGE,
    SystaBus,
    systa_bus_ns,
)

CONFIG_SCHEMA = cv.typed_schema(
    {
        CONF_SYSTASOLAR_AQUA: cv.Schema(
            {
                cv.GenerateID(CONF_SYSTA_BUS_ID): cv.use_id(SystaBus),
                cv.Optional(CONF_ERROR_CODE): text_sensor.text_sensor_schema(),
            }
        ),
    },
    key=CONF_MODEL,
    lower=True,
    space="_",
)


async def to_code(config: ConfigType) -> None:
    systa_bus = await cg.get_variable(config[CONF_SYSTA_BUS_ID])
    # The schema only lets a model's own keys through, so unconfigured ones are skipped here
    text_sensors = text_sensor.sub_text_sensors(config)
    if (var := await text_sensors(CONF_ERROR_CODE, None)) is None:
        return
    AQUA_SENSOR_DATA.add(
        systa_bus, systa_bus_ns.publish_aqua_error_code(parent_ref(var), MESSAGE)
    )
    DUMP.add(
        systa_bus,
        text_sensor.text_sensor_ns.log_text_sensor(
            DUMP_TAG, "  ", cg.MockObj("LOG_STR_LITERAL")("Error code"), parent_ref(var)
        ),
    )
