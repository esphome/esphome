import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import cover
from esphome.const import CONF_ID, CONF_LAMBDA
from .. import custom_ns

CustomCoverConstructor = custom_ns.class_("CustomCoverConstructor")
CONF_COVERS = "covers"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(CustomCoverConstructor),
        cv.Required(CONF_LAMBDA): cv.returning_lambda,
        cv.Required(CONF_COVERS): cv.ensure_list(cover.COVER_SCHEMA),
    }
)


async def to_code(config):
    template_ = await cg.process_lambda(
        config[CONF_LAMBDA],
        [],
        return_type=cg.std_vector.template(cover.Cover.operator("ptr")),
    )

    rhs = CustomCoverConstructor(template_)
    custom = cg.variable(config[CONF_ID], rhs)
    for i, conf in enumerate(config[CONF_COVERS]):
        rhs = custom.Pget_cover(i)
        await cover.register_cover(rhs, conf)
