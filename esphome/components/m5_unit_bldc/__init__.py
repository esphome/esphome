import esphome.codegen as cg
from esphome.components import i2c
import esphome.config_validation as cv
from esphome.const import CONF_DIRECTION, CONF_MODE, CONF_MODEL
from esphome.types import ConfigType

CODEOWNERS = ["@lboue"]
DEPENDENCIES = ["i2c"]
DOMAIN = "m5_unit_bldc"
MULTI_CONF = True

CONF_M5_UNIT_BLDC_ID = "m5_unit_bldc_id"
CONF_POLE_PAIRS = "pole_pairs"
CONF_PID = "pid"
CONF_P = "p"
CONF_I = "i"
CONF_D = "d"
CONF_SAVE_TO_FLASH = "save_to_flash"

# The device stores each gain multiplied by 100 in a signed 32-bit register
_GAIN = cv.float_range(min=-(2**31) / 100, max=(2**31 - 1) / 100)

m5_unit_bldc_ns = cg.esphome_ns.namespace("m5_unit_bldc")
M5UnitBldc = m5_unit_bldc_ns.class_("M5UnitBldc", i2c.I2CDevice, cg.PollingComponent)

ControlMode = m5_unit_bldc_ns.enum("ControlMode", is_class=True)
CONTROL_MODES = {
    "open_loop": ControlMode.CONTROL_MODE_OPEN_LOOP,
    "closed_loop": ControlMode.CONTROL_MODE_CLOSED_LOOP,
}

Direction = m5_unit_bldc_ns.enum("Direction", is_class=True)
DIRECTIONS = {
    "forward": Direction.DIRECTION_FORWARD,
    "backward": Direction.DIRECTION_BACKWARD,
}

MotorModel = m5_unit_bldc_ns.enum("MotorModel", is_class=True)
MOTOR_MODELS = {
    "low_speed": MotorModel.MOTOR_MODEL_LOW_SPEED,
    "high_speed": MotorModel.MOTOR_MODEL_HIGH_SPEED,
}

PID_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_P): _GAIN,
        cv.Required(CONF_I): _GAIN,
        cv.Required(CONF_D): _GAIN,
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


async def to_code(config: ConfigType) -> None:
    var = await i2c.new_i2c_component(
        config,
        config[CONF_MODE],
        config[CONF_DIRECTION],
        config[CONF_MODEL],
        config[CONF_POLE_PAIRS],
        config[CONF_SAVE_TO_FLASH],
    )
    if pid := config.get(CONF_PID):
        p, i, d = (round(pid[key] * 100) for key in (CONF_P, CONF_I, CONF_D))
        cg.add(var.set_pid(p, i, d))
