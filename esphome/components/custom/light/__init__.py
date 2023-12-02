import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import light
from esphome.const import CONF_ID, CONF_LAMBDA
from .. import custom_ns

CustomLightOutputConstructor = custom_ns.class_("CustomLightOutputConstructor")
CONF_LIGHTS = "lights"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(CustomLightOutputConstructor),
        cv.Required(CONF_LAMBDA): cv.returning_lambda,
        cv.Required(CONF_LIGHTS): cv.ensure_list(light.ADDRESSABLE_LIGHT_SCHEMA),
    }
)


async def to_code(config):
    template_ = await cg.process_lambda(
        config[CONF_LAMBDA],
        [],
        return_type=cg.std_vector.template(light.LightOutput.operator("ptr")),
    )

    rhs = CustomLightOutputConstructor(template_)
    custom = cg.variable(config[CONF_ID], rhs)
    for i, conf in enumerate(config[CONF_LIGHTS]):
        rhs = custom.Pget_light(i)
        await light.register_light(rhs, conf)
