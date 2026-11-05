from dataclasses import dataclass

import esphome.codegen as cg
from esphome.components import i2c
from esphome.components.const import (
    CONF_CHARGE_CURRENT,
    CONF_CHARGE_ENABLED,
    CONF_INPUT_CURRENT_LIMIT,
    CONF_PRECHARGE_CURRENT,
)
import esphome.config_validation as cv
from esphome.const import CONF_UPDATE_INTERVAL, CONF_WATCHDOG, SCHEDULER_DONT_RUN
from esphome.core import HexInt, TimePeriod
from esphome.types import ConfigType

CODEOWNERS = ["@beforeyouknowit"]
DEPENDENCIES = ["i2c"]
DOMAIN = "bq25820"
MULTI_CONF = True

bq25820_ns = cg.esphome_ns.namespace("bq25820")
BQ25820Component = bq25820_ns.class_(
    "BQ25820Component", cg.PollingComponent, i2c.I2CDevice
)
BQ25820Config = bq25820_ns.struct("BQ25820Config")

CONF_BQ25820_ID = "bq25820_id"
BQ25820BaseSchema = cv.Schema(
    {
        cv.GenerateID(CONF_BQ25820_ID): cv.use_id(BQ25820Component),
    }
)
BQ25820StatusRegisters = bq25820_ns.class_("BQ25820StatusRegisters")
STATUS_PUBLISHER = cg.HubFunction(
    "status",
    cg.void,
    [(BQ25820StatusRegisters.const_ref(), "status")],
    define="USE_BQ25820_STATUS_PUBLISHER",
)
STATUS = STATUS_PUBLISHER.param("status")
DUMP = cg.HubFunction(
    "dump",
    cg.void,
    [(cg.const_char_ptr, "tag")],
    define="USE_BQ25820_STATUS_PUBLISHER",
)
DUMP_TAG = DUMP.param("tag")
# Offsets into the status registers read from CHARGER_STATUS_1 (0x21)
CHARGER_STATUS_1 = 0
CHARGER_STATUS_2 = 1
CHARGER_STATUS_3 = 2
FAULT_STATUS = 3

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
# WATCHDOG[5:4]; the chip powers up with 40 s and the poll feeds it with WD_RST
WATCHDOG_CHOICES = {"disabled": 0, "40s": 1, "80s": 2, "160s": 3}


@dataclass(frozen=True, kw_only=True)
class _TsThreshold:
    """A TS_THRESHOLD_CONTROL field; its register code is the index of the chosen temperature."""

    key: str
    choices: tuple[int, ...]
    default: str
    shift: int


TS_THRESHOLDS = (
    _TsThreshold(key=CONF_TS_T1, choices=(-10, -5, 0, 5), default="0C", shift=0),
    _TsThreshold(key=CONF_TS_T2, choices=(5, 10, 15, 20), default="10C", shift=2),
    _TsThreshold(key=CONF_TS_T3, choices=(40, 45, 50, 55), default="45C", shift=4),
    _TsThreshold(key=CONF_TS_T5, choices=(50, 55, 60, 65), default="50C", shift=6),
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
# Register fields set from the configuration
TOPOFF_TMR_SHIFT = 6  # TIMER_CONTROL[7:6]
WATCHDOG_SHIFT = 4  # TIMER_CONTROL[5:4]
EN_CHG_TMR = 0x08  # TIMER_CONTROL[3]
CHG_TMR_SHIFT = 1  # TIMER_CONTROL[2:1]
EN_HIZ = 0x04  # CHARGER_CONTROL[2]
EN_CHG = 0x01  # CHARGER_CONTROL[0]
EN_BATFET_IDEAL_DIODE = 0x04  # POWER_PATH_CONTROL[2]
FULL_SWEEP_TMR_SHIFT = 1  # MPPT_CONTROL[2:1]
EN_MPPT = 0x01  # MPPT_CONTROL[0]
EN_TS = 0x01  # TS_REGION_CONTROL[0]
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
        # The unrounded code, so a current just outside the range is not rounded into it
        code = current * milliohms / CURRENT_LIMIT_STEP_MV
        if not low - 1e-9 <= code <= high + 1e-9:
            step = CURRENT_LIMIT_STEP_MV / milliohms
            raise cv.Invalid(
                f"Must be {low * step:g}-{high * step:g} A with a "
                f"{milliohms:g} mΩ {shunt_key}",
                path=[key],
            )
    return config


def _watchdog_boolean(value: object) -> object:
    """True picks the longest period, which the default 60 s update_interval feeds in time."""
    if isinstance(value, bool):
        return "160s" if value else "disabled"
    return value


def _check_watchdog(config: ConfigType) -> ConfigType:
    """Only the status poll feeds the watchdog, so it has to come round before the watchdog expires."""
    # The poll writes the configuration again after a charger reset; without one it stays at reset settings
    if config[CONF_UPDATE_INTERVAL].total_milliseconds == SCHEDULER_DONT_RUN:
        raise cv.Invalid(
            f"{CONF_UPDATE_INTERVAL} cannot be never: the poll restores the charger configuration after a reset",
            path=[CONF_UPDATE_INTERVAL],
        )
    if not (code := WATCHDOG_CHOICES[config[CONF_WATCHDOG]]):
        return config
    period = 20 << code
    if config[CONF_UPDATE_INTERVAL] >= TimePeriod(seconds=period):
        raise cv.Invalid(
            f"{CONF_UPDATE_INTERVAL} must be shorter than the {period} s watchdog, "
            f"or the watchdog resets the charger between polls; shorten "
            f"{CONF_UPDATE_INTERVAL} or choose a longer {CONF_WATCHDOG}",
            path=[CONF_UPDATE_INTERVAL],
        )
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(BQ25820Component),
            # Left out, ICHG_REG keeps its reset value and the ICHG pin sets the current
            cv.Optional(CONF_CHARGE_CURRENT): cv.current,
            cv.Optional(CONF_BAT_SNS_RES, default="5mΩ"): validate_sense_resistance,
            cv.Optional(CONF_AC_SNS_RES, default="2mΩ"): validate_sense_resistance,
            cv.Optional(CONF_WATCHDOG, default="disabled"): cv.All(
                _watchdog_boolean, cv.enum(WATCHDOG_CHOICES, lower=True)
            ),
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
                cv.Optional(ts.key, default=ts.default): cv.All(
                    cv.temperature, cv.one_of(*ts.choices, float=True)
                )
                for ts in TS_THRESHOLDS
            },
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x6B)),
    _validate_current_limits,
    _check_watchdog,
)


