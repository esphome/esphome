from esphome import pins
import esphome.codegen as cg
from esphome.components import i2c
from esphome.components.gpio_expander import validate_interrupt_pin
import esphome.config_validation as cv
from esphome.const import (
    CONF_DEVICE,
    CONF_ID,
    CONF_INPUT,
    CONF_INTERRUPT_PIN,
    CONF_INVERTED,
    CONF_MODE,
    CONF_NUMBER,
    CONF_OUTPUT,
    CONF_PULLDOWN,
    CONF_PULLUP,
)
from esphome.cpp_generator import MockObj
from esphome.types import ConfigType

CODEOWNERS = ["@hwstar", "@clydebarrow", "@bdraco"]
AUTO_LOAD = ["gpio_expander"]
DEPENDENCIES = ["i2c"]
DOMAIN = "pca9554"
MULTI_CONF = True

CONF_DRIVE_STRENGTH = "drive_strength"
CONF_LATCH = "latch"
CONF_PCA9554 = "pca9554"
CONF_PIN_COUNT = "pin_count"

pca9554_ns = cg.esphome_ns.namespace("pca9554")
PCA9554Component = pca9554_ns.class_("PCA9554Component", cg.Component, i2c.I2CDevice)
PCA9554GPIOPin = pca9554_ns.class_(
    "PCA9554GPIOPin", cg.GPIOPin, cg.Parented.template(PCA9554Component)
)

# Pin count of each supported model
DEVICES: dict[str, int] = {
    "PCA9536": 4,
    "PCA9554": 8,
    "PCA9554A": 8,
    "PCA9535": 16,
    "PCA9555": 16,
    "PCAL9538": 8,
    "PCAL9555": 16,
}
PCAL_DEVICES = frozenset({"PCAL9538", "PCAL9555"})
PCAL_ONLY_MODES = (CONF_PULLUP, CONF_PULLDOWN, CONF_LATCH)
# The device a legacy `pin_count:` stands for
_LEGACY_DEVICES = {4: "PCA9536", 8: "PCA9554", 16: "PCA9555"}


def _validate_hub(config: ConfigType) -> ConfigType:
    count = config.get(CONF_PIN_COUNT)
    if (device := config.get(CONF_DEVICE)) is None:
        device = _LEGACY_DEVICES[count] if count is not None else "PCA9554"
        config[CONF_DEVICE] = device
    elif count is not None and count != DEVICES[device]:
        raise cv.Invalid(
            f"'{CONF_PIN_COUNT}' ({count}) does not match the {DEVICES[device]} "
            f"pins of device {device}; remove '{CONF_PIN_COUNT}'"
        )
    config[CONF_PIN_COUNT] = DEVICES[device]
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.declare_id(PCA9554Component),
            cv.Optional(CONF_DEVICE): cv.one_of(*DEVICES, upper=True),
            cv.Optional(CONF_PIN_COUNT): cv.one_of(4, 8, 16),
            cv.Optional(CONF_INTERRUPT_PIN): validate_interrupt_pin,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    # 0x20 for the non-A parts; the PCA9554A parts start at 0x38
    .extend(i2c.i2c_device_schema(0x20)),
    _validate_hub,
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    cg.add(var.set_pin_count(config[CONF_PIN_COUNT]))
    if config[CONF_DEVICE] in PCAL_DEVICES:
        # Only a build with a PCAL part carries the code for its extra registers
        cg.add_define("USE_PCA9554_PCAL")
        cg.add(var.set_pcal(True))

    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
    if interrupt_pin := config.get(CONF_INTERRUPT_PIN):
        cg.add(var.set_interrupt_pin(await cg.gpio_pin_expression(interrupt_pin)))


def validate_mode(value: ConfigType) -> ConfigType:
    """The modes have to agree with each other; whether the device offers them is checked later."""
    if value[CONF_INPUT] == value[CONF_OUTPUT]:
        raise cv.Invalid("Mode must be either input or output")
    if value[CONF_PULLDOWN] and value[CONF_PULLUP]:
        raise cv.Invalid("Pull-up and pull-down are mutually exclusive.")
    if value[CONF_LATCH] and value[CONF_OUTPUT]:
        raise cv.Invalid("Outputs cannot be latched.")
    return value


PCA9554_PIN_SCHEMA = pins.gpio_base_schema(
    PCA9554GPIOPin,
    cv.int_range(min=0, max=15),
    modes=[CONF_INPUT, CONF_OUTPUT, *PCAL_ONLY_MODES],
    mode_validator=validate_mode,
).extend(
    {
        cv.Required(CONF_PCA9554): pins.use_id_or_address(PCA9554Component),
        cv.Optional(CONF_DRIVE_STRENGTH): cv.int_range(min=0, max=3),
    }
)


def pca9554_pin_final_validate(
    pin_config: ConfigType, parent_config: ConfigType
) -> None:
    count = parent_config[CONF_PIN_COUNT]
    if pin_config[CONF_NUMBER] >= count:
        raise cv.Invalid(f"Pin number must be in range 0-{count - 1}")

    device = parent_config[CONF_DEVICE]
    if device in PCAL_DEVICES:
        return
    mode = pin_config[CONF_MODE]
    for name in PCAL_ONLY_MODES:
        if mode[name]:
            raise cv.Invalid(f"Pin mode '{name}' is not valid for device {device}.")
    if CONF_DRIVE_STRENGTH in pin_config:
        raise cv.Invalid(f"'{CONF_DRIVE_STRENGTH}' is not supported by device {device}")


@pins.PIN_SCHEMA_REGISTRY.register(
    CONF_PCA9554, PCA9554_PIN_SCHEMA, pca9554_pin_final_validate
)
async def pca9554_pin_to_code(config: ConfigType) -> MockObj:
    var = cg.new_Pvariable(config[CONF_ID])
    parent = await cg.get_variable(config[CONF_PCA9554])

    cg.add(var.set_parent(parent))

    num = config[CONF_NUMBER]
    cg.add(var.set_pin(num))
    cg.add(var.set_inverted(config[CONF_INVERTED]))
    cg.add(var.set_flags(pins.gpio_flags_expr(config[CONF_MODE])))
    # Final validation only lets these through on a PCAL hub
    if config[CONF_MODE][CONF_LATCH]:
        cg.add(parent.set_latch(num))
    if (drive_strength := config.get(CONF_DRIVE_STRENGTH)) is not None:
        cg.add(parent.set_drive_strength(num, drive_strength))
    return var
