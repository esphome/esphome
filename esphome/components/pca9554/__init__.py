from esphome import pins
import esphome.codegen as cg
from esphome.components import i2c
import esphome.config_validation as cv
from esphome.const import (
    CONF_DEVICE,
    CONF_ID,
    CONF_INPUT,
    CONF_INTERRUPT,
    CONF_INTERRUPT_PIN,
    CONF_INVERTED,
    CONF_MODE,
    CONF_NUMBER,
    CONF_OUTPUT,
    CONF_PULLDOWN,
    CONF_PULLUP,
)

CONF_LATCH = "latch"
CONF_DRIVE_STRENGTH = "drive_strength"

CODEOWNERS = ["@hwstar", "@clydebarrow", "@bdraco"]
AUTO_LOAD = ["gpio_expander"]
DEPENDENCIES = ["i2c"]
MULTI_CONF = True
CONF_PIN_COUNT = "pin_count"
pca9554_ns = cg.esphome_ns.namespace("pca9554")


# Define capabilities of the expander. Set a 1 to this bit to
# enable this functionality.
# Bit 0: Input/Output, all expanders have this one
# Bit 1: Pull-up
# Bit 2: Pull-down
# bit 3: latch
# bit 4: interrupt mask
# bit 5: drive strength
def generate_capability(modes):
    capability = 1
    if "pullup" in modes:
        capability |= 2
    if "pulldown" in modes:
        capability |= 4
    if "latch" in modes:
        capability |= 8
    if "interrupt" in modes:
        capability |= 16
    if "drive_strength" in modes:
        capability |= 32

    return capability


PCA9554_DEVICE_TYPES = {
    "NONE": {
        "modes": [CONF_INPUT, CONF_OUTPUT],
        "pins": 8,
        "open_drain": False,
    },
    "PCA9536": {
        "modes": [CONF_INPUT, CONF_OUTPUT],
        "pins": 4,
        "open_drain": False,
    },
    "PCA9554": {
        "modes": [CONF_INPUT, CONF_OUTPUT],
        "pins": 8,
        "open_drain": False,
    },
    "PCA9554A": {
        "modes": [CONF_INPUT, CONF_OUTPUT],
        "pins": 8,
        "open_drain": False,
    },
    "PCA9535": {
        "modes": [CONF_INPUT, CONF_OUTPUT],
        "pins": 16,
        "open_drain": False,
    },
    "PCAL9538": {
        "modes": [
            CONF_INPUT,
            CONF_OUTPUT,
            CONF_PULLDOWN,
            CONF_PULLUP,
            CONF_INTERRUPT,
            CONF_LATCH,
            CONF_DRIVE_STRENGTH,
        ],
        "pins": 8,
        "open_drain": True,
    },
    "PCAL9555": {
        "modes": [
            CONF_INPUT,
            CONF_OUTPUT,
            CONF_PULLDOWN,
            CONF_PULLUP,
            CONF_INTERRUPT,
            CONF_LATCH,
            CONF_DRIVE_STRENGTH,
        ],
        "pins": 16,
        "open_drain": True,
    },
}

PCA9554Component = pca9554_ns.class_("PCA9554Component", cg.Component, i2c.I2CDevice)
PCA9554GPIOPin = pca9554_ns.class_(
    "PCA9554GPIOPin", cg.GPIOPin, cg.Parented.template(PCA9554Component)
)

