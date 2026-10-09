"""Shared code for Trinamic stepper drivers configured over the single wire UART interface."""

from esphome import automation, pins
import esphome.codegen as cg
from esphome.components import stepper, uart
import esphome.config_validation as cv
from esphome.const import (
    CONF_ADDRESS,
    CONF_DIR_PIN,
    CONF_ID,
    CONF_PLATFORM,
    CONF_STEP_PIN,
    CONF_UART_ID,
)
from esphome.cpp_generator import MockObjClass
import esphome.final_validate as fv
from esphome.types import ConfigType

CODEOWNERS = ["@remcom"]
DOMAIN = "tmc22xx"
DEPENDENCIES = ["uart"]

CONF_ANALOG_CURRENT_SCALE = "analog_current_scale"
CONF_CLOCK_FREQUENCY = "clock_frequency"
CONF_ENABLE_SPREADCYCLE = "enable_spreadcycle"
CONF_ENN_PIN = "enn_pin"
CONF_HOLD_CURRENT = "hold_current"
CONF_IHOLD = "ihold"
CONF_IHOLDDELAY = "iholddelay"
CONF_INDEX_PIN = "index_pin"
CONF_INTERPOLATION = "interpolation"
CONF_INVERSE_DIRECTION = "inverse_direction"
CONF_IRUN = "irun"
CONF_MICROSTEPS = "microsteps"
CONF_OTTRIM = "ottrim"
CONF_RSENSE = "rsense"
CONF_RUN_CURRENT = "run_current"
CONF_STANDSTILL_MODE = "standstill_mode"
CONF_TPOWERDOWN = "tpowerdown"
CONF_TPWM_THRESHOLD = "tpwm_threshold"
CONF_VSENSE = "vsense"

tmc22xx_ns = cg.esphome_ns.namespace("tmc22xx")
TMC22XXStepper = tmc22xx_ns.class_(
    "TMC22XXStepper", stepper.Stepper, cg.Component, uart.UARTDevice
)
StandstillMode = tmc22xx_ns.enum("StandstillMode")

STANDSTILL_MODES = {
    "normal": StandstillMode.STANDSTILL_MODE_NORMAL,
    "freewheeling": StandstillMode.STANDSTILL_MODE_FREEWHEELING,
    "coil_short_ls": StandstillMode.STANDSTILL_MODE_COIL_SHORT_LS,
    "coil_short_hs": StandstillMode.STANDSTILL_MODE_COIL_SHORT_HS,
}

validate_microsteps = cv.one_of(1, 2, 4, 8, 16, 32, 64, 128, 256, int=True)
validate_current = cv.All(cv.current, cv.positive_float)


def tmc22xx_schema(cls: MockObjClass, max_address: int) -> cv.Schema:
    """Return the stepper schema for a family member; `max_address` is 0 for chips without address pins."""
    return cv.All(
        stepper.STEPPER_SCHEMA.extend(
            {
                cv.GenerateID(): cv.declare_id(cls),
                cv.Optional(CONF_ADDRESS, default=0): cv.int_range(0, max_address),
                cv.Optional(CONF_ENN_PIN): pins.gpio_output_pin_schema,
                cv.Optional(CONF_STEP_PIN): pins.gpio_output_pin_schema,
                cv.Optional(CONF_DIR_PIN): pins.gpio_output_pin_schema,
                cv.Optional(CONF_INDEX_PIN): pins.internal_gpio_input_pin_schema,
                cv.Optional(CONF_CLOCK_FREQUENCY, default="12MHz"): cv.All(
                    cv.frequency, cv.int_range(min=1)
                ),
                cv.Optional(CONF_RSENSE): cv.All(
                    cv.resistance, cv.float_range(min=0.0, min_included=False)
                ),
                cv.Optional(CONF_VSENSE): cv.boolean,
                cv.Optional(CONF_OTTRIM): cv.int_range(0, 3),
                cv.Optional(CONF_ANALOG_CURRENT_SCALE, default=False): cv.boolean,
                cv.Optional(CONF_RUN_CURRENT): validate_current,
                cv.Optional(CONF_HOLD_CURRENT): validate_current,
                cv.Optional(CONF_MICROSTEPS): validate_microsteps,
            }
        )
        .extend(cv.COMPONENT_SCHEMA)
        .extend(uart.UART_DEVICE_SCHEMA),
        cv.has_exactly_one_key(CONF_INDEX_PIN, CONF_STEP_PIN),
        cv.has_none_or_all_keys(CONF_STEP_PIN, CONF_DIR_PIN),
    )


def final_validate(config: ConfigType) -> ConfigType:
    """Reject two drivers with the same address on one UART bus."""
    for other in fv.full_config.get().get("stepper", []):
        if other is config:
            break
        if (
            other[CONF_PLATFORM] == config[CONF_PLATFORM]
            and other[CONF_UART_ID] == config[CONF_UART_ID]
            and other[CONF_ADDRESS] == config[CONF_ADDRESS]
        ):
            raise cv.Invalid(
                f"Address {config[CONF_ADDRESS]} is already used by '{other[CONF_ID]}' "
                "on the same UART bus"
            )
    return config


