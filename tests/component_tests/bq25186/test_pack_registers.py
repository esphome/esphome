"""Tests for the bq25186 register packer."""

import pytest

from esphome import config_validation as cv
from esphome.components.bq25186 import (
    BQ25186_CONFIG_SCHEMA,
    CONF_BATTERY_REGULATION_VOLTAGE,
    CONF_CHARGE_ENABLED,
    CONF_FAST_CHARGE_CURRENT,
    CONF_MASK_TS_INTERRUPT,
    CONF_PUSH_BUTTON_LONG_PRESS_ACTION,
    CONF_WATCHDOG_15S_ENABLE,
    CONFIG_SCHEMA,
    REG_ICHG_CTRL,
    REG_MASK_ID,
    REG_SHIP_RST,
    REG_VBAT_CTRL,
    _check_watchdog,
    pack_registers,
)
from esphome.const import CONF_UPDATE_INTERVAL, CONF_WATCHDOG
from esphome.types import ConfigType

# Datasheet power-on values for VBAT_CTRL..MASK_ID, except that IC_CTRL has the
# watchdog disabled (0x87 instead of 0x84) and MASK_ID carries only its
# configurable bits 7:4 (0x40 instead of 0x41 with the device id).
DEFAULT_REGISTERS = [0x46, 0x05, 0x24, 0xC6, 0x87, 0x4D, 0x11, 0x40, 0x00, 0x40]


def _packed(reg: int, config: ConfigType | None = None) -> int:
    regs = pack_registers(BQ25186_CONFIG_SCHEMA(config or {}))
    return regs[reg - REG_VBAT_CTRL]


def test_defaults_match_power_on_values() -> None:
    assert pack_registers(BQ25186_CONFIG_SCHEMA({})) == DEFAULT_REGISTERS


def test_battery_regulation_voltage_code() -> None:
    assert _packed(REG_VBAT_CTRL, {CONF_BATTERY_REGULATION_VOLTAGE: "4.1V"}) == 0x3C
    assert _packed(REG_VBAT_CTRL, {CONF_BATTERY_REGULATION_VOLTAGE: "3.5V"}) == 0x00
    assert _packed(REG_VBAT_CTRL, {CONF_BATTERY_REGULATION_VOLTAGE: "4.65V"}) == 0x73


def test_fast_charge_current_code() -> None:
    # 1 mA steps from 5 mA to 35 mA
    assert _packed(REG_ICHG_CTRL, {CONF_FAST_CHARGE_CURRENT: "5mA"}) == 0x00
    assert _packed(REG_ICHG_CTRL, {CONF_FAST_CHARGE_CURRENT: "20mA"}) == 0x0F
    assert _packed(REG_ICHG_CTRL, {CONF_FAST_CHARGE_CURRENT: "35mA"}) == 0x1E
    # 10 mA steps from 40 mA
    assert _packed(REG_ICHG_CTRL, {CONF_FAST_CHARGE_CURRENT: "40mA"}) == 0x1F
    assert _packed(REG_ICHG_CTRL, {CONF_FAST_CHARGE_CURRENT: "200mA"}) == 0x2F
    assert _packed(REG_ICHG_CTRL, {CONF_FAST_CHARGE_CURRENT: "1A"}) == 0x7F


def test_charge_disabled_sets_bit_7() -> None:
    assert _packed(REG_ICHG_CTRL, {CONF_CHARGE_ENABLED: False}) == 0x85
    assert _packed(REG_ICHG_CTRL, {CONF_CHARGE_ENABLED: True}) == 0x05


def test_masked_registers_only_carry_configurable_bits() -> None:
    config: ConfigType = {
        CONF_PUSH_BUTTON_LONG_PRESS_ACTION: "shutdown_mode",
        CONF_MASK_TS_INTERRUPT: True,
    }
    # SHIP_RST bits 7:5 are reset triggers
    ship_rst = _packed(REG_SHIP_RST, config)
    assert ship_rst == 0x19
    assert ship_rst & ~0x1F == 0
    # MASK_ID bits 3:0 are the device id
    mask_id = _packed(REG_MASK_ID, config)
    assert mask_id == 0xC0
    assert mask_id & ~0xF0 == 0


@pytest.mark.parametrize(
    "key, value",
    [
        (CONF_BATTERY_REGULATION_VOLTAGE, "4.105V"),
        (CONF_FAST_CHARGE_CURRENT, "36mA"),
        (CONF_FAST_CHARGE_CURRENT, "39mA"),
        (CONF_FAST_CHARGE_CURRENT, "49mA"),
        (CONF_FAST_CHARGE_CURRENT, "5.5mA"),
        (CONF_FAST_CHARGE_CURRENT, "34.6mA"),
        (CONF_BATTERY_REGULATION_VOLTAGE, "4.2005V"),
    ],
)
def test_values_between_steps_are_rejected(key: str, value: str) -> None:
    with pytest.raises(cv.Invalid):
        BQ25186_CONFIG_SCHEMA({key: value})


def _watchdog_config(interval: str, options: ConfigType) -> ConfigType:
    config = BQ25186_CONFIG_SCHEMA(options)
    config[CONF_UPDATE_INTERVAL] = cv.update_interval(interval)
    return config


@pytest.mark.parametrize(
    "interval, options",
    [
        ("60s", {CONF_WATCHDOG: "160s_software_reset"}),
        ("60s", {CONF_WATCHDOG: "160s_hardware_reset"}),
        ("30s", {CONF_WATCHDOG: "40s_hardware_reset"}),
        ("60s", {CONF_WATCHDOG: "disabled"}),
        ("10s", {CONF_WATCHDOG_15S_ENABLE: True}),
    ],
)
def test_update_interval_within_watchdog_passes(
    interval: str, options: ConfigType
) -> None:
    _check_watchdog(_watchdog_config(interval, options))


@pytest.mark.parametrize(
    "interval, options",
    [
        ("60s", {CONF_WATCHDOG: "40s_hardware_reset"}),
        ("40s", {CONF_WATCHDOG: "40s_hardware_reset"}),
        ("60s", {CONF_WATCHDOG_15S_ENABLE: True}),
        ("20s", {CONF_WATCHDOG: "160s_software_reset", CONF_WATCHDOG_15S_ENABLE: True}),
        ("never", {CONF_WATCHDOG: "160s_software_reset"}),
    ],
)
def test_update_interval_past_watchdog_is_rejected(
    interval: str, options: ConfigType
) -> None:
    with pytest.raises(cv.Invalid, match=CONF_UPDATE_INTERVAL):
        _check_watchdog(_watchdog_config(interval, options))


def test_unconfigured_chip_keeps_its_power_on_watchdog() -> None:
    config = {"configure_on_boot": False, "update_interval": "160s"}
    with pytest.raises(cv.Invalid, match="shorter than 160 s"):
        CONFIG_SCHEMA(config)
    CONFIG_SCHEMA({**config, "update_interval": "60s"})
