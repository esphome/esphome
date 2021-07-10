import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_COMPONENTS, CONF_ID, CONF_LAMBDA

custom_component_ns = cg.esphome_ns.namespace("custom_component")
CustomComponentConstructor = custom_component_ns.class_("CustomComponentConstructor")

MULTI_CONF = True
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(CustomComponentConstructor),
        cv.Required(CONF_LAMBDA): cv.returning_lambda,
        cv.Optional(CONF_COMPONENTS): cv.ensure_list(
            cv.Schema({cv.GenerateID(): cv.declare_id(cg.Component)}).extend(
                cv.COMPONENT_SCHEMA
            )
        ),
    }
)


async def to_code(config):
    template_ = await cg.process_lambda(
        config[CONF_LAMBDA], [], return_type=cg.std_vector.template(cg.ComponentPtr)
    )

    rhs = CustomComponentConstructor(template_)
    var = cg.variable(config[CONF_ID], rhs)
    for i, conf in enumerate(config.get(CONF_COMPONENTS, [])):
        comp = cg.Pvariable(conf[CONF_ID], var.get_component(i))
        await cg.register_component(comp, conf)
