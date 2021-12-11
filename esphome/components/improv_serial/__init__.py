from esphome.const import CONF_BAUD_RATE, CONF_ID, CONF_LOGGER
import esphome.codegen as cg
import esphome.config_validation as cv
import esphome.final_validate as fv

CODEOWNERS = ["@esphome/core"]
DEPENDENCIES = ["logger", "wifi"]

improv_serial_ns = cg.esphome_ns.namespace("improv_serial")

ImprovSerialComponent = improv_serial_ns.class_("ImprovSerialComponent", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(ImprovSerialComponent),
    }
).extend(cv.COMPONENT_SCHEMA)


def validate_logger_baud_rate(config):
    logger_conf = fv.full_config.get()[CONF_LOGGER]
    if logger_conf[CONF_BAUD_RATE] == 0:
        raise cv.Invalid("improv_serial requires the logger baud_rate to be not 0")
    return config


FINAL_VALIDATE_SCHEMA = validate_logger_baud_rate


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add_library("esphome/Improv", "1.0.0")
