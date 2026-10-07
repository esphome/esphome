from esphome import automation
import esphome.codegen as cg
from esphome.components import i2c
import esphome.config_validation as cv
from esphome.const import (
    CONF_ADDRESS,
    CONF_CHANNEL,
    CONF_DIRECTION,
    CONF_ID,
    CONF_SPEED,
)

DEPENDENCIES = ["i2c"]

CODEOWNERS = ["@max246"]

MULTI_CONF = True

grove_tb6612fng_ns = cg.esphome_ns.namespace("grove_tb6612fng")
GROVE_TB6612FNG = grove_tb6612fng_ns.class_(
    "GroveMotorDriveTB6612FNG", cg.Component, i2c.I2CDevice
)
DIRECTION_TYPE = {
    "FORWARD": 1,
    "BACKWARD": -1,
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


automation.register_apply_action(
    "grove_tb6612fng.run",
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(GROVE_TB6612FNG),
            cv.Required(CONF_CHANNEL): cv.templatable(cv.int_range(min=0, max=1)),
            cv.Required(CONF_SPEED): cv.templatable(cv.int_range(min=0, max=255)),
            cv.Required(CONF_DIRECTION): cv.enum(DIRECTION_TYPE, upper=True),
        }
    ),
    automation.ApplyCall(
        "dc_motor_run({}, {} * {})",
        ((CONF_CHANNEL, cg.uint8), (CONF_DIRECTION, cg.int16), (CONF_SPEED, cg.uint16)),
    ),
)

CHANNEL_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.use_id(GROVE_TB6612FNG),
        cv.Required(CONF_CHANNEL): cv.templatable(cv.int_range(min=0, max=1)),
    }
)

automation.register_apply_action(
    "grove_tb6612fng.break",
    CHANNEL_SCHEMA,
    automation.ApplyField(CONF_CHANNEL, "dc_motor_brake", cg.uint8),
)

automation.register_apply_action(
    "grove_tb6612fng.stop",
    CHANNEL_SCHEMA,
    automation.ApplyField(CONF_CHANNEL, "dc_motor_stop", cg.uint8),
)

PARENT_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.use_id(GROVE_TB6612FNG),
    }
)

automation.register_apply_action(
    "grove_tb6612fng.standby", PARENT_SCHEMA, automation.ApplyCall("standby()")
)

automation.register_apply_action(
    "grove_tb6612fng.no_standby", PARENT_SCHEMA, automation.ApplyCall("not_standby()")
)

automation.register_apply_action(
    "grove_tb6612fng.change_address",
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(GROVE_TB6612FNG),
            cv.Required(CONF_ADDRESS): cv.i2c_address,
        }
    ),
    automation.ApplyField(CONF_ADDRESS, "set_i2c_addr", cg.uint8),
)