CONF_PCA9554 = "pca9554"
CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.declare_id(PCA9554Component),
            cv.Optional(CONF_DEVICE, default="NONE"): cv.enum(
                PCA9554_DEVICE_TYPES, upper=True
            ),
            # To maintain backwards compatibility and to also allow the device
            # type to set pin count, we remove the default here. If this is not
            # set, the number of pins is set from the device type using the dict
            # defintion above.
            cv.Optional(CONF_PIN_COUNT): cv.one_of(4, 8, 16),
            cv.Optional(CONF_INTERRUPT_PIN): pins.internal_gpio_input_pin_schema,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(
        i2c.i2c_device_schema(0x20)
    )  # Note: 0x20 for the non-A part. The PCA9554A parts start at addess 0x38
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    device_dict = PCA9554_DEVICE_TYPES[config[CONF_DEVICE]]

    cg.add(var.set_capability(generate_capability(device_dict["modes"])))

    # This doesnt run until after the pin configs are validated, so we cant set
    # the pin count here only.
    # To maintain backwards compatibiliy, we allow the pin count to be manually
    # set. If it is not manually set, the number of pins is set from the device
    # dict above.
    if CONF_PIN_COUNT in config:
        cg.add(var.set_pin_count(config[CONF_PIN_COUNT]))
    else:
        cg.add(var.set_pin_count(device_dict["pins"]))

    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
    if interrupt_pin := config.get(CONF_INTERRUPT_PIN):
        cg.add(var.set_interrupt_pin(await cg.gpio_pin_expression(interrupt_pin)))


def validate_mode(value):
    # Note: here we cannot validate that the modes entered are supported by the
    # device, only that the modes entered are compatible.
    if not (value[CONF_INPUT] or value[CONF_OUTPUT]):
        raise cv.Invalid("Mode must be either input or output")
    if value[CONF_INPUT] and value[CONF_OUTPUT]:
        raise cv.Invalid("Mode must be either input or output")
    if value[CONF_PULLDOWN] and value[CONF_PULLUP]:
        raise cv.Invalid("Pull-up and pull-down are mutually exclusive.")
    if value[CONF_INTERRUPT] and value[CONF_OUTPUT]:
        raise cv.Invalid("Outputs cannot generate interrupts.")
    if value[CONF_LATCH] and value[CONF_OUTPUT]:
        raise cv.Invalid("Outputs cannot be latched.")
    return value


PCA9554_PIN_SCHEMA = pins.gpio_base_schema(
    PCA9554GPIOPin,
    cv.int_range(min=0, max=15),
    # These are all possible modes supported by this component, they are not
    # necessarily the modes supported by the selected device. That is checked
    # later.
    modes=[
        CONF_INPUT,
        CONF_OUTPUT,
        CONF_PULLDOWN,
        CONF_PULLUP,
        CONF_INTERRUPT,
        CONF_LATCH,
    ],
    mode_validator=validate_mode,
).extend(
    {
        cv.Required(CONF_PCA9554): cv.use_id(PCA9554Component),
        cv.Optional(CONF_DRIVE_STRENGTH, default=3): cv.int_range(min=0, max=3),
    }
)


def pca9554_pin_final_validate(pin_config, parent_config):
    device_name = parent_config[CONF_DEVICE]
    device_dict = PCA9554_DEVICE_TYPES[device_name]

    # To maintain backwards compatibility, we allow the pin count to be manually set.
    if CONF_PIN_COUNT in parent_config:
        count = parent_config[CONF_PIN_COUNT]
    else:
        count = device_dict["pins"]

    # Make sure the user entered a valid pin number
    if pin_config[CONF_NUMBER] >= count:
        raise cv.Invalid(f"Pin number must be in range 0-{count - 1}")

    # Verify that pin modes requested are supported by the device
    for entered_mode, active in list(pin_config["mode"].items()):
        if active and (entered_mode not in device_dict["modes"]):
            raise cv.Invalid(
                f"Pin mode '{entered_mode}' is not valid for device {device_name}."
            )


@pins.PIN_SCHEMA_REGISTRY.register(
    CONF_PCA9554, PCA9554_PIN_SCHEMA, pca9554_pin_final_validate
)
async def pca9554_pin_to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    parent = await cg.get_variable(config[CONF_PCA9554])

    cg.add(var.set_parent(parent))

    num = config[CONF_NUMBER]
    cg.add(var.set_pin(num))
    cg.add(var.set_inverted(config[CONF_INVERTED]))
    cg.add(var.set_flags(pins.gpio_flags_expr(config[CONF_MODE])))
    cg.add(var.set_drive_strength(config[CONF_DRIVE_STRENGTH]))
    cg.add(var.set_latch(config[CONF_MODE]["latch"]))
    cg.add(var.set_interrupt(config[CONF_MODE]["interrupt"]))
    return var
