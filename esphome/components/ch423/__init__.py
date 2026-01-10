from esphome import pins
import esphome.codegen as cg
from esphome.components import i2c
from esphome.components.i2c import I2CBus
import esphome.config_validation as cv
from esphome.const import (
    CONF_BINARY_SENSOR,
    CONF_I2C_ID,
    CONF_ID,
    CONF_INPUT,
    CONF_INVERTED,
    CONF_MODE,
    CONF_NUMBER,
    CONF_OPEN_DRAIN,
    CONF_OUTPUT,
    CONF_PIN,
    CONF_SWITCH,
)
import esphome.final_validate as fv

CODEOWNERS = ["@dwmw2"]
DEPENDENCIES = ["i2c"]
MULTI_CONF = True
ch423_ns = cg.esphome_ns.namespace("ch423")

CH423Component = ch423_ns.class_("CH423Component", cg.Component, i2c.I2CDevice)
CH423GPIOPin = ch423_ns.class_(
    "CH423GPIOPin", cg.GPIOPin, cg.Parented.template(CH423Component)
)

CONF_CH423 = "ch423"

# Note that no address is configurable - each register in the CH423 has a dedicated i2c address
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ID): cv.declare_id(CH423Component),
        cv.GenerateID(CONF_I2C_ID): cv.use_id(I2CBus),
    }
).extend(cv.COMPONENT_SCHEMA)


def _final_validate(config):
    """Validate global CH423 pin mode restrictions."""
    fconf = fv.full_config.get()
    ch423_id = config[CONF_ID]

    # Collect all pins used by this CH423 instance
    gpio_pins = {}  # pin_number -> (is_output, component_type, index)
    gpo_pins = {}  # pin_number -> (is_open_drain, component_type, index)

    # Check binary_sensor and switch components for pins using this CH423
    for component_type in [CONF_BINARY_SENSOR, CONF_SWITCH]:
        if (components := fconf.get(component_type)) is not None:
            for idx, component in enumerate(components):
                if CONF_PIN not in component:
                    continue
                pin_conf = component[CONF_PIN]
                if (
                    not isinstance(pin_conf, dict)
                    or pin_conf.get(CONF_CH423) != ch423_id
                ):
                    continue

                pin_num = pin_conf[CONF_NUMBER]
                mode = pin_conf.get(CONF_MODE, {})

                if pin_num < 8:
                    # GPIO pins (0-7)
                    is_output = mode.get(CONF_OUTPUT, False)
                    gpio_pins[pin_num] = (is_output, component_type, idx)
                else:
                    # GPO pins (8-23)
                    is_open_drain = mode.get(CONF_OPEN_DRAIN, False)
                    gpo_pins[pin_num] = (is_open_drain, component_type, idx)

    # Validate GPIO pins (0-7): all must have same direction
    if gpio_pins:
        directions = {is_output for is_output, _, _ in gpio_pins.values()}
        if len(directions) > 1:
            raise cv.Invalid(
                f"CH423 GPIO pins (0-7) must all be configured as input or all as output. "
                f"Found mixed configuration on pins {sorted(gpio_pins.keys())}"
            )

    # Validate GPO pins (8-23): all must have same open-drain setting
    if gpo_pins:
        od_modes = {is_od for is_od, _, _ in gpo_pins.values()}
        if len(od_modes) > 1:
            raise cv.Invalid(
                f"CH423 GPO pins (8-23) must all be configured as push-pull or all as open-drain. "
                f"Found mixed configuration on pins {sorted(gpo_pins.keys())}"
            )


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    # Can't use register_i2c_device because there is no CONF_ADDRESS
    parent = await cg.get_variable(config[CONF_I2C_ID])
    cg.add(var.set_i2c_bus(parent))


# This is used as a final validation step so that modes have been fully transformed.
def pin_mode_check(pin_config, _):
    if pin_config[CONF_MODE][CONF_INPUT] and pin_config[CONF_NUMBER] >= 8:
        raise cv.Invalid("CH423 only supports input on pins 0-7")
    if pin_config[CONF_MODE][CONF_OPEN_DRAIN] and pin_config[CONF_NUMBER] < 8:
        raise cv.Invalid("CH423 only supports open drain output on pins 8-23")


CH423_PIN_SCHEMA = pins.gpio_base_schema(
    CH423GPIOPin,
    cv.int_range(min=0, max=23),
    modes=[CONF_INPUT, CONF_OUTPUT, CONF_OPEN_DRAIN],
).extend(
    {
        cv.Required(CONF_CH423): cv.use_id(CH423Component),
    }
)


@pins.PIN_SCHEMA_REGISTRY.register(CONF_CH423, CH423_PIN_SCHEMA, pin_mode_check)
async def ch423_pin_to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    parent = await cg.get_variable(config[CONF_CH423])

    cg.add(var.set_parent(parent))

    num = config[CONF_NUMBER]
    cg.add(var.set_pin(num))
    cg.add(var.set_inverted(config[CONF_INVERTED]))
    cg.add(var.set_flags(pins.gpio_flags_expr(config[CONF_MODE])))
    return var
