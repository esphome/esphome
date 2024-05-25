import esphome.codegen as cg
import esphome.config_validation as cv
import esphome.final_validate as fv
from esphome import pins
from esphome.components import i2c
from esphome.const import (
    CONF_BINARY_SENSOR,
    CONF_CHANNEL,
    CONF_ID,
    CONF_INPUT,
    CONF_INVERTED,
    CONF_MODE,
    CONF_NUMBER,
    CONF_OUTPUT,
)

CONF_TOUCH_THRESHOLD = "touch_threshold"
CONF_RELEASE_THRESHOLD = "release_threshold"
CONF_TOUCH_DEBOUNCE = "touch_debounce"
CONF_RELEASE_DEBOUNCE = "release_debounce"
CONF_MAX_TOUCH_CHANNEL = "max_touch_channel"
CONF_MPR121 = "mpr121"
CONF_MPR121_ID = "mpr121_id"

DEPENDENCIES = ["i2c"]

mpr121_ns = cg.esphome_ns.namespace("mpr121")
MPR121Component = mpr121_ns.class_("MPR121Component", cg.Component, i2c.I2CDevice)
MPR121GPIOPin = mpr121_ns.class_("MPR121GPIOPin", cg.GPIOPin)

MULTI_CONF = True
CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(MPR121Component),
            cv.Optional(CONF_RELEASE_DEBOUNCE, default=0): cv.int_range(min=0, max=7),
            cv.Optional(CONF_TOUCH_DEBOUNCE, default=0): cv.int_range(min=0, max=7),
            cv.Optional(CONF_TOUCH_THRESHOLD, default=0x0B): cv.int_range(
                min=0x05, max=0x30
            ),
            cv.Optional(CONF_RELEASE_THRESHOLD, default=0x06): cv.int_range(
                min=0x05, max=0x30
            ),
            cv.Optional(CONF_MAX_TOUCH_CHANNEL): cv.int_range(min=3, max=11),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x5A))
)


def _final_validate(config):
    fconf = fv.full_config.get()
    max_touch_channel = 3
    if (binary_sensors := fconf.get(CONF_BINARY_SENSOR)) is not None:
        for binary_sensor in binary_sensors:
            if binary_sensor.get(CONF_MPR121_ID) == config[CONF_ID]:
                max_touch_channel = max(max_touch_channel, binary_sensor[CONF_CHANNEL])
    if max_touch_channel_in_config := config.get(CONF_MAX_TOUCH_CHANNEL):
        if max_touch_channel != max_touch_channel_in_config:
            raise cv.Invalid(
                "Max touch channel must equal the highest binary sensor channel or be removed for auto calculation",
                path=[CONF_MAX_TOUCH_CHANNEL],
            )
    path = fconf.get_path_for_id(config[CONF_ID])[:-1]
    this_config = fconf.get_config_for_path(path)
    this_config[CONF_MAX_TOUCH_CHANNEL] = max_touch_channel


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    cg.add(var.set_touch_debounce(config[CONF_TOUCH_DEBOUNCE]))
    cg.add(var.set_release_debounce(config[CONF_RELEASE_DEBOUNCE]))
    cg.add(var.set_touch_threshold(config[CONF_TOUCH_THRESHOLD]))
    cg.add(var.set_release_threshold(config[CONF_RELEASE_THRESHOLD]))
    cg.add(var.set_max_touch_channel(config[CONF_MAX_TOUCH_CHANNEL]))
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)


def validate_mode(value):
    if bool(value[CONF_INPUT]) == bool(value[CONF_OUTPUT]):
        raise cv.Invalid("Mode must be either input or output")
    return value


# https://www.nxp.com/docs/en/data-sheet/MPR121.pdf, page 4
#
# Among the 12 electrode inputs, 8 inputs are designed as multifunctional pins. When these pins are
# not configured as electrodes, they may be used to drive LEDs or used for general purpose input or
# output.
MPR121_GPIO_PIN_SCHEMA = pins.gpio_base_schema(
    MPR121GPIOPin,
    cv.int_range(min=4, max=11),
    modes=[CONF_INPUT, CONF_OUTPUT],
    mode_validator=validate_mode,
).extend(
    {
        cv.Required(CONF_MPR121): cv.use_id(MPR121Component),
    }
)


def mpr121_pin_final_validate(pin_config, parent_config):
    if pin_config[CONF_NUMBER] <= parent_config[CONF_MAX_TOUCH_CHANNEL]:
        raise cv.Invalid(
            "Pin number must be higher than the max touch channel of the MPR121 component",
        )


@pins.PIN_SCHEMA_REGISTRY.register(
    CONF_MPR121, MPR121_GPIO_PIN_SCHEMA, mpr121_pin_final_validate
)
async def mpr121_gpio_pin_to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    parent = await cg.get_variable(config[CONF_MPR121])

    cg.add(var.set_parent(parent))

    num = config[CONF_NUMBER]
    cg.add(var.set_pin(num))
    cg.add(var.set_inverted(config[CONF_INVERTED]))
    cg.add(var.set_flags(pins.gpio_flags_expr(config[CONF_MODE])))
    return var
