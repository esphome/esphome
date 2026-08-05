import esphome.codegen as cg
from esphome.components import i2c
import esphome.config_validation as cv
from esphome.const import CONF_DIRECTION, CONF_ID, CONF_MODE, CONF_MODEL

CODEOWNERS = ["@lboue"]
DEPENDENCIES = ["i2c"]
MULTI_CONF = True

CONF_M5_UNIT_BLDC_ID = "m5_unit_bldc_id"
CONF_POLE_PAIRS = "pole_pairs"
CONF_PID = "pid"
CONF_P = "p"
CONF_I = "i"
CONF_D = "d"
CONF_SAVE_TO_FLASH = "save_to_flash"

UNIT_RPM = "rpm"

m5_unit_bldc_ns = cg.esphome_ns.namespace("m5_unit_bldc")
M5UnitBldc = m5_unit_bldc_ns.class_("M5UnitBldc", i2c.I2CDevice, cg.PollingComponent)

ControlMode = m5_unit_bldc_ns.enum("ControlMode", is_class=True)
CONTROL_MODES = {
    "open_loop": ControlMode.OPEN_LOOP,
    "closed_loop": ControlMode.CLOSED_LOOP,
}

Direction = m5_unit_bldc_ns.enum("Direction", is_class=True)
DIRECTIONS = {
    "forward": Direction.FORWARD,
    "backward": Direction.BACKWARD,
}

MotorModel = m5_unit_bldc_ns.enum("MotorModel", is_class=True)
MOTOR_MODELS = {
    "low_speed": MotorModel.LOW_SPEED,
    "high_speed": MotorModel.HIGH_SPEED,
}

PID_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_P): cv.float_,
        cv.Required(CONF_I): cv.float_,
        cv.Required(CONF_D): cv.float_,
    }
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(M5UnitBldc),
            cv.Optional(CONF_MODE, default="open_loop"): cv.enum(
                CONTROL_MODES, lower=True
            ),
            cv.Optional(CONF_DIRECTION, default="forward"): cv.enum(
                DIRECTIONS, lower=True
            ),
            cv.Optional(CONF_MODEL, default="low_speed"): cv.enum(
                MOTOR_MODELS, lower=True
            ),
            cv.Required(CONF_POLE_PAIRS): cv.int_range(min=1, max=255),
            cv.Optional(CONF_PID): PID_SCHEMA,
            cv.Optional(CONF_SAVE_TO_FLASH, default=False): cv.boolean,
        }
    )
    .extend(cv.polling_component_schema("1s"))
    .extend(i2c.i2c_device_schema(0x65))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    cg.add(var.set_control_mode(config[CONF_MODE]))
    cg.add(var.set_initial_direction(config[CONF_DIRECTION]))
    cg.add(var.set_motor_model(config[CONF_MODEL]))
    cg.add(var.set_pole_pairs(config[CONF_POLE_PAIRS]))
    cg.add(var.set_save_to_flash(config[CONF_SAVE_TO_FLASH]))

    if pid := config.get(CONF_PID):
        cg.add(var.set_pid(pid[CONF_P], pid[CONF_I], pid[CONF_D]))
