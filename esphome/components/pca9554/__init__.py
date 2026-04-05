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
    CONF_OPEN_DRAIN,
    CONF_OUTPUT,
    CONF_PULLDOWN,
    CONF_PULLUP,
)

CONF_LATCH = "latch"

CODEOWNERS = ["@hwstar", "@clydebarrow", "@bdraco"]
AUTO_LOAD = ["gpio_expander"]
DEPENDENCIES = ["i2c"]
MULTI_CONF = True
CONF_PIN_COUNT = "pin_count"
pca9554_ns = cg.esphome_ns.namespace("pca9554")

# We could define allowable device addresses in here, but I don't know if that is a good idea.
PCA9554_DEVICE_TYPES = {
    "NONE": {
        "modes": [CONF_INPUT, CONF_OUTPUT],
        "pins": 8,
        "open_drain": False,  # Device has open drain capability
    },
    "PCA9536": {
        "modes": [CONF_INPUT, CONF_OUTPUT],
        "pins": 4,
        "open_drain": False,  # Device has open drain capability
    },
    "PCA9554": {
        "modes": [CONF_INPUT, CONF_OUTPUT],
        "pins": 8,
        "open_drain": False,  # Device has open drain capability
    },
    "PCA9554A": {
        "modes": [CONF_INPUT, CONF_OUTPUT],
        "pins": 8,
        "open_drain": False,  # Device has open drain capability
    },
    "PCA9535": {
        "modes": [CONF_INPUT, CONF_OUTPUT],
        "pins": 16,
        "open_drain": False,  # Device has open drain capability
    },
    "PCAL9554": {
        "modes": [
            CONF_INPUT,
            CONF_OUTPUT,
            CONF_PULLDOWN,
            CONF_PULLUP,
            CONF_INTERRUPT,
            CONF_LATCH,
        ],
        "pins": 8,
        "open_drain": True,  # Device has open drain capability TODO: open drain can only be set for all of port 0 or port 1. Indicate that here?
    },
    "PCAL9555": {
        "modes": [
            CONF_INPUT,
            CONF_OUTPUT,
            CONF_PULLDOWN,
            CONF_PULLUP,
            CONF_INTERRUPT,
            CONF_LATCH,
        ],
        "pins": 16,
        "open_drain": True,  # Device has open drain capability TODO: open drain can only be set for all of port 0 or port 1. Indicate that here?
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
            cv.Optional(CONF_PIN_COUNT): cv.one_of(
                4, 8, 16
            ),  # to maintain backwards compatibility and to also allow the device type to set pin count, we remove the default here. If this is not set, the number of pins is set from the device type using the dict defintion above.
            cv.Optional(
                CONF_OPEN_DRAIN, default=False
            ): cv.boolean,  # This is a per-port setting for the PCAL devices, how do we deal with the user setting this on incompabitle devices? just ignore it? Probably...
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

    if (config[CONF_OPEN_DRAIN]) and not (device_dict["open_drain"]):
        raise cv.Invalid(
            "Device does not support open-drain pin mode"
        )  # This doesnt throw the error properly. It does make an error, but it doesnt look right.

    # This doesnt run until after the pin configs are validated, so we cant set the pin count here only.
    # To maintain backwards compatibiliy, we allow the pin count to be manually set. If it is not manually set, the number of pins is set from the device dict above.
    if CONF_PIN_COUNT in config:
        cg.add(var.set_pin_count(config[CONF_PIN_COUNT]))
    else:
        cg.add(var.set_pin_count(device_dict["pins"]))

    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
    if interrupt_pin := config.get(CONF_INTERRUPT_PIN):
        cg.add(var.set_interrupt_pin(await cg.gpio_pin_expression(interrupt_pin)))


def validate_mode(value):
    # Note: here we cannot validate that the modes entered are supported by the device, only that the modes entered are compatible.
    if not (value[CONF_INPUT] or value[CONF_OUTPUT]):  # TODO: Does this need to be xor?
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
    modes=[
        CONF_INPUT,
        CONF_OUTPUT,
        CONF_PULLDOWN,
        CONF_PULLUP,
        CONF_INTERRUPT,
        CONF_LATCH,
    ],  # These are all possible modes supported by this component, they are not nessecarially the modes supported by the selected device. That is checked later.
    mode_validator=validate_mode,
).extend(
    {
        cv.Required(CONF_PCA9554): cv.use_id(PCA9554Component),
    }
)


def pca9554_pin_final_validate(pin_config, parent_config):
    device_name = parent_config[CONF_DEVICE]
    device_dict = PCA9554_DEVICE_TYPES[device_name]

    # To maintain backwards compatibility, we allow the pin count to be manually set.
    if CONF_PIN_COUNT in parent_config:
        # print("Pin count manually set")
        count = parent_config[CONF_PIN_COUNT]
    else:
        # print("Setting pin count from dict")
        count = device_dict["pins"]
    # print(count)

    # Make sure the user entered a valid pin number
    if pin_config[CONF_NUMBER] >= count:
        raise cv.Invalid(f"Pin number must be in range 0-{count - 1}")

    # Verify that pin modes requested are supported by the device
    #  We also remove entries from the pin_config['mode'] dictionary that are not supported by the device.
    #  This is to distinguish between setting them to 'false' and not setting them at all.
    for entered_mode, active in list(pin_config["mode"].items()):
        if entered_mode not in device_dict["modes"]:
            if active:
                raise cv.Invalid(
                    f"Pin mode '{entered_mode}' is not valid for device {device_name}."
                )
            del pin_config["mode"][entered_mode]


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
    if "latch" in config[CONF_MODE]:
        cg.add(var.set_latch(config[CONF_MODE]["latch"]))
    if "interrupt" in config[CONF_MODE]:
        cg.add(var.set_interrupt(config[CONF_MODE]["interrupt"]))
    return var
