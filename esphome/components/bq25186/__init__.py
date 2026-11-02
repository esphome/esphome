from collections.abc import Callable
from typing import Any

from esphome import automation
import esphome.codegen as cg
from esphome.components import i2c
from esphome.components.const import CONF_CHARGE_ENABLED, CONF_INPUT_CURRENT_LIMIT
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_UPDATE_INTERVAL, CONF_WATCHDOG
from esphome.core import CORE, ID, EsphomeError, Lambda, TimePeriod
import esphome.final_validate as fv
from esphome.types import ConfigType, TemplateArgsType

CODEOWNERS = ["@Rapsssito"]
DEPENDENCIES = ["i2c"]
DOMAIN = "bq25186"
MULTI_CONF = True

CONF_BQ25186_ID = "bq25186_id"
CONF_CONFIGURE_ON_BOOT = "configure_on_boot"
CONF_BATTERY_REGULATION_VOLTAGE = "battery_regulation_voltage"
CONF_FAST_CHARGE_CURRENT = "fast_charge_current"
CONF_FLASH_CHARGING_MODE = "flash_charging_mode"
CONF_PRECHARGE_IS_ITERM = "precharge_is_iterm"
CONF_TERMINATION_PERCENT = "termination_percent"
CONF_VINDPM_MODE = "vindpm_mode"
CONF_THERMAL_REGULATION = "thermal_regulation"
CONF_BATTERY_OCP_LIMIT = "battery_ocp_limit"
CONF_BATTERY_UVLO = "battery_uvlo"
CONF_MASK_CHARGE_STATUS_INTERRUPT = "mask_charge_status_interrupt"
CONF_MASK_ILIM_INTERRUPT = "mask_ilim_interrupt"
CONF_MASK_VINDPM_INTERRUPT = "mask_vindpm_interrupt"
CONF_TS_AUTO_FUNCTION = "ts_auto_function"
CONF_VLOWV = "vlowv"
CONF_RECHARGE_THRESHOLD = "recharge_threshold"
CONF_DOUBLE_TIMER_DURING_DPM = "double_timer_during_dpm"
CONF_SAFETY_TIMER = "safety_timer"
CONF_LONG_PRESS_DURATION = "long_press_duration"
CONF_HW_RESET_REQUIRES_VIN = "hw_reset_requires_vin"
CONF_AUTOWAKE = "autowake"
CONF_PUSH_BUTTON_LONG_PRESS_ACTION = "push_button_long_press_action"
CONF_WAKE1_TIMER = "wake1_timer"
CONF_WAKE2_TIMER = "wake2_timer"
CONF_ENABLE_PUSH_BUTTON = "enable_push_button"
CONF_SYSTEM_REGULATION = "system_regulation"
CONF_SYS_MODE = "sys_mode"
CONF_WATCHDOG_15S_ENABLE = "watchdog_15s_enable"
CONF_DISABLE_VDPPM = "disable_vdppm"
CONF_TS_HOT = "ts_hot"
CONF_TS_COLD = "ts_cold"
CONF_TS_WARM_DISABLE = "ts_warm_disable"
CONF_TS_COOL_DISABLE = "ts_cool_disable"
CONF_TS_ICHG = "ts_ichg"
CONF_TS_VRCG = "ts_vrcg"
CONF_MASK_TS_INTERRUPT = "mask_ts_interrupt"
CONF_MASK_TREG_INTERRUPT = "mask_treg_interrupt"
CONF_MASK_BAT_INTERRUPT = "mask_bat_interrupt"
CONF_MASK_PG_INTERRUPT = "mask_pg_interrupt"

bq25186_ns = cg.esphome_ns.namespace("bq25186")
BQ25186Component = bq25186_ns.class_(
    "BQ25186Component", cg.PollingComponent, i2c.I2CDevice
)

