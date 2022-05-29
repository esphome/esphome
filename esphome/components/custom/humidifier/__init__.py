import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import humidifier
from esphome.const import CONF_ID, CONF_LAMBDA
from .. import custom_ns

CustomHumidifierConstructor = custom_ns.class_("CustomHumidifierConstructor")
CONF_HUMIDIFIERS = "humidifiers"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(CustomHumidifierConstructor),
        cv.Required(CONF_LAMBDA): cv.returning_lambda,
        cv.Required(CONF_HUMIDIFIERS): cv.ensure_list(humidifier.HUMIDIFIER_SCHEMA),
    }
)


async def to_code(config):
    template_ = await cg.process_lambda(
        config[CONF_LAMBDA],
        [],
        return_type=cg.std_vector.template(humidifier.Humidifier.operator("ptr")),
    )

    rhs = CustomHumidifierConstructor(template_)
    custom = cg.variable(config[CONF_ID], rhs)
    for i, conf in enumerate(config[CONF_HUMIDIFIERS]):
        rhs = custom.Pget_humidifier(i)
        await humidifier.register_humidifier(rhs, conf)
