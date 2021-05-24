import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch
from esphome.const import CONF_ID, CONF_LAMBDA, CONF_SWITCHES
from .. import custom_ns

CustomSwitchConstructor = custom_ns.class_("CustomSwitchConstructor")

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(CustomSwitchConstructor),
        cv.Required(CONF_LAMBDA): cv.returning_lambda,
        cv.Required(CONF_SWITCHES): cv.ensure_list(
            switch.SWITCH_SCHEMA.extend(
                {
                    cv.GenerateID(): cv.declare_id(switch.Switch),
                }
            )
        ),
    }
)


async def to_code(config):
    template_ = await cg.process_lambda(
        config[CONF_LAMBDA], [], return_type=cg.std_vector.template(switch.SwitchPtr)
    )

    rhs = CustomSwitchConstructor(template_)
    var = cg.variable(config[CONF_ID], rhs)
    for i, conf in enumerate(config[CONF_SWITCHES]):
        switch_ = cg.Pvariable(conf[CONF_ID], var.get_switch(i))
        await switch.register_switch(switch_, conf)
