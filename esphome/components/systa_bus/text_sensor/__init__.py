import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import CONF_ID, CONF_MODEL

from .. import (
    CONF_SYSTA_BUS_ID,
    CONF_SYSTASOLAR_AQUA,
    SystaBus,
    systa_bus_ns,
)

SystaSolar_Aqua = systa_bus_ns.class_(
    "SystaSolarAquaTextSensor", text_sensor.TextSensor, cg.Component
)

CONF_ERROR_CODE = "error_code"


CONFIG_SCHEMA = cv.typed_schema(
    {
        CONF_SYSTASOLAR_AQUA: cv.COMPONENT_SCHEMA.extend(
            {
                cv.GenerateID(): cv.declare_id(SystaSolar_Aqua),
                cv.GenerateID(CONF_SYSTA_BUS_ID): cv.use_id(SystaBus),
                cv.Optional(CONF_ERROR_CODE): text_sensor.text_sensor_schema(),
            }
        ),
    },
    key=CONF_MODEL,
    lower=True,
    space="_",
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    if config[CONF_MODEL] == CONF_SYSTASOLAR_AQUA:
        if CONF_ERROR_CODE in config:
            sens = await text_sensor.new_text_sensor(config[CONF_ERROR_CODE])
            cg.add(var.set_error_code_text_sensor(sens))

    systa_bus = await cg.get_variable(config[CONF_SYSTA_BUS_ID])
    cg.add(systa_bus.register_listener(var))
