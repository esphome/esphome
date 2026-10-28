"""Tests for the mcxn236 variant's to_code()."""

from __future__ import annotations

import asyncio
from unittest.mock import MagicMock, patch

import pytest

from esphome.components.zephyr.const import BOOTLOADER_MCUBOOT, KEY_BOOTLOADER
from esphome.components.zephyr.variants import mcxn236
from esphome.const import CONF_BOARD, CONF_WATCHDOG_TIMEOUT
from esphome.core import TimePeriod

BOARD = "frdm_mcxn236"
_ZEPHYR = "esphome.components.zephyr"
_DTS = "esphome.components.zephyr.dts_lookup"


def _run_to_code(
    watchdog_ms: int = 10000,
    rng_label: str | None = "trng",
    bootloader: str = BOOTLOADER_MCUBOOT,
) -> tuple[MagicMock, MagicMock, MagicMock]:
    """Run to_code() and return the prj.conf, overlay and sysbuild.conf mocks."""
    config = {
        CONF_BOARD: BOARD,
        CONF_WATCHDOG_TIMEOUT: TimePeriod(milliseconds=watchdog_ms),
    }
    with (
        patch(f"{_ZEPHYR}.zephyr_add_prj_conf") as mock_conf,
        patch(f"{_ZEPHYR}.zephyr_add_overlay") as mock_overlay,
        patch(f"{_ZEPHYR}.zephyr_add_sysbuild_conf") as mock_sysbuild,
        patch(f"{_ZEPHYR}.zephyr_setup_preferences"),
        patch(f"{_ZEPHYR}.zephyr_data", return_value={KEY_BOOTLOADER: bootloader}),
        patch(f"{_DTS}.get_rng_node_label", return_value=rng_label) as mock_rng,
        patch.object(mcxn236.cg, "add_build_flag"),
        patch.object(mcxn236.cg, "add_define"),
    ):
        asyncio.run(mcxn236.to_code(config))
    mock_rng.assert_called_once_with(BOARD, "nxp,els-trng")
    return mock_conf, mock_overlay, mock_sysbuild


def _conf_names(mock_conf: MagicMock) -> list[str]:
    return [call.args[0] for call in mock_conf.call_args_list]


def test_to_code_switches_settings_to_zms() -> None:
    mock_conf, _, _ = _run_to_code()
    mock_conf.assert_any_call("ZMS", True)
    mock_conf.assert_any_call("ZMS_CUSTOMIZE_BLOCK_SIZE", True)
    mock_conf.assert_any_call("ZMS_CUSTOM_BLOCK_SIZE", 128)


@pytest.mark.parametrize(("watchdog_ms", "set_warning"), [(10000, True), (0, False)])
def test_to_code_sets_the_watchdog_warning_only_with_a_watchdog(
    watchdog_ms: int, set_warning: bool
) -> None:
    mock_conf, _, _ = _run_to_code(watchdog_ms=watchdog_ms)
    if set_warning:
        mock_conf.assert_any_call("WDT_MCUX_WWDT_WARNING_INTERRUPT_CFG", 1023)
    else:
        assert "WDT_MCUX_WWDT_WARNING_INTERRUPT_CFG" not in _conf_names(mock_conf)


def test_to_code_uses_the_trng_when_the_soc_has_one() -> None:
    mock_conf, mock_overlay, _ = _run_to_code(rng_label="trng")
    mock_conf.assert_any_call("ENTROPY_GENERATOR", True)
    assert "TEST_RANDOM_GENERATOR" not in _conf_names(mock_conf)
    mock_overlay.assert_called_once_with('&trng { status = "okay"; };')


def test_to_code_falls_back_to_the_test_rng_without_a_trng() -> None:
    mock_conf, mock_overlay, _ = _run_to_code(rng_label=None)
    mock_conf.assert_any_call("TEST_RANDOM_GENERATOR", True)
    assert "ENTROPY_GENERATOR" not in _conf_names(mock_conf)
    mock_overlay.assert_not_called()


@pytest.mark.parametrize(
    ("bootloader", "built"), [(BOOTLOADER_MCUBOOT, True), ("", False)]
)
def test_to_code_builds_mcuboot_only_when_selected(
    bootloader: str, built: bool
) -> None:
    _, _, mock_sysbuild = _run_to_code(bootloader=bootloader)
    if built:
        mock_sysbuild.assert_any_call("BOOTLOADER_MCUBOOT", True)
        mock_sysbuild.assert_any_call("BOOT_SIGNATURE_TYPE_ECDSA_P256", True)
    else:
        mock_sysbuild.assert_not_called()
