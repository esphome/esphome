import esphome.codegen as cg
from esphome.components import i2c
from esphome.components.const import (
    CONF_CHARGE_CURRENT,
    CONF_CHARGE_ENABLED,
    CONF_INPUT_CURRENT_LIMIT,
    CONF_PRECHARGE_CURRENT,
)
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_WATCHDOG
from esphome.types import ConfigType

CODEOWNERS = ["@beforeyouknowit"]
DEPENDENCIES = ["i2c"]
DOMAIN = "bq25820"
MULTI_CONF = True

bq25820_ns = cg.esphome_ns.namespace("bq25820")
BQ25820Component = bq25820_ns.class_(
    "BQ25820Component", cg.PollingComponent, i2c.I2CDevice
)

CONF_BQ25820_ID = "bq25820_id"
BQ25820BaseSchema = cv.Schema(
    {
        cv.GenerateID(CONF_BQ25820_ID): cv.use_id(BQ25820Component),
    }
)

CONF_BAT_SNS_RES = "bat_sns_res"
CONF_AC_SNS_RES = "ac_sns_res"
CONF_CHARGE_VOLTAGE = "charge_voltage"
CONF_INPUT_VOLTAGE_LIMIT = "input_voltage_limit"
CONF_REVERSE_SYSTEM_VOLTAGE = "reverse_system_voltage"
CONF_HIZ_ENABLED = "hiz_enabled"
CONF_CHARGE_TIMER = "charge_timer"
CONF_TOPOFF_TIMER = "topoff_timer"
CONF_IDEAL_DIODE = "ideal_diode"
CONF_MPPT_ENABLED = "mppt_enabled"
CONF_MPPT_SWEEP_TIMER = "mppt_sweep_timer"
CONF_FORCE_MPPT_SWEEP = "force_mppt_sweep"
CONF_TEMP_EN = "temp_en"
CONF_TS_T1 = "ts_t1"
CONF_TS_T2 = "ts_t2"
CONF_TS_T3 = "ts_t3"
CONF_TS_T5 = "ts_t5"

CHARGE_TIMER_CHOICES = {"5h": 0, "8h": 1, "12h": 2, "24h": 3}
TOPOFF_TIMER_CHOICES = {"disabled": 0, "15m": 1, "30m": 2, "45m": 3}
MPPT_SWEEP_TIMER_CHOICES = {"3m": 0, "10m": 1, "15m": 2, "20m": 3}
# (key, choices, default, shift): the register code is the index of the choice
TS_THRESHOLDS = (
    (CONF_TS_T1, (-10, -5, 0, 5), "0C", 0),
    (CONF_TS_T2, (5, 10, 15, 20), "10C", 2),
    (CONF_TS_T3, (40, 45, 50, 55), "45C", 4),
    (CONF_TS_T5, (50, 55, 60, 65), "50C", 6),
)

# The current limits step 250 uV across the sense resistor: 50 mA with the
# 5 mOhm battery reference shunt, 125 mA with the 2 mOhm input one
CURRENT_LIMIT_STEP_MV = 0.25
# The current ADCs step 10 uV (IBAT) and 4 uV (IAC) across the sense resistor
IBAT_ADC_STEP_MV = 0.010
IAC_ADC_STEP_MV = 0.004
# Charger control bits the component does not set, at their reset values:
# VRECHG 97.6% and EN_CHG_BIT_RESET_BEHAVIOR
CHARGER_CONTROL_DEFAULTS = 0xC8
# (key, sense resistor key, lowest code, highest code)
CURRENT_LIMITS = (
    (CONF_CHARGE_CURRENT, CONF_BAT_SNS_RES, 8, 400),
    (CONF_PRECHARGE_CURRENT, CONF_BAT_SNS_RES, 5, 200),
    (CONF_INPUT_CURRENT_LIMIT, CONF_AC_SNS_RES, 8, 400),
)


def validate_sense_resistance(value: object) -> float:
    """Shunt resistance in milliohms, e.g. '5mΩ'."""
    milliohms = cv.resistance(value) * 1000
    if not 0.01 <= milliohms <= 10:
        raise cv.Invalid(f"Sense resistance must be 0.01-10 mΩ, got {milliohms} mΩ")
    return milliohms


def _current_code(current: float, milliohms: float) -> int:
    return round(current * milliohms / CURRENT_LIMIT_STEP_MV)


def _validate_current_limits(config: ConfigType) -> ConfigType:
    """The charger regulates the voltage across the shunt, so the range depends on it."""
    for key, shunt_key, low, high in CURRENT_LIMITS:
        if (current := config.get(key)) is None:
            continue
        milliohms = config[shunt_key]
        if not low <= _current_code(current, milliohms) <= high:
            step = CURRENT_LIMIT_STEP_MV / milliohms
            raise cv.Invalid(
                f"Must be {low * step:g}-{high * step:g} A with a "
                f"{milliohms:g} mΩ {shunt_key}",
                path=[key],
            )
    return config


