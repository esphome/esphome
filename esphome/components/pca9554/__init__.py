from dataclasses import dataclass

from esphome import pins
import esphome.codegen as cg
from esphome.components import i2c
from esphome.components.gpio_expander import validate_interrupt_pin
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
from esphome.core import CORE, ID
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

# Full drive strength, the power-on value of every PCAL pin.
DEFAULT_DRIVE_STRENGTH = 3


@dataclass(frozen=True)
class DeviceProps:
    """What one expander model offers: its pin count and the pin options it supports."""

    pins: int
    pcal: bool = False

    @property
    def modes(self) -> frozenset[str]:
        if self.pcal:
            return PCAL_MODES
        return BASIC_MODES


BASIC_MODES = frozenset({CONF_INPUT, CONF_OUTPUT})
# The PCAL parts add pull resistors, input latches, a per-pin interrupt mask and drive strength.
PCAL_MODES = BASIC_MODES | {
    CONF_PULLUP,
    CONF_PULLDOWN,
    CONF_INTERRUPT,
    CONF_LATCH,
    CONF_DRIVE_STRENGTH,
}

# "NONE" keeps the pre-`device:` behaviour: a plain 8 pin part unless `pin_count:` says otherwise.
DEVICES: dict[str, DeviceProps] = {
    "NONE": DeviceProps(8),
    "PCA9536": DeviceProps(4),
    "PCA9554": DeviceProps(8),
    "PCA9554A": DeviceProps(8),
    "PCA9535": DeviceProps(16),
    "PCA9555": DeviceProps(16),
    "PCAL9538": DeviceProps(8, pcal=True),
    "PCAL9555": DeviceProps(16, pcal=True),
}


def _pin_count(config: ConfigType) -> int:
    """`pin_count:` stays for existing configs; otherwise the device decides."""
    return config.get(CONF_PIN_COUNT, DEVICES[config[CONF_DEVICE]].pins)


def _validate_hub(config: ConfigType) -> ConfigType:
    """A `pin_count:` that contradicts `device:` would address every register wrongly."""
    device = config[CONF_DEVICE]
    count = config.get(CONF_PIN_COUNT)
    if device != "NONE" and count is not None and count != DEVICES[device].pins:
        raise cv.Invalid(
            f"'{CONF_PIN_COUNT}' ({count}) does not match the {DEVICES[device].pins} "
            f"pins of device {device}; remove '{CONF_PIN_COUNT}'"
        )
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.declare_id(PCA9554Component),
            cv.Optional(CONF_DEVICE, default="NONE"): cv.one_of(*DEVICES, upper=True),
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
    cg.add(var.set_pin_count(_pin_count(config)))
    if DEVICES[config[CONF_DEVICE]].pcal:
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
    if value[CONF_INTERRUPT] and value[CONF_OUTPUT]:
        raise cv.Invalid("Outputs cannot generate interrupts.")
    if value[CONF_LATCH] and value[CONF_OUTPUT]:
        raise cv.Invalid("Outputs cannot be latched.")
    return value


PCA9554_PIN_SCHEMA = pins.gpio_base_schema(
    PCA9554GPIOPin,
    cv.int_range(min=0, max=15),
    # Every mode any supported device offers; the final validation checks the chosen device.
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
        cv.Required(CONF_PCA9554): pins.use_id_or_address(PCA9554Component),
        cv.Optional(CONF_DRIVE_STRENGTH, default=DEFAULT_DRIVE_STRENGTH): cv.int_range(
            min=0, max=3
        ),
    }
)


def pca9554_pin_final_validate(
    pin_config: ConfigType, parent_config: ConfigType
) -> None:
    device_name = parent_config[CONF_DEVICE]
    device = DEVICES[device_name]

    count = _pin_count(parent_config)
    if pin_config[CONF_NUMBER] >= count:
        raise cv.Invalid(f"Pin number must be in range 0-{count - 1}")

    mode = pin_config[CONF_MODE]
    for name, active in mode.items():
        if active and name not in device.modes:
            raise cv.Invalid(
                f"Pin mode '{name}' is not valid for device {device_name}."
            )
    if (
        pin_config[CONF_DRIVE_STRENGTH] != DEFAULT_DRIVE_STRENGTH
        and CONF_DRIVE_STRENGTH not in device.modes
    ):
        raise cv.Invalid(
            f"'{CONF_DRIVE_STRENGTH}' is not supported by device {device_name}"
        )
    # The PCAL interrupt mask powers up with every pin masked, so an input that does not
    # opt in never asserts INT and, with an interrupt pin, is never read again.
    if (
        device.pcal
        and CONF_INTERRUPT_PIN in parent_config
        and mode[CONF_INPUT]
        and not mode[CONF_INTERRUPT]
    ):
        raise cv.Invalid(
            f"Input pin {pin_config[CONF_NUMBER]} of {device_name} needs "
            f"'{CONF_INTERRUPT}: true' in its mode, or it never updates once "
            f"'{CONF_INTERRUPT_PIN}' is set"
        )


def _hub_config(hub_id: ID) -> ConfigType:
    """The validated config of the hub a pin belongs to."""
    for hub in CORE.config[DOMAIN]:
        if hub[CONF_ID] == hub_id:
            return hub
    raise KeyError(hub_id)


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
    # The setters only exist in a build with a PCAL part, so only such a pin gets them
    if DEVICES[_hub_config(config[CONF_PCA9554])[CONF_DEVICE]].pcal:
        mode = config[CONF_MODE]
        cg.add(var.set_latch(mode[CONF_LATCH]))
        cg.add(var.set_interrupt(mode[CONF_INTERRUPT]))
        cg.add(var.set_drive_strength(config[CONF_DRIVE_STRENGTH]))
    return var
