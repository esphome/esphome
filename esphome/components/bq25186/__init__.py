import esphome.codegen as cg
from esphome.components import i2c
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_WATCHDOG

CODEOWNERS = ["@Rapsssito"]
DEPENDENCIES = ["i2c"]
MULTI_CONF = True

CONF_BQ25186_ID = "bq25186_id"
CONF_BATTERY_REGULATION_VOLTAGE = "battery_regulation_voltage"
CONF_FAST_CHARGE_CURRENT = "fast_charge_current"
CONF_CHARGE_ENABLED = "charge_enabled"
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
CONF_INPUT_CURRENT_LIMIT = "input_current_limit"
CONF_PUSH_BUTTON_LONG_PRESS_ACTION = "push_button_long_press_action"
CONF_WAKE1_TIMER = "wake1_timer"
CONF_WAKE2_TIMER = "wake2_timer"
CONF_ENABLE_PUSH_BUTTON = "enable_push_button"
CONF_SYSTEM_REGULATION = "system_regulation"
CONF_PG_GPO_LEVEL = "pg_gpo_level"
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
CONF_PG_MODE = "pg_mode"

bq25186_ns = cg.esphome_ns.namespace("bq25186")
BQ25186Component = bq25186_ns.class_(
    "BQ25186Component", cg.PollingComponent, i2c.I2CDevice
)
BQ25186Listener = bq25186_ns.class_("BQ25186Listener")

TERMINATION_PERCENT_OPTIONS = {
    "disabled": 0,
    "5%": 1,
    "10%": 2,
    "20%": 3,
}

VINDPM_MODE_OPTIONS = {
    "battery_tracking": 0,
    "4.5v": 1,
    "4.7v": 2,
    "disabled": 3,
}

THERM_REGULATION_OPTIONS = {
    "100c": 0,
    "80c": 1,
    "60c": 2,
    "disabled": 3,
}

BATTERY_OCP_LIMIT_OPTIONS = {
    "500ma": 0,
    "1000ma": 1,
    "1500ma": 2,
    "3000ma": 3,
}

BATTERY_UVLO_OPTIONS = {
    "3.0v": 0,
    "2.8v": 3,
    "2.6v": 4,
    "2.4v": 5,
    "2.2v": 6,
    "2.0v": 7,
}

SAFETY_TIMER_OPTIONS = {
    "3h": 0,
    "6h": 1,
    "12h": 2,
    "disabled": 3,
}

WATCHDOG_OPTIONS = {
    "160s_software_reset": 0,
    "160s_hardware_reset": 1,
    "40s_hardware_reset": 2,
    "disabled": 3,
}

LONG_PRESS_DURATION_OPTIONS = {
    "5s": 0,
    "10s": 1,
    "15s": 2,
    "20s": 3,
}

AUTOWAKE_OPTIONS = {
    "0.5s": 0,
    "1s": 1,
    "2s": 2,
    "4s": 3,
}

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

TS_HOT_OPTIONS = {
    "60c": 0,
    "65c": 1,
    "50c": 2,
    "45c": 3,
}

TS_COLD_OPTIONS = {
    "0c": 0,
    "3c": 1,
    "5c": 2,
    "-3c": 3,
}

TS_ICHG_OPTIONS = {
    "0.5x": 0,
    "0.2x": 1,
}

TS_VRCG_OPTIONS = {
    "-100mv": 0,
    "-200mv": 1,
}