def _ts_threshold(choices: tuple[int, ...]) -> cv.All:
    return cv.All(cv.temperature, cv.one_of(*choices, float=True))


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(BQ25820Component),
            # Left out, ICHG_REG keeps its reset value and the ICHG pin sets the current
            cv.Optional(CONF_CHARGE_CURRENT): cv.current,
            cv.Optional(CONF_BAT_SNS_RES, default="5mΩ"): validate_sense_resistance,
            cv.Optional(CONF_AC_SNS_RES, default="2mΩ"): validate_sense_resistance,
            cv.Optional(CONF_WATCHDOG, default=False): cv.boolean,
            # VFB feedback setpoint. The pack charge voltage is set by the
            # external VFB divider; this trims the 1.536 V regulation limit.
            cv.Optional(CONF_CHARGE_VOLTAGE, default="1.536V"): cv.All(
                cv.voltage, cv.float_range(min=1.504, max=1.566)
            ),
            cv.Optional(CONF_INPUT_CURRENT_LIMIT): cv.current,
            cv.Optional(CONF_INPUT_VOLTAGE_LIMIT, default="4.4V"): cv.All(
                cv.voltage, cv.float_range(min=4.4, max=65)
            ),
            cv.Optional(CONF_PRECHARGE_CURRENT): cv.current,
            cv.Optional(CONF_REVERSE_SYSTEM_VOLTAGE, default="5V"): cv.All(
                cv.voltage, cv.float_range(min=3.3, max=65)
            ),
            cv.Optional(CONF_CHARGE_ENABLED, default=True): cv.boolean,
            cv.Optional(CONF_HIZ_ENABLED, default=False): cv.boolean,
            cv.Optional(CONF_CHARGE_TIMER, default="8h"): cv.enum(
                CHARGE_TIMER_CHOICES, lower=True
            ),
            cv.Optional(CONF_TOPOFF_TIMER, default="disabled"): cv.enum(
                TOPOFF_TIMER_CHOICES, lower=True
            ),
            cv.Optional(CONF_IDEAL_DIODE, default=False): cv.boolean,
            cv.Optional(CONF_MPPT_ENABLED, default=False): cv.boolean,
            cv.Optional(CONF_MPPT_SWEEP_TIMER, default="3m"): cv.enum(
                MPPT_SWEEP_TIMER_CHOICES, lower=True
            ),
            cv.Optional(CONF_FORCE_MPPT_SWEEP, default=False): cv.boolean,
            cv.Optional(CONF_TEMP_EN, default=True): cv.boolean,
            **{
                cv.Optional(key, default=default): _ts_threshold(choices)
                for key, choices, default, _ in TS_THRESHOLDS
            },
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x6B)),
    _validate_current_limits,
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    bat_sns_res = config[CONF_BAT_SNS_RES]
    ac_sns_res = config[CONF_AC_SNS_RES]
    # Amps per ADC step
    cg.add(var.set_ibat_scale(IBAT_ADC_STEP_MV / bat_sns_res))
    cg.add(var.set_iac_scale(IAC_ADC_STEP_MV / ac_sns_res))
    if (current := config.get(CONF_CHARGE_CURRENT)) is not None:
        cg.add(var.set_charge_current_code(_current_code(current, bat_sns_res)))
    if (current := config.get(CONF_PRECHARGE_CURRENT)) is not None:
        cg.add(var.set_precharge_current_code(_current_code(current, bat_sns_res)))
    if (current := config.get(CONF_INPUT_CURRENT_LIMIT)) is not None:
        cg.add(var.set_input_current_limit_code(_current_code(current, ac_sns_res)))
    # 2 mV steps from 1.504 V; 20 mV steps from 0 V
    cg.add(
        var.set_charge_voltage_code(
            round((config[CONF_CHARGE_VOLTAGE] - 1.504) / 0.002)
        )
    )
    cg.add(
        var.set_input_voltage_limit_code(round(config[CONF_INPUT_VOLTAGE_LIMIT] / 0.02))
    )
    cg.add(
        var.set_reverse_system_voltage_code(
            round(config[CONF_REVERSE_SYSTEM_VOLTAGE] / 0.02)
        )
    )
    # TOPOFF_TMR[7:6], WATCHDOG[5:4] (01b = 40 s), EN_CHG_TMR, CHG_TMR[2:1]
    cg.add(
        var.set_timer_control(
            TOPOFF_TIMER_CHOICES[config[CONF_TOPOFF_TIMER]] << 6
            | (0x10 if config[CONF_WATCHDOG] else 0)
            | 0x08
            | CHARGE_TIMER_CHOICES[config[CONF_CHARGE_TIMER]] << 1
        )
    )
    # EN_HIZ, EN_CHG
    cg.add(
        var.set_charger_control(
            CHARGER_CONTROL_DEFAULTS
            | (0x04 if config[CONF_HIZ_ENABLED] else 0)
            | (0x01 if config[CONF_CHARGE_ENABLED] else 0)
        )
    )
    # FULL_SWEEP_TMR[2:1], EN_MPPT
    cg.add(
        var.set_mppt_control(
            MPPT_SWEEP_TIMER_CHOICES[config[CONF_MPPT_SWEEP_TIMER]] << 1
            | (0x01 if config[CONF_MPPT_ENABLED] else 0)
        )
    )
    ts_thresholds = 0
    for key, choices, _, shift in TS_THRESHOLDS:
        ts_thresholds |= choices.index(config[key]) << shift
    cg.add(var.set_ts_thresholds(ts_thresholds))
    cg.add(var.set_ideal_diode_enabled(config[CONF_IDEAL_DIODE]))
    cg.add(var.set_force_mppt_sweep(config[CONF_FORCE_MPPT_SWEEP]))
    cg.add(var.set_temp_enabled(config[CONF_TEMP_EN]))