BQ25186_CHILD_SCHEMA = cv.Schema(
    {cv.GenerateID(CONF_BQ25186_ID): cv.use_id(BQ25186Component)}
)
_request_binary_sensor_slot = cg.slot_counter("BQ25186_BINARY_SENSOR_COUNT")
_request_text_sensor_slot = cg.slot_counter("BQ25186_TEXT_SENSOR_COUNT")


def register_binary_sensor(hub: cg.MockObj, var: cg.MockObj) -> cg.Expression:
    _request_binary_sensor_slot(str(hub))
    return hub.register_binary_sensor(var)


def register_text_sensor(hub: cg.MockObj, var: cg.MockObj) -> cg.Expression:
    _request_text_sensor_slot(str(hub))
    return hub.register_text_sensor(var)


REG_VBAT_CTRL = 0x03
REG_ICHG_CTRL = 0x04
REG_CHARGECTRL0 = 0x05
REG_CHARGECTRL1 = 0x06
REG_IC_CTRL = 0x07
REG_TMR_ILIM = 0x08
REG_SHIP_RST = 0x09
REG_SYS_REG = 0x0A
REG_TS_CONTROL = 0x0B
REG_MASK_ID = 0x0C
REGISTER_COUNT = REG_MASK_ID - REG_VBAT_CTRL + 1

TERMINATION_PERCENT_OPTIONS = {"disabled": 0, "5%": 1, "10%": 2, "20%": 3}
VINDPM_MODE_OPTIONS = {"battery_tracking": 0, "4.5v": 1, "4.7v": 2, "disabled": 3}
THERM_REGULATION_OPTIONS = {"100c": 0, "80c": 1, "60c": 2, "disabled": 3}
BATTERY_OCP_LIMIT_OPTIONS = {"500ma": 0, "1000ma": 1, "1500ma": 2, "3000ma": 3}
BATTERY_UVLO_OPTIONS = {
    "3.0v": 0,
    "2.8v": 3,
    "2.6v": 4,
    "2.4v": 5,
    "2.2v": 6,
    "2.0v": 7,
}
VLOWV_OPTIONS = {"3.0v": 0, "2.8v": 1}
RECHARGE_THRESHOLD_OPTIONS = {"100mv": 0, "200mv": 1}
SAFETY_TIMER_OPTIONS = {"3h": 0, "6h": 1, "12h": 2, "disabled": 3}
# The I2C watchdog restores the power-on registers when the bus stays quiet for its period
WATCHDOG_OPTIONS = {
    "160s_software_reset": 0,
    "160s_hardware_reset": 1,
    "40s_hardware_reset": 2,
    "disabled": 3,
}
WATCHDOG_PERIOD_S = {
    "160s_software_reset": 160,
    "160s_hardware_reset": 160,
    "40s_hardware_reset": 40,
}
# SYS_REG watchdog_15s_enable: a hardware reset when the bus stays quiet for 15 s after VIN connects
WATCHDOG_15S_PERIOD_S = 15
# The chip boots with the 160 s software reset watchdog, which runs until the configuration is applied
POWER_ON_WATCHDOG_PERIOD_S = 160
LONG_PRESS_DURATION_OPTIONS = {"5s": 0, "10s": 1, "15s": 2, "20s": 3}
AUTOWAKE_OPTIONS = {"0.5s": 0, "1s": 1, "2s": 2, "4s": 3}
INPUT_CURRENT_LIMIT_OPTIONS = {
    "50ma": 0,
    "100ma": 1,
    "200ma": 2,
    "300ma": 3,
    "400ma": 4,
    "500ma": 5,
    "665ma": 6,
    "1050ma": 7,
}
PUSH_BUTTON_LONG_PRESS_ACTION_OPTIONS = {
    "none": 0,
    "hardware_reset": 1,
    "ship_mode": 2,
    "shutdown_mode": 3,
}
WAKE1_TIMER_OPTIONS = {"300ms": 0, "1s": 1}
WAKE2_TIMER_OPTIONS = {"2s": 0, "3s": 1}
SYSTEM_REGULATION_OPTIONS = {
    "battery_tracking": 0,
    "4.4v": 1,
    "4.5v": 2,
    "4.6v": 3,
    "4.7v": 4,
    "4.8v": 5,
    "4.9v": 6,
    "pass_through": 7,
}
SYS_MODE_OPTIONS = {
    "normal": 0,
    "battery_only": 1,
    "floating_off": 2,
    "pulldown_off": 3,
}
TS_HOT_OPTIONS = {"60c": 0, "65c": 1, "50c": 2, "45c": 3}
TS_COLD_OPTIONS = {"0c": 0, "3c": 1, "5c": 2, "-3c": 3}
TS_ICHG_OPTIONS = {"0.5x": 0, "0.2x": 1}
TS_VRCG_OPTIONS = {"-100mv": 0, "-200mv": 1}