def _flag(enabled: bool, bit: int) -> int:
    return bit if enabled else 0


def build_config(config: ConfigType) -> cg.StructInitializer:
    """The register values, in the order of the tables in bq25820.cpp."""
    bat_sns_res = config[CONF_BAT_SNS_RES]
    ac_sns_res = config[CONF_AC_SNS_RES]

    def current_limit(key: str, milliohms: float) -> int:
        # Left out, the register keeps its reset value
        current = config.get(key)
        return 0 if current is None else _current_code(current, milliohms) << 2

    # The 16-bit limits hold their code from bit 2 up; 20 mV voltage steps from 0 V
    limits = (
        current_limit(CONF_CHARGE_CURRENT, bat_sns_res),
        current_limit(CONF_PRECHARGE_CURRENT, bat_sns_res),
        current_limit(CONF_INPUT_CURRENT_LIMIT, ac_sns_res),
        round(config[CONF_INPUT_VOLTAGE_LIMIT] / 0.02) << 2,
        round(config[CONF_REVERSE_SYSTEM_VOLTAGE] / 0.02) << 2,
    )
    ts_thresholds = 0
    for ts in TS_THRESHOLDS:
        ts_thresholds |= ts.choices.index(config[ts.key]) << ts.shift
    # VFB_REG: 2 mV steps from 1.504 V
    charge_voltage = round((config[CONF_CHARGE_VOLTAGE] - 1.504) / 0.002)
    timer_control = (
        TOPOFF_TIMER_CHOICES[config[CONF_TOPOFF_TIMER]] << TOPOFF_TMR_SHIFT
        | WATCHDOG_CHOICES[config[CONF_WATCHDOG]] << WATCHDOG_SHIFT
        | EN_CHG_TMR
        | CHARGE_TIMER_CHOICES[config[CONF_CHARGE_TIMER]] << CHG_TMR_SHIFT
    )
    charger_control = (
        CHARGER_CONTROL_DEFAULTS
        | _flag(config[CONF_HIZ_ENABLED], EN_HIZ)
        | _flag(config[CONF_CHARGE_ENABLED], EN_CHG)
    )
    power_path_control = _flag(config[CONF_IDEAL_DIODE], EN_BATFET_IDEAL_DIODE)
    mppt_control = MPPT_SWEEP_TIMER_CHOICES[
        config[CONF_MPPT_SWEEP_TIMER]
    ] << FULL_SWEEP_TMR_SHIFT | _flag(config[CONF_MPPT_ENABLED], EN_MPPT)
    ts_region_control = _flag(config[CONF_TEMP_EN], EN_TS)
    controls = (
        charge_voltage,
        timer_control,
        power_path_control,
        mppt_control,
        ts_thresholds,
        ts_region_control,
        charger_control,
    )
    return cg.StructInitializer(
        BQ25820Config,
        ("limits", cg.ArrayInitializer(*limits)),
        ("controls", cg.ArrayInitializer(*(HexInt(v) for v in controls))),
        ("ibat_scale", IBAT_ADC_STEP_MV / bat_sns_res),
        ("iac_scale", IAC_ADC_STEP_MV / ac_sns_res),
    )


async def to_code(config: ConfigType) -> None:
    # In flash: the chip only needs it again after a charger reset
    table = cg.shared_progmem_array(
        "bq25820_config", BQ25820Config, cg.ArrayInitializer(build_config(config))
    )
    await i2c.new_i2c_component(config, table, config[CONF_FORCE_MPPT_SWEEP])
