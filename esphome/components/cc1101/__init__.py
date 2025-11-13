from esphome import automation, pins
from esphome.automation import maybe_simple_id
import esphome.codegen as cg
from esphome.components import remote_base, spi
import esphome.config_validation as cv
from esphome.const import CONF_ID

from .const import (
    CONF_AGC,
    CONF_GDO0_PIN,
    CONF_TUNER,
    TYPES,
    CC1101Component,
    for_each_conf,
)

AUTO_LOAD = ["remote_base"]


ns = cg.esphome_ns.namespace("cc1101")


CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(CC1101Component),
            # CHANGED: Use generic gpio_pin_schema.
            # The C++ GPIOPin abstraction handles internal vs external pins.
            cv.Optional(CONF_GDO0_PIN): pins.gpio_pin_schema,
        }
    )
    .extend(
        {cv.Optional(k): v[0] for k, v in TYPES[None].items()},
    )
    .extend(
        {
            cv.Optional(CONF_TUNER): cv.Schema(
                {cv.Optional(k): v[0] for k, v in TYPES[CONF_TUNER].items()}
            ),
            cv.Optional(CONF_AGC): cv.Schema(
                {cv.Optional(k): v[0] for k, v in TYPES[CONF_AGC].items()}
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(spi.spi_device_schema(cs_pin_required=True))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await spi.register_spi_device(var, config)
    if CONF_GDO0_PIN in config:
        gdo0_pin = await cg.gpio_pin_expression(config[CONF_GDO0_PIN])
        cg.add(var.set_config_gdo0_pin(gdo0_pin))

    async def set_var(c, args, setter):
        val = c
        if len(args) > 2:
            val = args[2][c]
        cg.add(getattr(var, f"set_{setter}")(val))

    await for_each_conf(config, TYPES, set_var)


BeginTxAction = ns.class_("BeginTxAction", automation.Action)
EndTxAction = ns.class_("EndTxAction", automation.Action)
ResetAction = ns.class_("ResetAction", automation.Action)

CC1101_ACTION_SCHEMA = cv.Schema(
    maybe_simple_id(
        {
            cv.GenerateID(CONF_ID): cv.use_id(CC1101Component),
        }
    )
)


@automation.register_action("cc1101.begin_tx", BeginTxAction, CC1101_ACTION_SCHEMA)
@automation.register_action("cc1101.end_tx", EndTxAction, CC1101_ACTION_SCHEMA)
@automation.register_action("cc1101.reset", ResetAction, CC1101_ACTION_SCHEMA)
async def cc1101_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


CC1101_TRANSMIT_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID("cc1101_id"): cv.use_id(CC1101Component),
        }
    )
    .extend(remote_base.REMOTE_TRANSMITTABLE_SCHEMA)
    .extend(remote_base.RC_SWITCH_RAW_SCHEMA)
    .extend(remote_base.RC_SWITCH_TRANSMITTER)
)
