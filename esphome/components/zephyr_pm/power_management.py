import esphome.codegen as cg
from esphome.components import power_management, zephyr
from esphome.components.zephyr.const import KEY_BOARD
from esphome.components.zephyr.dts_lookup import (
    board_has_cpu_freq_pstates,
    board_has_pm_states,
)
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_OPENTHREAD, CONF_PLATFORM
from esphome.core import CORE, EsphomeError
import esphome.final_validate as fv

from .const import (
    CONF_ENABLE_LIGHT_SLEEP,
    CONF_POLICY,
    CONF_POWER_DOWN_DEVICE,
    CONF_STATS,
    POLICIES,
)

CODEOWNERS = ["@rwrozelle"]
DEPENDENCIES = ["zephyr"]

zephyr_pm_ns = cg.esphome_ns.namespace("zephyr_pm")
PowerManagement = zephyr_pm_ns.class_(
    "ZephyrPowerManagement", power_management.PowerManagementComponent
)


def _has_pm() -> bool:
    # DTS isn't fetched until zephyr's own to_code() -- only call this (or anything
    # depending on it) from to_code(), never from FINAL_VALIDATE_SCHEMA.
    return board_has_pm_states(zephyr.zephyr_data()[KEY_BOARD])


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(PowerManagement),
        # No schema defaults -- whether these mean anything depends on the board's
        # DTS, not known until to_code(), which decides what "unset" means there.
        cv.Optional(CONF_ENABLE_LIGHT_SLEEP): cv.boolean,
        cv.Optional(CONF_POWER_DOWN_DEVICE): cv.boolean,
        cv.Optional(CONF_STATS): cv.boolean,
        cv.Optional(CONF_POLICY): cv.one_of(*POLICIES, lower=True),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await power_management.register_power_management(var, config)

    has_pm = _has_pm()
    enable_light_sleep = config.get(CONF_ENABLE_LIGHT_SLEEP)
    if not has_pm and enable_light_sleep is not None:
        raise EsphomeError(
            f"{CONF_ENABLE_LIGHT_SLEEP}: not applicable on this board -- it has "
            "no CONFIG_PM support in Zephyr"
        )
    light_sleep = has_pm and enable_light_sleep is True

    is_esp32 = zephyr.zephyr_variant_family() == "esp32"
    if light_sleep:
        zephyr.zephyr_add_prj_conf("PM", True)
        if is_esp32:
            # rtc_timer is disabled by default in every Espressif SoC dtsi, and
            # soc/espressif/common/power.c requires it ready and flagged as a wakeup
            # source to actually enter light sleep -- without this it silently
            # no-ops PM_STATE_STANDBY. PM_DEVICE is what makes the wakeup-source
            # flag exist at all (PM_DEVICE_DT_INST_DEFINE() is a no-op without it),
            # independent of whether power_down_device below is also requested.
            zephyr.zephyr_add_overlay(
                '&rtc_timer { status = "okay"; wakeup-source; };\n'
            )
            zephyr.zephyr_add_prj_conf("COUNTER", True)
            zephyr.zephyr_add_prj_conf("PM_DEVICE", True)
            if CORE.config.get(CONF_OPENTHREAD):
                # Without this, light sleep can engage mid-TX/RX and corrupt the radio.
                zephyr.zephyr_add_prj_conf("IEEE802154_ESP32_SLEEP_ENABLE", True)

    if config.get(CONF_POWER_DOWN_DEVICE):
        # Only suspends devices whose own driver implements PM_DEVICE hooks --
        # others are silently skipped (-ENOSYS), which this component can't detect.
        zephyr.zephyr_add_prj_conf("PM_DEVICE", True)
        if light_sleep:
            zephyr.zephyr_add_prj_conf("PM_DEVICE_SYSTEM_MANAGED", True)
        else:
            # No CONFIG_PM, so no system-suspend event for SYSTEM_MANAGED to hook.
            # RUNTIME alone leaves devices active unless DEFAULT_ENABLE opts them in.
            zephyr.zephyr_add_prj_conf("PM_DEVICE_RUNTIME", True)
            zephyr.zephyr_add_prj_conf("PM_DEVICE_RUNTIME_DEFAULT_ENABLE", True)
    elif light_sleep and is_esp32:
        # PM_DEVICE above is only for rtc_timer's own wakeup-source flag -- without
        # power_down_device, suppress its unrelated default (PM_DEVICE_SYSTEM_MANAGED
        # defaults to y whenever PM_DEVICE is on and PM_DEVICE_RUNTIME isn't) of
        # suspending every PM_DEVICE-capable device around every sleep/wake.
        zephyr.zephyr_add_prj_conf("PM_DEVICE_SYSTEM_MANAGED", False)

    policy = config.get(CONF_POLICY)
    if policy is not None:
        if not board_has_cpu_freq_pstates(zephyr.zephyr_data()[KEY_BOARD]):
            raise EsphomeError(
                f"{CONF_POLICY}: this board's devicetree declares no CPU "
                "frequency P-states, so no P-state setter backs any policy "
                "here -- selecting one would enable CONFIG_CPU_FREQ for no "
                "effect"
            )
        zephyr.zephyr_add_prj_conf("CPU_FREQ", True)
        zephyr.zephyr_add_prj_conf(f"CPU_FREQ_POLICY_{policy.upper()}", True)

    if config.get(CONF_STATS):
        # Catch-all: profiles whichever of PM/CPU_FREQ is actually active.
        if not light_sleep and policy is None:
            raise EsphomeError(
                f"{CONF_STATS}: True requires {CONF_ENABLE_LIGHT_SLEEP} or "
                f"{CONF_POLICY} to be set -- there is nothing to profile otherwise"
            )
        if light_sleep:
            zephyr.zephyr_add_prj_conf("PM_STATS", True)
            zephyr.zephyr_add_prj_conf("STATS", True)
        if policy is not None:
            # CPU_FREQ_LOG_LEVEL_DBG's choice depends on LOG.
            zephyr.zephyr_add_prj_conf("LOG", True)
            zephyr.zephyr_add_prj_conf("CPU_FREQ_LOG_LEVEL_DBG", True)


def _pm_final_validate(config):
    full_config = fv.full_config.get()
    pm_entries = full_config.get("power_management", [])
    if sum(1 for entry in pm_entries if entry.get(CONF_PLATFORM) == "zephyr_pm") > 1:
        raise cv.Invalid("Only one zephyr_pm instance is allowed")


FINAL_VALIDATE_SCHEMA = _pm_final_validate