def _on_voltage_step(volts: float) -> float:
    millivolts = volts * 1000
    if abs(millivolts - round(millivolts / 10) * 10) > 1e-6:
        raise cv.Invalid("battery_regulation_voltage must be a multiple of 10 mV")
    return volts


def _on_current_step(amps: float) -> float:
    milliamps = amps * 1000
    step = 1 if milliamps < 36 else 10
    if abs(milliamps - round(milliamps / step) * step) > 1e-6 or 35 < milliamps < 40:
        raise cv.Invalid(
            "fast_charge_current must be a whole number of mA up to 35mA, then a multiple of 10 mA"
        )
    return amps


# (config key, register, bit shift, validator or enum options, default).
# VBAT_CTRL bit 7 (pg_mode) and SYS_REG bit 4 (pg_gpo_level) stay 0; the PG/GPO
# switch drives them.
FIELDS: tuple[tuple[str, int, int, Any, Any], ...] = (
    (
        CONF_BATTERY_REGULATION_VOLTAGE,
        REG_VBAT_CTRL,
        0,
        cv.All(cv.voltage, cv.Range(min=3.5, max=4.65), _on_voltage_step),
        "4.2V",
    ),
    (CONF_CHARGE_ENABLED, REG_ICHG_CTRL, 7, cv.boolean, True),
    (
        CONF_FAST_CHARGE_CURRENT,
        REG_ICHG_CTRL,
        0,
        cv.All(cv.current, cv.Range(min=0.005, max=1.0), _on_current_step),
        "10mA",
    ),
    (CONF_FLASH_CHARGING_MODE, REG_CHARGECTRL0, 7, cv.boolean, False),
    (CONF_PRECHARGE_IS_ITERM, REG_CHARGECTRL0, 6, cv.boolean, False),
    (CONF_TERMINATION_PERCENT, REG_CHARGECTRL0, 4, TERMINATION_PERCENT_OPTIONS, "10%"),
    (CONF_VINDPM_MODE, REG_CHARGECTRL0, 2, VINDPM_MODE_OPTIONS, "4.5v"),
    (CONF_THERMAL_REGULATION, REG_CHARGECTRL0, 0, THERM_REGULATION_OPTIONS, "100c"),
    (CONF_BATTERY_OCP_LIMIT, REG_CHARGECTRL1, 6, BATTERY_OCP_LIMIT_OPTIONS, "3000ma"),
    (CONF_BATTERY_UVLO, REG_CHARGECTRL1, 3, BATTERY_UVLO_OPTIONS, "3.0v"),
    (CONF_MASK_CHARGE_STATUS_INTERRUPT, REG_CHARGECTRL1, 2, cv.boolean, True),
    (CONF_MASK_ILIM_INTERRUPT, REG_CHARGECTRL1, 1, cv.boolean, True),
    (CONF_MASK_VINDPM_INTERRUPT, REG_CHARGECTRL1, 0, cv.boolean, False),
    (CONF_TS_AUTO_FUNCTION, REG_IC_CTRL, 7, cv.boolean, True),
    (CONF_VLOWV, REG_IC_CTRL, 6, VLOWV_OPTIONS, "3.0v"),
    (CONF_RECHARGE_THRESHOLD, REG_IC_CTRL, 5, RECHARGE_THRESHOLD_OPTIONS, "100mv"),
    (CONF_DOUBLE_TIMER_DURING_DPM, REG_IC_CTRL, 4, cv.boolean, False),
    (CONF_SAFETY_TIMER, REG_IC_CTRL, 2, SAFETY_TIMER_OPTIONS, "6h"),
    (CONF_WATCHDOG, REG_IC_CTRL, 0, WATCHDOG_OPTIONS, "disabled"),
    (CONF_LONG_PRESS_DURATION, REG_TMR_ILIM, 6, LONG_PRESS_DURATION_OPTIONS, "10s"),
    (CONF_HW_RESET_REQUIRES_VIN, REG_TMR_ILIM, 5, cv.boolean, False),
    (CONF_AUTOWAKE, REG_TMR_ILIM, 3, AUTOWAKE_OPTIONS, "1s"),
    (CONF_INPUT_CURRENT_LIMIT, REG_TMR_ILIM, 0, INPUT_CURRENT_LIMIT_OPTIONS, "500ma"),
    (
        CONF_PUSH_BUTTON_LONG_PRESS_ACTION,
        REG_SHIP_RST,
        3,
        PUSH_BUTTON_LONG_PRESS_ACTION_OPTIONS,
        "ship_mode",
    ),
    (CONF_WAKE1_TIMER, REG_SHIP_RST, 2, WAKE1_TIMER_OPTIONS, "300ms"),
    (CONF_WAKE2_TIMER, REG_SHIP_RST, 1, WAKE2_TIMER_OPTIONS, "2s"),
    (CONF_ENABLE_PUSH_BUTTON, REG_SHIP_RST, 0, cv.boolean, True),
    (CONF_SYSTEM_REGULATION, REG_SYS_REG, 5, SYSTEM_REGULATION_OPTIONS, "4.5v"),
    (CONF_SYS_MODE, REG_SYS_REG, 2, SYS_MODE_OPTIONS, "normal"),
    (CONF_WATCHDOG_15S_ENABLE, REG_SYS_REG, 1, cv.boolean, False),
    (CONF_DISABLE_VDPPM, REG_SYS_REG, 0, cv.boolean, False),
    (CONF_TS_HOT, REG_TS_CONTROL, 6, TS_HOT_OPTIONS, "60c"),
    (CONF_TS_COLD, REG_TS_CONTROL, 4, TS_COLD_OPTIONS, "0c"),
    (CONF_TS_WARM_DISABLE, REG_TS_CONTROL, 3, cv.boolean, False),
    (CONF_TS_COOL_DISABLE, REG_TS_CONTROL, 2, cv.boolean, False),
    (CONF_TS_ICHG, REG_TS_CONTROL, 1, TS_ICHG_OPTIONS, "0.5x"),
    (CONF_TS_VRCG, REG_TS_CONTROL, 0, TS_VRCG_OPTIONS, "-100mv"),
    (CONF_MASK_TS_INTERRUPT, REG_MASK_ID, 7, cv.boolean, False),
    (CONF_MASK_TREG_INTERRUPT, REG_MASK_ID, 6, cv.boolean, True),
    (CONF_MASK_BAT_INTERRUPT, REG_MASK_ID, 5, cv.boolean, False),
    (CONF_MASK_PG_INTERRUPT, REG_MASK_ID, 4, cv.boolean, False),
)


