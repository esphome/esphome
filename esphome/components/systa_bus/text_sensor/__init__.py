from esphome.automation import parent_ref
import esphome.codegen as cg
from esphome.components import text_sensor
from esphome.components.const import CONF_ERROR_CODE
import esphome.config_validation as cv
from esphome.const import CONF_MODEL
from esphome.types import ConfigType

from .. import (
    AQUA_PUBLISHER,
    CONF_SYSTA_BUS_ID,
    CONF_SYSTASOLAR_AQUA,
    DUMP_PUBLISHER,
    DUMP_TAG,
    MESSAGE,
    child_schema,
    systa_bus_ns,
)

CONFIG_SCHEMA = cv.typed_schema(
    {
        CONF_SYSTASOLAR_AQUA: child_schema(
            "text_sensor",
            {cv.Optional(CONF_ERROR_CODE): text_sensor.text_sensor_schema()},
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
    AQUA_PUBLISHER.add(
        systa_bus, systa_bus_ns.publish_aqua_error_code(parent_ref(var), MESSAGE)
    )
    DUMP_PUBLISHER.add(
        systa_bus, systa_bus_ns.dump_aqua_error_code(DUMP_TAG, parent_ref(var))
    )
