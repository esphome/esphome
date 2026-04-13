from esphome import automation
import esphome.codegen as cg
from esphome.components import i2c
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_WATCHDOG

CODEOWNERS = ["@Rapsssito"]
DEPENDENCIES = ["i2c"]
MULTI_CONF = True

CONF_BQ25186_ID = "bq25186_id"
CONF_CONFIGURE_ON_BOOT = "configure_on_boot"
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
BQ25186VBatCtrlConfig = bq25186_ns.struct("BQ25186VBatCtrlConfig")
BQ25186IchgCtrlConfig = bq25186_ns.struct("BQ25186IchgCtrlConfig")
BQ25186ChargeCtrl0Config = bq25186_ns.struct("BQ25186ChargeCtrl0Config")
BQ25186ChargeCtrl1Config = bq25186_ns.struct("BQ25186ChargeCtrl1Config")
BQ25186IcCtrlConfig = bq25186_ns.struct("BQ25186IcCtrlConfig")
BQ25186TmrIlimConfig = bq25186_ns.struct("BQ25186TmrIlimConfig")
BQ25186ShipRstConfig = bq25186_ns.struct("BQ25186ShipRstConfig")
BQ25186SysRegConfig = bq25186_ns.struct("BQ25186SysRegConfig")
BQ25186TsControlConfig = bq25186_ns.struct("BQ25186TsControlConfig")
BQ25186MaskIdConfig = bq25186_ns.struct("BQ25186MaskIdConfig")
BQ25186Config = bq25186_ns.struct("BQ25186Config")
BQ25186ConfigureAction = bq25186_ns.class_("BQ25186ConfigureAction", automation.Action)

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
            cv.Optional(CONF_CONFIGURE_ON_BOOT, default=True): cv.boolean,
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
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x6A))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    vbat_ctrl = cg.StructInitializer(
        BQ25186VBatCtrlConfig,
        ("battery_regulation_voltage_mv", config[CONF_BATTERY_REGULATION_VOLTAGE]),
        ("pg_mode", config[CONF_PG_MODE]),
    )

    ichg_ctrl = cg.StructInitializer(
        BQ25186IchgCtrlConfig,
        ("charge_current_ma", config[CONF_FAST_CHARGE_CURRENT]),
        ("charge_enabled", config[CONF_CHARGE_ENABLED]),
    )

    chargectrl0 = cg.StructInitializer(
        BQ25186ChargeCtrl0Config,
        ("flash_charging_mode", config[CONF_FLASH_CHARGING_MODE]),
        ("precharge_is_iterm", config[CONF_PRECHARGE_IS_ITERM]),
        ("termination_percent", config[CONF_TERMINATION_PERCENT]),
        ("vindpm_mode", config[CONF_VINDPM_MODE]),
        ("thermal_regulation", config[CONF_THERMAL_REGULATION]),
    )

    chargectrl1 = cg.StructInitializer(
        BQ25186ChargeCtrl1Config,
        ("battery_ocp_limit", config[CONF_BATTERY_OCP_LIMIT]),
        ("battery_uvlo", config[CONF_BATTERY_UVLO]),
        ("mask_charge_status_interrupt", config[CONF_MASK_CHARGE_STATUS_INTERRUPT]),
        ("mask_ilim_interrupt", config[CONF_MASK_ILIM_INTERRUPT]),
        ("mask_vindpm_interrupt", config[CONF_MASK_VINDPM_INTERRUPT]),
    )

    ic_ctrl = cg.StructInitializer(
        BQ25186IcCtrlConfig,
        ("ts_auto_function", config[CONF_TS_AUTO_FUNCTION]),
        ("vlowv_select", 1 if config[CONF_VLOWV] == "2.8v" else 0),
        ("recharge_threshold", 1 if config[CONF_RECHARGE_THRESHOLD] == "200mv" else 0),
        ("double_timer_during_dpm", config[CONF_DOUBLE_TIMER_DURING_DPM]),
        ("safety_timer", config[CONF_SAFETY_TIMER]),
        ("watchdog", config[CONF_WATCHDOG]),
    )

    tmr_ilim = cg.StructInitializer(
        BQ25186TmrIlimConfig,
        ("long_press_duration", config[CONF_LONG_PRESS_DURATION]),
        ("hw_reset_requires_vin", config[CONF_HW_RESET_REQUIRES_VIN]),
        ("autowake", config[CONF_AUTOWAKE]),
        ("input_current_limit", config[CONF_INPUT_CURRENT_LIMIT]),
    )

    ship_rst = cg.StructInitializer(
        BQ25186ShipRstConfig,
        ("push_button_long_press_action", config[CONF_PUSH_BUTTON_LONG_PRESS_ACTION]),
        ("wake1_timer", 1 if config[CONF_WAKE1_TIMER] == "1s" else 0),
        ("wake2_timer", 1 if config[CONF_WAKE2_TIMER] == "3s" else 0),
        ("enable_push_button", config[CONF_ENABLE_PUSH_BUTTON]),
    )

    sys_reg = cg.StructInitializer(
        BQ25186SysRegConfig,
        ("system_regulation", config[CONF_SYSTEM_REGULATION]),
        ("pg_gpo_level", False),
        ("sys_mode", config[CONF_SYS_MODE]),
        ("watchdog_15s_enable", config[CONF_WATCHDOG_15S_ENABLE]),
        ("disable_vdppm", config[CONF_DISABLE_VDPPM]),
    )

    ts_control = cg.StructInitializer(
        BQ25186TsControlConfig,
        ("ts_hot", config[CONF_TS_HOT]),
        ("ts_cold", config[CONF_TS_COLD]),
        ("ts_warm_disable", config[CONF_TS_WARM_DISABLE]),
        ("ts_cool_disable", config[CONF_TS_COOL_DISABLE]),
        ("ts_ichg", config[CONF_TS_ICHG]),
        ("ts_vrcg", config[CONF_TS_VRCG]),
    )

    mask_id = cg.StructInitializer(
        BQ25186MaskIdConfig,
        ("mask_ts_interrupt", config[CONF_MASK_TS_INTERRUPT]),
        ("mask_treg_interrupt", config[CONF_MASK_TREG_INTERRUPT]),
        ("mask_bat_interrupt", config[CONF_MASK_BAT_INTERRUPT]),
        ("mask_pg_interrupt", config[CONF_MASK_PG_INTERRUPT]),
    )

    component_config = cg.StructInitializer(
        BQ25186Config,
        ("vbat_ctrl", vbat_ctrl),
        ("ichg_ctrl", ichg_ctrl),
        ("chargectrl0", chargectrl0),
        ("chargectrl1", chargectrl1),
        ("ic_ctrl", ic_ctrl),
        ("tmr_ilim", tmr_ilim),
        ("ship_rst", ship_rst),
        ("sys_reg", sys_reg),
        ("ts_control", ts_control),
        ("mask_id", mask_id),
    )
    cg.add(var.set_setup_config(component_config))
    cg.add(var.set_configure_on_boot(config[CONF_CONFIGURE_ON_BOOT]))


@automation.register_action(
    "bq25186.configure",
    BQ25186ConfigureAction,
    automation.maybe_simple_id(
        {
            cv.GenerateID(): cv.use_id(BQ25186Component),
        }
    ),
    synchronous=True,
)
async def bq25186_configure_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)