PG_MODE_OPTIONS = {
    "power_good": 0,
    "gpo": 1,
}

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(BQ25186Component),
            cv.Optional(CONF_PG_MODE, default="power_good"): cv.enum(
                PG_MODE_OPTIONS, lower=True
            ),
            cv.Optional(CONF_BATTERY_REGULATION_VOLTAGE, default=4200): cv.int_range(
                min=3500, max=4650
            ),
            cv.Optional(CONF_CHARGE_ENABLED, default=True): cv.boolean,
            cv.Optional(CONF_FAST_CHARGE_CURRENT, default=10): cv.int_range(
                min=5, max=1000
            ),
            cv.Optional(CONF_FLASH_CHARGING_MODE, default=False): cv.boolean,
            cv.Optional(CONF_PRECHARGE_IS_ITERM, default=False): cv.boolean,
            cv.Optional(CONF_TERMINATION_PERCENT, default="10%"): cv.enum(
                TERMINATION_PERCENT_OPTIONS, lower=True
            ),
            cv.Optional(CONF_VINDPM_MODE, default="4.5v"): cv.enum(
                VINDPM_MODE_OPTIONS, lower=True
            ),
            cv.Optional(CONF_THERMAL_REGULATION, default="100c"): cv.enum(
                THERM_REGULATION_OPTIONS, lower=True
            ),
            cv.Optional(CONF_BATTERY_OCP_LIMIT, default="3000ma"): cv.enum(
                BATTERY_OCP_LIMIT_OPTIONS, lower=True
            ),
            cv.Optional(CONF_BATTERY_UVLO, default="3.0v"): cv.enum(
                BATTERY_UVLO_OPTIONS, lower=True
            ),
            cv.Optional(CONF_MASK_CHARGE_STATUS_INTERRUPT, default=True): cv.boolean,
            cv.Optional(CONF_MASK_ILIM_INTERRUPT, default=True): cv.boolean,
            cv.Optional(CONF_MASK_VINDPM_INTERRUPT, default=False): cv.boolean,
            cv.Optional(CONF_TS_AUTO_FUNCTION, default=True): cv.boolean,
            cv.Optional(CONF_VLOWV, default="3.0v"): cv.one_of(
                "3.0v", "2.8v", lower=True
            ),
            cv.Optional(CONF_RECHARGE_THRESHOLD, default="100mv"): cv.one_of(
                "100mv", "200mv", lower=True
            ),
            cv.Optional(CONF_DOUBLE_TIMER_DURING_DPM, default=False): cv.boolean,
            cv.Optional(CONF_SAFETY_TIMER, default="6h"): cv.enum(
                SAFETY_TIMER_OPTIONS, lower=True
            ),
            cv.Optional(CONF_WATCHDOG, default="160s_software_reset"): cv.enum(
                WATCHDOG_OPTIONS, lower=True
            ),
            cv.Optional(CONF_LONG_PRESS_DURATION, default="10s"): cv.enum(
                LONG_PRESS_DURATION_OPTIONS, lower=True
            ),
            cv.Optional(CONF_HW_RESET_REQUIRES_VIN, default=False): cv.boolean,
            cv.Optional(CONF_AUTOWAKE, default="1s"): cv.enum(
                AUTOWAKE_OPTIONS, lower=True
            ),
            cv.Optional(CONF_INPUT_CURRENT_LIMIT, default="500ma"): cv.enum(
                INPUT_CURRENT_LIMIT_OPTIONS, lower=True
            ),
            cv.Optional(
                CONF_PUSH_BUTTON_LONG_PRESS_ACTION, default="ship_mode"
            ): cv.enum(PUSH_BUTTON_LONG_PRESS_ACTION_OPTIONS, lower=True),
            cv.Optional(CONF_WAKE1_TIMER, default="300ms"): cv.one_of(
                "300ms", "1s", lower=True
            ),
            cv.Optional(CONF_WAKE2_TIMER, default="2s"): cv.one_of(
                "2s", "3s", lower=True
            ),
            cv.Optional(CONF_ENABLE_PUSH_BUTTON, default=True): cv.boolean,
            cv.Optional(CONF_SYSTEM_REGULATION, default="4.5v"): cv.enum(
                SYSTEM_REGULATION_OPTIONS, lower=True
            ),
            cv.Optional(CONF_SYS_MODE, default="normal"): cv.enum(
                SYS_MODE_OPTIONS, lower=True
            ),
            cv.Optional(CONF_WATCHDOG_15S_ENABLE, default=False): cv.boolean,
            cv.Optional(CONF_DISABLE_VDPPM, default=False): cv.boolean,
            cv.Optional(CONF_TS_HOT, default="60c"): cv.enum(
                TS_HOT_OPTIONS, lower=True
            ),
            cv.Optional(CONF_TS_COLD, default="0c"): cv.enum(
                TS_COLD_OPTIONS, lower=True
            ),
            cv.Optional(CONF_TS_WARM_DISABLE, default=False): cv.boolean,
            cv.Optional(CONF_TS_COOL_DISABLE, default=False): cv.boolean,
            cv.Optional(CONF_TS_ICHG, default="0.5x"): cv.enum(
                TS_ICHG_OPTIONS, lower=True
            ),
            cv.Optional(CONF_TS_VRCG, default="-100mv"): cv.enum(
                TS_VRCG_OPTIONS, lower=True
            ),
            cv.Optional(CONF_MASK_TS_INTERRUPT, default=False): cv.boolean,
            cv.Optional(CONF_MASK_TREG_INTERRUPT, default=True): cv.boolean,
            cv.Optional(CONF_MASK_BAT_INTERRUPT, default=False): cv.boolean,
            cv.Optional(CONF_MASK_PG_INTERRUPT, default=False): cv.boolean,
        }
    )
    .extend(cv.polling_component_schema("5s"))
    .extend(i2c.i2c_device_schema(0x6A))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    cg.add(var.set_battery_regulation_voltage(config[CONF_BATTERY_REGULATION_VOLTAGE]))
    cg.add(var.set_fast_charge_current(config[CONF_FAST_CHARGE_CURRENT]))
    cg.add(var.set_charge_enabled(config[CONF_CHARGE_ENABLED]))
    cg.add(var.set_flash_charging_mode(config[CONF_FLASH_CHARGING_MODE]))
    cg.add(var.set_precharge_is_iterm(config[CONF_PRECHARGE_IS_ITERM]))
    cg.add(var.set_termination_percent(config[CONF_TERMINATION_PERCENT]))
    cg.add(var.set_vindpm_mode(config[CONF_VINDPM_MODE]))
    cg.add(var.set_thermal_regulation(config[CONF_THERMAL_REGULATION]))
    cg.add(var.set_battery_ocp_limit(config[CONF_BATTERY_OCP_LIMIT]))
    cg.add(var.set_battery_uvlo(config[CONF_BATTERY_UVLO]))
    cg.add(
        var.set_interrupt_masks(
            config[CONF_MASK_CHARGE_STATUS_INTERRUPT],
            config[CONF_MASK_ILIM_INTERRUPT],
            config[CONF_MASK_VINDPM_INTERRUPT],
        )
    )

    cg.add(var.set_ts_auto_function(config[CONF_TS_AUTO_FUNCTION]))
    cg.add(var.set_vlowv_select(1 if config[CONF_VLOWV] == "2.8v" else 0))
    cg.add(
        var.set_recharge_threshold(
            1 if config[CONF_RECHARGE_THRESHOLD] == "200mv" else 0
        )
    )
    cg.add(var.set_double_timer_during_dpm(config[CONF_DOUBLE_TIMER_DURING_DPM]))
    cg.add(var.set_safety_timer(config[CONF_SAFETY_TIMER]))
    cg.add(var.set_watchdog(config[CONF_WATCHDOG]))

    cg.add(var.set_long_press_duration(config[CONF_LONG_PRESS_DURATION]))
    cg.add(var.set_hw_reset_requires_vin(config[CONF_HW_RESET_REQUIRES_VIN]))
    cg.add(var.set_autowake(config[CONF_AUTOWAKE]))
    cg.add(var.set_input_current_limit(config[CONF_INPUT_CURRENT_LIMIT]))

    cg.add(
        var.set_push_button_settings(
            config[CONF_PUSH_BUTTON_LONG_PRESS_ACTION],
            1 if config[CONF_WAKE1_TIMER] == "1s" else 0,
            1 if config[CONF_WAKE2_TIMER] == "3s" else 0,
            config[CONF_ENABLE_PUSH_BUTTON],
        )
    )

    cg.add(var.set_system_regulation(config[CONF_SYSTEM_REGULATION]))
    cg.add(var.set_sys_mode(config[CONF_SYS_MODE]))
    cg.add(var.set_watchdog_15s_enable(config[CONF_WATCHDOG_15S_ENABLE]))
    cg.add(var.set_disable_vdppm(config[CONF_DISABLE_VDPPM]))

    cg.add(var.set_ts_hot(config[CONF_TS_HOT]))
    cg.add(var.set_ts_cold(config[CONF_TS_COLD]))
    cg.add(var.set_ts_warm_disable(config[CONF_TS_WARM_DISABLE]))
    cg.add(var.set_ts_cool_disable(config[CONF_TS_COOL_DISABLE]))
    cg.add(var.set_ts_ichg(config[CONF_TS_ICHG]))
    cg.add(var.set_ts_vrcg(config[CONF_TS_VRCG]))

    cg.add(
        var.set_global_interrupt_masks(
            config[CONF_MASK_TS_INTERRUPT],
            config[CONF_MASK_TREG_INTERRUPT],
            config[CONF_MASK_BAT_INTERRUPT],
            config[CONF_MASK_PG_INTERRUPT],
        )
    )
    cg.add(var.set_pg_mode(config[CONF_PG_MODE]))