async def new_tmc22xx(config: ConfigType) -> cg.Pvariable:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    await stepper.register_stepper(var, config)

    cg.add(var.set_address(config[CONF_ADDRESS]))
    cg.add(var.set_clock_frequency(config[CONF_CLOCK_FREQUENCY]))
    cg.add(var.set_analog_current_scale(config[CONF_ANALOG_CURRENT_SCALE]))
    for key, setter in (
        (CONF_ENN_PIN, var.set_enn_pin),
        (CONF_STEP_PIN, var.set_step_pin),
        (CONF_DIR_PIN, var.set_dir_pin),
        (CONF_INDEX_PIN, var.set_index_pin),
    ):
        if (pin := config.get(key)) is not None:
            cg.add(setter(await cg.gpio_pin_expression(pin)))
    for key, setter in (
        (CONF_RSENSE, var.set_rsense),
        (CONF_VSENSE, var.set_vsense),
        (CONF_OTTRIM, var.set_ottrim),
        (CONF_RUN_CURRENT, var.set_initial_run_current),
        (CONF_HOLD_CURRENT, var.set_initial_hold_current),
        (CONF_MICROSTEPS, var.set_initial_microsteps),
    ):
        if (value := config.get(key)) is not None:
            cg.add(setter(value))
    return var


def register_actions(domain: str, cls: MockObjClass) -> None:
    """Register the actions shared by the family under `domain`, e.g. `tmc2209.enable`."""
    id_schema = {cv.GenerateID(): cv.use_id(cls)}
    for name, enabled in (("enable", "true"), ("disable", "false")):
        automation.register_apply_action(
            f"{domain}.{name}",
            automation.maybe_simple_id(id_schema),
            automation.ApplyCall(f"set_enabled({enabled})"),
        )

    automation.register_apply_action(
        f"{domain}.configure",
        cv.Schema(
            {
                **id_schema,
                cv.Optional(CONF_INVERSE_DIRECTION): cv.templatable(cv.boolean),
                cv.Optional(CONF_MICROSTEPS): cv.templatable(validate_microsteps),
                cv.Optional(CONF_INTERPOLATION): cv.templatable(cv.boolean),
                cv.Optional(CONF_ENABLE_SPREADCYCLE): cv.templatable(cv.boolean),
                cv.Optional(CONF_TPWM_THRESHOLD): cv.templatable(
                    cv.int_range(0, 2**20 - 1)
                ),
            }
        ),
        automation.ApplyField(
            CONF_INVERSE_DIRECTION, "set_inverse_direction", cg.bool_
        ),
        automation.ApplyField(CONF_MICROSTEPS, "set_microsteps", cg.uint16),
        automation.ApplyField(CONF_INTERPOLATION, "set_interpolation", cg.bool_),
        automation.ApplyField(CONF_ENABLE_SPREADCYCLE, "set_spreadcycle", cg.bool_),
        automation.ApplyField(CONF_TPWM_THRESHOLD, "set_tpwm_threshold", cg.uint32),
    )

    automation.register_apply_action(
        f"{domain}.currents",
        cv.Schema(
            {
                **id_schema,
                cv.Exclusive(CONF_RUN_CURRENT, "run"): cv.templatable(validate_current),
                cv.Exclusive(CONF_IRUN, "run"): cv.templatable(cv.int_range(0, 31)),
                cv.Exclusive(CONF_HOLD_CURRENT, "hold"): cv.templatable(
                    validate_current
                ),
                cv.Exclusive(CONF_IHOLD, "hold"): cv.templatable(cv.int_range(0, 31)),
                cv.Optional(CONF_IHOLDDELAY): cv.templatable(cv.int_range(0, 15)),
                cv.Optional(CONF_TPOWERDOWN): cv.templatable(cv.int_range(0, 255)),
                cv.Optional(CONF_STANDSTILL_MODE): cv.templatable(
                    cv.enum(STANDSTILL_MODES, lower=True)
                ),
            }
        ),
        automation.ApplyField(CONF_RUN_CURRENT, "set_run_current", cg.float_),
        automation.ApplyField(CONF_IRUN, "set_irun", cg.uint8),
        automation.ApplyField(CONF_HOLD_CURRENT, "set_hold_current", cg.float_),
        automation.ApplyField(CONF_IHOLD, "set_ihold", cg.uint8),
        automation.ApplyField(CONF_IHOLDDELAY, "set_iholddelay", cg.uint8),
        automation.ApplyField(CONF_TPOWERDOWN, "set_tpowerdown", cg.uint8),
        automation.ApplyField(
            CONF_STANDSTILL_MODE, "set_standstill_mode", StandstillMode
        ),
    )
