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
        cv.Required(CONF_BQ25820_ID): cv.use_id(BQ25820Component),
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


def validate_sense_resistance(value):
    """Shunt resistance in milliohms, e.g. '5mΩ'.

    The current ADCs step 10 mV (IBAT) and 4 mV (IAC) per LSB, which
    round to zero above 10 mΩ.
    """
    milliohms = cv.resistance(value) * 1000
    if not 0.01 <= milliohms <= 10:
        raise cv.Invalid(f"Sense resistance must be 0.01-10 mΩ, got {milliohms} mΩ")
    return milliohms


def _choice_to_code(mapping):
    """Map a string choice such as '12h' to its register code."""

    def validator(value):
        if isinstance(value, str) and value.lower() in mapping:
            return mapping[value.lower()]
        raise cv.Invalid(f"Expected one of: {', '.join(mapping)}")

    return validator


def _temp_to_code(temps):
    """Map a temperature such as '5C' to its 2-bit register code."""

    def validator(value):
        temp = cv.temperature(value)
        for code, choice in enumerate(temps):
            if temp == choice:
                return code
        raise cv.Invalid(f"Expected one of: {', '.join(f'{t}C' for t in temps)}")

    return validator


CHARGE_TIMER_CHOICES = {"5h": 0, "8h": 1, "12h": 2, "24h": 3}
TOPOFF_TIMER_CHOICES = {"disabled": 0, "15m": 1, "30m": 2, "45m": 3}
MPPT_SWEEP_TIMER_CHOICES = {"3m": 0, "10m": 1, "15m": 2, "20m": 3}
TS_T1_CHOICES = [-10, -5, 0, 5]
TS_T2_CHOICES = [5, 10, 15, 20]
TS_T3_CHOICES = [40, 45, 50, 55]
TS_T5_CHOICES = [50, 55, 60, 65]


BQ25820_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(BQ25820Component),
            # 0 leaves ICHG_REG at its power-on-reset value (ICHG pin / 5 A)
            cv.Optional(CONF_CHARGE_CURRENT, default=0): cv.All(
                cv.current, cv.float_range(min=0, max=20)
            ),
            cv.Optional(CONF_BAT_SNS_RES, default="5mΩ"): validate_sense_resistance,
            cv.Optional(CONF_AC_SNS_RES, default="2mΩ"): validate_sense_resistance,
            cv.Optional(CONF_WATCHDOG, default=False): cv.boolean,
            # VFB feedback setpoint. The pack charge voltage is set by the
            # external VFB divider; this trims the 1.536 V regulation limit.
            cv.Optional(CONF_CHARGE_VOLTAGE, default="1.536V"): cv.All(
                cv.voltage, cv.float_range(min=1.504, max=1.566)
            ),
            cv.Optional(CONF_INPUT_CURRENT_LIMIT, default="50A"): cv.All(
                cv.current, cv.float_range(min=1, max=50)
            ),
            cv.Optional(CONF_INPUT_VOLTAGE_LIMIT, default="4.4V"): cv.All(
                cv.voltage, cv.float_range(min=4.4, max=65)
            ),
            cv.Optional(CONF_PRECHARGE_CURRENT, default="4A"): cv.All(
                cv.current, cv.float_range(min=0.25, max=10)
            ),
            cv.Optional(CONF_REVERSE_SYSTEM_VOLTAGE, default="5V"): cv.All(
                cv.voltage, cv.float_range(min=3.3, max=65)
            ),
            cv.Optional(CONF_CHARGE_ENABLED, default=True): cv.boolean,
            cv.Optional(CONF_HIZ_ENABLED, default=False): cv.boolean,
            cv.Optional(CONF_CHARGE_TIMER, default="8h"): _choice_to_code(
                CHARGE_TIMER_CHOICES
            ),
            cv.Optional(CONF_TOPOFF_TIMER, default="disabled"): _choice_to_code(
                TOPOFF_TIMER_CHOICES
            ),
            cv.Optional(CONF_IDEAL_DIODE, default=False): cv.boolean,
            cv.Optional(CONF_MPPT_ENABLED, default=False): cv.boolean,
            cv.Optional(CONF_MPPT_SWEEP_TIMER, default="3m"): _choice_to_code(
                MPPT_SWEEP_TIMER_CHOICES
            ),
            cv.Optional(CONF_FORCE_MPPT_SWEEP, default=False): cv.boolean,
            cv.Optional(CONF_TEMP_EN, default=True): cv.boolean,
            cv.Optional(CONF_TS_T1, default="0C"): _temp_to_code(TS_T1_CHOICES),
            cv.Optional(CONF_TS_T2, default="10C"): _temp_to_code(TS_T2_CHOICES),
            cv.Optional(CONF_TS_T3, default="45C"): _temp_to_code(TS_T3_CHOICES),
            cv.Optional(CONF_TS_T5, default="50C"): _temp_to_code(TS_T5_CHOICES),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(cv.polling_component_schema("500ms"))
    .extend(i2c.i2c_device_schema(0x6B))
)

CONFIG_SCHEMA = BQ25820_SCHEMA


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
    cg.add(var.set_charge_current_ma(round(config[CONF_CHARGE_CURRENT] * 1000)))
    cg.add(var.set_bat_sns_res_mohm(config[CONF_BAT_SNS_RES]))
    cg.add(var.set_ac_sns_res_mohm(config[CONF_AC_SNS_RES]))
    cg.add(var.set_watchdog_enabled(config[CONF_WATCHDOG]))
    cg.add(var.set_charge_voltage_mv(round(config[CONF_CHARGE_VOLTAGE] * 1000)))
    cg.add(
        var.set_input_current_limit_ma(round(config[CONF_INPUT_CURRENT_LIMIT] * 1000))
    )
    cg.add(
        var.set_input_voltage_limit_mv(round(config[CONF_INPUT_VOLTAGE_LIMIT] * 1000))
    )
    cg.add(var.set_precharge_current_ma(round(config[CONF_PRECHARGE_CURRENT] * 1000)))
    cg.add(
        var.set_reverse_system_voltage_mv(
            round(config[CONF_REVERSE_SYSTEM_VOLTAGE] * 1000)
        )
    )
    cg.add(var.set_charge_enabled(config[CONF_CHARGE_ENABLED]))
    cg.add(var.set_hiz_enabled(config[CONF_HIZ_ENABLED]))
    cg.add(var.set_charge_timer_code(config[CONF_CHARGE_TIMER]))
    cg.add(var.set_topoff_timer_code(config[CONF_TOPOFF_TIMER]))
    cg.add(var.set_ideal_diode_enabled(config[CONF_IDEAL_DIODE]))
    cg.add(var.set_mppt_enabled(config[CONF_MPPT_ENABLED]))
    cg.add(var.set_mppt_sweep_timer_code(config[CONF_MPPT_SWEEP_TIMER]))
    cg.add(var.set_force_mppt_sweep(config[CONF_FORCE_MPPT_SWEEP]))
    cg.add(var.set_temp_enabled(config[CONF_TEMP_EN]))
    cg.add(
        var.set_ts_thresholds(
            config[CONF_TS_T1],
            config[CONF_TS_T2],
            config[CONF_TS_T3],
            config[CONF_TS_T5],
        )
    )
