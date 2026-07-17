from esphome import automation
import esphome.codegen as cg
from esphome.components import i2c
import esphome.config_validation as cv
from esphome.const import CONF_DIRECTION, CONF_HYSTERESIS, CONF_ID, CONF_MODE

from ..mt6701 import MT6701Component, position12

CODEOWNERS = ["@slimcdk"]
AUTO_LOAD = ["mt6701"]
DEPENDENCIES = ["i2c"]
MULTI_CONF = True

mt6701_i2c_ns = cg.esphome_ns.namespace("mt6701_i2c")
MT6701I2CComponent = mt6701_i2c_ns.class_(
    "MT6701I2CComponent", MT6701Component, i2c.I2CDevice
)
SaveEEPROMAction = mt6701_i2c_ns.class_("SaveEEPROMAction", automation.Action)

CONF_ZERO_OFFSET = "zero_offset"
CONF_OUTPUT_MODE = "output_mode"
CONF_ABZ = "abz"
CONF_PULSES_PER_REVOLUTION = "pulses_per_revolution"
CONF_Z_PULSE_WIDTH = "z_pulse_width"
CONF_UVW = "uvw"
CONF_POLE_PAIRS = "pole_pairs"
CONF_OUT_PIN = "out_pin"
CONF_PWM_FREQUENCY = "pwm_frequency"
CONF_PWM_POLARITY = "pwm_polarity"
CONF_ANALOG_START = "analog_start"
CONF_ANALOG_STOP = "analog_stop"

# Register bit value written for each rotation direction (DIR bit, datasheet
# section 8.1: DIR=1 means the angle increases clockwise).
DIRECTION = {
    "CLOCKWISE": 1,
    "COUNTERCLOCKWISE": 0,
}

# Hysteresis expressed in output LSBs -> HYST register code.
HYSTERESIS = {
    0: 4,
    0.25: 5,
    0.5: 6,
    1: 0,
    2: 1,
    4: 2,
    8: 3,
}

Z_PULSE_WIDTH = {
    "1LSB": 0,
    "2LSB": 1,
    "4LSB": 2,
    "8LSB": 3,
    "12LSB": 4,
    "16LSB": 5,
    "180DEG": 6,
}

OUTPUT_MODE = {
    "ABZ": 0,
    "UVW": 1,
}

OUT_PIN_MODE = {
    "ANALOG": 0,
    "PWM": 1,
}

PWM_FREQUENCY = {
    "994.4HZ": 0,
    "497.2HZ": 1,
}

PWM_POLARITY = {
    "HIGH": 0,
    "LOW": 1,
}


def validate_hysteresis(value):
    value = cv.float_(value)
    if value not in HYSTERESIS:
        raise cv.Invalid(
            f"Hysteresis must be one of {sorted(HYSTERESIS)} (LSBs), got {value}"
        )
    return HYSTERESIS[value]


CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(MT6701I2CComponent),
            cv.Optional(CONF_DIRECTION): cv.enum(DIRECTION, upper=True),
            cv.Optional(CONF_ZERO_OFFSET): position12,
            cv.Optional(CONF_HYSTERESIS): validate_hysteresis,
            cv.Optional(CONF_OUTPUT_MODE): cv.enum(OUTPUT_MODE, upper=True),
            cv.Optional(CONF_ABZ): cv.Schema(
                {
                    cv.Optional(CONF_PULSES_PER_REVOLUTION): cv.int_range(
                        min=1, max=1024
                    ),
                    cv.Optional(CONF_Z_PULSE_WIDTH): cv.enum(Z_PULSE_WIDTH, upper=True),
                }
            ),
            cv.Optional(CONF_UVW): cv.Schema(
                {
                    cv.Required(CONF_POLE_PAIRS): cv.int_range(min=1, max=16),
                }
            ),
            cv.Optional(CONF_OUT_PIN): cv.Schema(
                {
                    cv.Required(CONF_MODE): cv.enum(OUT_PIN_MODE, upper=True),
                    cv.Optional(CONF_PWM_FREQUENCY): cv.enum(PWM_FREQUENCY, upper=True),
                    cv.Optional(CONF_PWM_POLARITY): cv.enum(PWM_POLARITY, upper=True),
                    cv.Optional(CONF_ANALOG_START): position12,
                    cv.Optional(CONF_ANALOG_STOP): position12,
                }
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x06))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    if (direction := config.get(CONF_DIRECTION)) is not None:
        cg.add(var.set_direction(direction))
    if (zero_offset := config.get(CONF_ZERO_OFFSET)) is not None:
        cg.add(var.set_zero_offset(zero_offset))
    if (hysteresis := config.get(CONF_HYSTERESIS)) is not None:
        cg.add(var.set_hysteresis(hysteresis))
    if (output_mode := config.get(CONF_OUTPUT_MODE)) is not None:
        # cv.enum returns the (always truthy) key string; the register code
        # sits in enum_value (ABZ=0, UVW=1).
        cg.add(var.set_output_mode_uvw(bool(output_mode.enum_value)))

    if abz := config.get(CONF_ABZ):
        if (ppr := abz.get(CONF_PULSES_PER_REVOLUTION)) is not None:
            cg.add(var.set_abz_pulses_per_revolution(ppr))
        if (width := abz.get(CONF_Z_PULSE_WIDTH)) is not None:
            cg.add(var.set_z_pulse_width(width))

    if uvw := config.get(CONF_UVW):
        cg.add(var.set_uvw_pole_pairs(uvw[CONF_POLE_PAIRS]))

    if out_pin := config.get(CONF_OUT_PIN):
        # Same enum-key caveat as output_mode above (ANALOG=0, PWM=1).
        cg.add(var.set_out_pin_pwm(bool(out_pin[CONF_MODE].enum_value)))
        if (freq := out_pin.get(CONF_PWM_FREQUENCY)) is not None:
            cg.add(var.set_pwm_frequency(freq))
        if (pol := out_pin.get(CONF_PWM_POLARITY)) is not None:
            cg.add(var.set_pwm_polarity(pol))
        if (start := out_pin.get(CONF_ANALOG_START)) is not None:
            cg.add(var.set_analog_start(start))
        if (stop := out_pin.get(CONF_ANALOG_STOP)) is not None:
            cg.add(var.set_analog_stop(stop))


MT6701_SAVE_EEPROM_ACTION_SCHEMA = automation.maybe_simple_id(
    {
        cv.Required(CONF_ID): cv.use_id(MT6701I2CComponent),
    }
)


@automation.register_action(
    "mt6701_i2c.save_eeprom",
    SaveEEPROMAction,
    MT6701_SAVE_EEPROM_ACTION_SCHEMA,
    synchronous=True,
)
async def save_eeprom_action_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, parent)