def _battery_regulation_code(volts: float) -> int:
    """VBAT_CTRL[6:0]: 3.5 V plus 10 mV per step."""
    return (round(volts * 1000) - 3500) // 10


def _fast_charge_code(amps: float) -> int:
    """ICHG_CTRL[6:0]: 1 mA steps from 5 mA to 35 mA, then 10 mA steps from 40 mA."""
    milliamps = round(amps * 1000)
    if milliamps <= 35:
        return milliamps - 5
    return milliamps // 10 + 27


_ENCODERS: dict[str, Callable[[Any], int]] = {
    CONF_BATTERY_REGULATION_VOLTAGE: _battery_regulation_code,
    CONF_FAST_CHARGE_CURRENT: _fast_charge_code,
    CONF_CHARGE_ENABLED: lambda enabled: not enabled,
}


def pack_registers(config: ConfigType) -> list[int]:
    """Pack the validated field values into the VBAT_CTRL..MASK_ID register bytes."""
    regs = [0] * REGISTER_COUNT
    for key, register, shift, validator, _ in FIELDS:
        value = config[key]
        if isinstance(validator, dict):
            value = validator[value]
        elif (encode := _ENCODERS.get(key)) is not None:
            value = encode(value)
        regs[register - REG_VBAT_CTRL] |= int(value) << shift
    return regs


