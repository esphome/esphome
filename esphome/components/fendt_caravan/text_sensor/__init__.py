import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import CONF_TYPE, ENTITY_CATEGORY_DIAGNOSTIC

from .. import CONF_KEY_NAME, CONF_PARENT_ID, CaravanDeviceComponent, fendt_caravan_ns

FendtTextSensor = fendt_caravan_ns.class_(
    "FendtTextSensor",
    text_sensor.TextSensor,
    cg.Component,
    cg.Parented.template(CaravanDeviceComponent),
)


def _text_schema(key_name=cv.UNDEFINED) -> cv.Schema:
    return text_sensor.text_sensor_schema(
        FendtTextSensor, entity_category=ENTITY_CATEGORY_DIAGNOSTIC
    ).extend(
        {
            cv.Required(CONF_PARENT_ID): cv.use_id(CaravanDeviceComponent),
            cv.Optional(CONF_KEY_NAME, default=key_name): cv.string,
        }
    )


CONFIG_SCHEMA = cv.typed_schema(
    {
        "power_status": _text_schema("LINE_EN"),
        "software_version": _text_schema("SOFTWARE_VERSION"),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_PARENT_ID])
    var = await text_sensor.new_text_sensor(config)
    if CONF_KEY_NAME in config:
        cg.add(var.set_key_name(config[CONF_KEY_NAME]))
    await cg.register_component(var, config)
    await cg.register_parented(var, parent)
    cg.add(getattr(parent, f"set_{config[CONF_TYPE]}_text_sensor")(var))
