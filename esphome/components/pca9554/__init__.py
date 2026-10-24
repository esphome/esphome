from typing import NamedTuple

from esphome import pins
import esphome.codegen as cg
from esphome.components import gpio_expander, i2c
from esphome.components.const import CONF_DRIVE_STRENGTH
import esphome.config_validation as cv
from esphome.const import (
    CONF_ADDRESS,
    CONF_DEVICE,
    CONF_I2C_ID,
    CONF_ID,
    CONF_INPUT,
    CONF_INTERRUPT_PIN,
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

CONF_LATCH = "latch"
CONF_PCA9554 = "pca9554"
CONF_PIN_COUNT = "pin_count"

pca9554_ns = cg.esphome_ns.namespace("pca9554")
PCA9554Component = pca9554_ns.class_("PCA9554Component", cg.Component, i2c.I2CDevice)
PCA9554GPIOPin = pca9554_ns.class_(
    "PCA9554GPIOPin", cg.GPIOPin, cg.Parented.template(PCA9554Component)
)


class _Device(NamedTuple):
    pins: int
    # Lowest I2C address, with all address pins low
    address: int
    # Has the PCAL registers: pull resistors, input latch, drive strength, interrupt mask
    pcal: bool = False


DEVICES: dict[str, _Device] = {
    "PCA9536": _Device(4, 0x41),
    "PCA9534": _Device(8, 0x20),
    "PCA9534A": _Device(8, 0x38),
    "PCA9538": _Device(8, 0x70),
    "PCA9554": _Device(8, 0x20),
    "PCA9554A": _Device(8, 0x38),
    "TCA9554": _Device(8, 0x20),
    "PCA9535": _Device(16, 0x20),
    "PCA9555": _Device(16, 0x20),
    "TCA9535": _Device(16, 0x20),
    "TCA9555": _Device(16, 0x20),
    "PCAL9538": _Device(8, 0x70, True),
    "PCAL9555": _Device(16, 0x20, True),
}
# The device a legacy `pin_count:` stands for
_LEGACY_DEVICES = {4: "PCA9536", 8: "PCA9554", 16: "PCA9555"}
# Datasheet drive levels and their register codes
DRIVE_STRENGTHS = {0.25: 0, 0.5: 1, 0.75: 2, 1.0: 3}


def _validate_hub(config: ConfigType) -> ConfigType:
    count = config.get(CONF_PIN_COUNT)
    if (device := config.get(CONF_DEVICE)) is None:
        device = config[CONF_DEVICE] = _LEGACY_DEVICES[8 if count is None else count]
    elif count is not None and count != DEVICES[device].pins:
        raise cv.Invalid(
            f"'{CONF_PIN_COUNT}' ({count}) does not match the {DEVICES[device].pins} "
            f"pins of device {device}; remove '{CONF_PIN_COUNT}'"
        )
    config.setdefault(CONF_ADDRESS, DEVICES[device].address)
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.declare_id(PCA9554Component),
            cv.GenerateID(CONF_I2C_ID): cv.use_id(i2c.I2CBus),
            # The default depends on the device, so _validate_hub fills it in
            cv.Optional(CONF_ADDRESS): cv.i2c_address,
            cv.Optional(CONF_DEVICE): cv.one_of(*DEVICES, upper=True),
            cv.Optional(CONF_PIN_COUNT): cv.one_of(4, 8, 16),
        }
    ).extend(gpio_expander.INTERRUPT_EXPANDER_SCHEMA),
    _validate_hub,
)


async def to_code(config: ConfigType) -> None:
    device = DEVICES[config[CONF_DEVICE]]
    var = await i2c.new_i2c_component(config, device.pins)
    if device.pcal:
        # Only a build with a PCAL part carries the code for its extra registers
        cg.add_define("USE_PCA9554_PCAL")
        cg.add(var.set_pcal())
    await cg.add_pin(var.set_interrupt_pin, config.get(CONF_INTERRUPT_PIN))


def validate_mode(value: ConfigType) -> ConfigType:
    """Reject contradictory modes; whether the device supports a mode is checked in final validation."""
    pins.gpio_validate_input_xor_output(value)
    if value[CONF_PULLDOWN] and value[CONF_PULLUP]:
        raise cv.Invalid("Pull-up and pull-down are mutually exclusive")
    return value


PCA9554_PIN_SCHEMA = pins.gpio_base_schema(
    PCA9554GPIOPin,
    cv.int_range(min=0, max=15),
    modes=[CONF_INPUT, CONF_OUTPUT, CONF_PULLUP, CONF_PULLDOWN],
    mode_validator=validate_mode,
).extend(
    {
        cv.Required(CONF_PCA9554): pins.use_id_or_address(PCA9554Component),
        cv.Optional(CONF_LATCH, default=False): cv.boolean,
        cv.Optional(CONF_DRIVE_STRENGTH): cv.All(
            cv.percentage, cv.one_of(*DRIVE_STRENGTHS)
        ),
    }
)


def pca9554_pin_final_validate(
    pin_config: ConfigType, parent_config: ConfigType
) -> None:
    device_name = parent_config[CONF_DEVICE]
    device = DEVICES[device_name]
    if pin_config[CONF_NUMBER] >= device.pins:
        raise cv.Invalid(f"Pin number must be in range 0-{device.pins - 1}")

    mode = pin_config[CONF_MODE]
    if pin_config[CONF_LATCH] and mode[CONF_OUTPUT]:
        raise cv.Invalid(f"'{CONF_LATCH}' is not valid for an output pin")
    if CONF_DRIVE_STRENGTH in pin_config and mode[CONF_INPUT]:
        raise cv.Invalid(f"'{CONF_DRIVE_STRENGTH}' is not valid for an input pin")
    if device.pcal:
        return
    for name in (CONF_PULLUP, CONF_PULLDOWN):
        if mode[name]:
            raise cv.Invalid(f"Pin mode '{name}' is not valid for device {device_name}")
    for key in (CONF_LATCH, CONF_DRIVE_STRENGTH):
        if pin_config.get(key):
            raise cv.Invalid(f"'{key}' is not supported by device {device_name}")


@pins.PIN_SCHEMA_REGISTRY.register(
    CONF_PCA9554, PCA9554_PIN_SCHEMA, pca9554_pin_final_validate
)
async def pca9554_pin_to_code(config: ConfigType) -> MockObj:
    var = await pins.new_parented_pin(config, CONF_PCA9554)
    parent = await cg.get_variable(config[CONF_PCA9554])
    num = config[CONF_NUMBER]
    # Final validation only lets these through on a PCAL hub
    if config[CONF_LATCH]:
        cg.add(parent.set_latch(num))
    if (drive_strength := config.get(CONF_DRIVE_STRENGTH)) is not None:
        cg.add(parent.set_drive_strength(num, DRIVE_STRENGTHS[drive_strength]))
    return var