def _field_schema(with_defaults: bool) -> cv.Schema:
    schema = {}
    for key, _, _, validator, default in FIELDS:
        if isinstance(validator, dict):
            validator = cv.enum(validator, lower=True)
        optional = (
            cv.Optional(key, default=default) if with_defaults else cv.Optional(key)
        )
        schema[optional] = validator
    return cv.Schema(schema)


BQ25186_CONFIG_SCHEMA = _field_schema(with_defaults=True)


def _check_watchdog(config: ConfigType) -> ConfigType:
    """Only the status poll talks to the chip, so it has to come round before any watchdog does."""
    periods = []
    if (period := WATCHDOG_PERIOD_S.get(config[CONF_WATCHDOG])) is not None:
        periods.append(period)
    if config[CONF_WATCHDOG_15S_ENABLE]:
        periods.append(WATCHDOG_15S_PERIOD_S)
    if not config.get(CONF_CONFIGURE_ON_BOOT, True):
        periods.append(POWER_ON_WATCHDOG_PERIOD_S)
    if not periods:
        return config
    if config[CONF_UPDATE_INTERVAL] >= TimePeriod(seconds=(limit := min(periods))):
        raise cv.Invalid(
            f"{CONF_UPDATE_INTERVAL} must be shorter than {limit} s, or the "
            f"watchdog resets the charger between status polls",
            path=[CONF_UPDATE_INTERVAL],
        )
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(BQ25186Component),
            cv.Optional(CONF_CONFIGURE_ON_BOOT, default=True): cv.boolean,
        }
    )
    .extend(BQ25186_CONFIG_SCHEMA)
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x6A)),
    _check_watchdog,
)

CONFIGURE_ACTION_SCHEMA = automation.maybe_simple_id(
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(BQ25186Component),
        }
    ).extend(_field_schema(with_defaults=False))
)


async def to_code(config: ConfigType) -> None:
    var = await i2c.new_i2c_component(config)

    if config[CONF_CONFIGURE_ON_BOOT]:
        cg.add(var.set_setup_registers(cg.ArrayInitializer(*pack_registers(config))))


@automation.register_action(
    "bq25186.apply_configuration",
    automation.LambdaAction,
    CONFIGURE_ACTION_SCHEMA,
    synchronous=True,
)
async def bq25186_configure_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
) -> cg.MockObj:
    parent = await cg.get_variable(config[CONF_ID])

    # Options the action leaves out fall back to the component's configuration
    parent_config = fv.get_declaring_config(config[CONF_ID], CORE.config)
    merged = {**parent_config, **config}
    try:
        _check_watchdog(merged)
    except cv.Invalid as err:
        raise EsphomeError(f"bq25186.apply_configuration: {err.msg}") from err
    regs = pack_registers(merged)

    text = str(cg.statement(parent.apply_config(cg.ArrayInitializer(*regs))))
    lambda_ = await cg.process_lambda(Lambda(text), args, return_type=cg.void)
    return automation.new_lambda_pvariable(
        action_id, lambda_, automation.StatelessLambdaAction, template_arg
    )
