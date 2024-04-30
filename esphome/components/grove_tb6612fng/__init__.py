import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import i2c

from esphome.const import (
    CONF_ID,
    CONF_CHANNEL,
    CONF_SPEED,
    CONF_DIRECTION,
    CONF_ADDRESS,
)

DEPENDENCIES = ["i2c"]

CODEOWNERS = ["@max246"]

MULTI_CONF = True

grove_tb6612fng_ns = cg.esphome_ns.namespace("grove_tb6612fng")
GROVE_TB6612FNG = grove_tb6612fng_ns.class_(
    "GroveMotorDriveTB6612FNG", cg.Component, i2c.I2CDevice
)
GROVETB6612FNGMotorRunAction = grove_tb6612fng_ns.class_(
    "GROVETB6612FNGMotorRunAction", automation.Action
)
GROVETB6612FNGMotorBrakeAction = grove_tb6612fng_ns.class_(
    "GROVETB6612FNGMotorBrakeAction", automation.Action
)
GROVETB6612FNGMotorStopAction = grove_tb6612fng_ns.class_(
    "GROVETB6612FNGMotorStopAction", automation.Action
)
GROVETB6612FNGMotorStandbyAction = grove_tb6612fng_ns.class_(
    "GROVETB6612FNGMotorStandbyAction", automation.Action
)
GROVETB6612FNGMotorNoStandbyAction = grove_tb6612fng_ns.class_(
    "GROVETB6612FNGMotorNoStandbyAction", automation.Action
)
GROVETB6612FNGMotorChangeAddressAction = grove_tb6612fng_ns.class_(
    "GROVETB6612FNGMotorChangeAddressAction", automation.Action
)

DIRECTION_TYPE = {
    "FORWARD": 1,
    "BACKWARD": 2,
}

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.declare_id(GROVE_TB6612FNG),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x14))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)


@automation.register_action(
    "grove_tb6612fng.run",
    GROVETB6612FNGMotorRunAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(GROVE_TB6612FNG),
            cv.Required(CONF_CHANNEL): cv.templatable(cv.int_range(min=0, max=1)),
            cv.Required(CONF_SPEED): cv.templatable(cv.int_range(min=0, max=255)),
            cv.Required(CONF_DIRECTION): cv.enum(DIRECTION_TYPE, upper=True),
        }
    ),
)
async def grove_tb6612fng_run_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])

    template_channel = await cg.templatable(config[CONF_CHANNEL], args, int)
    template_speed = await cg.templatable(config[CONF_SPEED], args, cg.uint16)
    template_speed = (
        template_speed if config[CONF_DIRECTION] == "FORWARD" else -template_speed
    )
    cg.add(var.set_channel(template_channel))
    cg.add(var.set_speed(template_speed))
    return var


@automation.register_action(
    "grove_tb6612fng.break",
    GROVETB6612FNGMotorBrakeAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(GROVE_TB6612FNG),
            cv.Required(CONF_CHANNEL): cv.templatable(cv.int_range(min=0, max=1)),
        }
    ),
)
async def grove_tb6612fng_break_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])

    template_channel = await cg.templatable(config[CONF_CHANNEL], args, int)
    cg.add(var.set_channel(template_channel))
    return var


@automation.register_action(
    "grove_tb6612fng.stop",
    GROVETB6612FNGMotorStopAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(GROVE_TB6612FNG),
            cv.Required(CONF_CHANNEL): cv.templatable(cv.int_range(min=0, max=1)),
        }
    ),
)
async def grove_tb6612fng_stop_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])

    template_channel = await cg.templatable(config[CONF_CHANNEL], args, int)
    cg.add(var.set_channel(template_channel))
    return var


@automation.register_action(
    "grove_tb6612fng.standby",
    GROVETB6612FNGMotorStandbyAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(GROVE_TB6612FNG),
        }
    ),
)
async def grove_tb6612fng_standby_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])

    return var


@automation.register_action(
    "grove_tb6612fng.no_standby",
    GROVETB6612FNGMotorNoStandbyAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(GROVE_TB6612FNG),
        }
    ),
)
async def grove_tb6612fng_no_standby_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])

    return var


@automation.register_action(
    "grove_tb6612fng.change_address",
    GROVETB6612FNGMotorChangeAddressAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(GROVE_TB6612FNG),
            cv.Required(CONF_ADDRESS): cv.i2c_address,
        }
    ),
)
async def grove_tb6612fng_change_address_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])

    template_channel = await cg.templatable(config[CONF_ADDRESS], args, int)
    cg.add(var.set_address(template_channel))
    return var
