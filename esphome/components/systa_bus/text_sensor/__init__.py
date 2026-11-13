import esphome.codegen as cg
from esphome.components import text_sensor
from esphome.components.const import CONF_ERROR_CODE
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_MODEL
from esphome.types import ConfigType

from .. import (
    CONF_SYSTA_BUS_ID,
    CONF_SYSTASOLAR_AQUA,
    SystaBus,
    register_systa_bus_listener,
    systa_bus_ns,
)

SystaSolarAquaTextSensor = systa_bus_ns.class_("SystaSolarAquaTextSensor")


CONFIG_SCHEMA = cv.typed_schema(
    {
        CONF_SYSTASOLAR_AQUA: cv.Schema(
            {
                cv.GenerateID(): cv.declare_id(SystaSolarAquaTextSensor),
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
    var = cg.new_Pvariable(config[CONF_ID])

    # The schema only lets a model's own keys through, so unconfigured ones are skipped here
    text_sensors = text_sensor.sub_text_sensors(config)
    await text_sensors(CONF_ERROR_CODE, var.set_error_code_text_sensor)

    systa_bus = await cg.get_variable(config[CONF_SYSTA_BUS_ID])
    await register_systa_bus_listener(systa_bus, var)
