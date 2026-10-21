"""Tests for debug:'s RTT crash console on Zephyr and the RTT0 gate."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.config_validation import Invalid
from esphome.const import PlatformFramework
from esphome.core import EsphomeError
from tests.component_tests.types import SetCoreConfigCallable

CONFIG_DIR = Path(__file__).parent / "config"
_RTT_CONSOLE_OPTIONS = (
    "USE_SEGGER_RTT",
    "CONSOLE",
    "RTT_CONSOLE",
    "PRINTK",
    "SEGGER_RTT_MODE_BLOCK_IF_FIFO_FULL",
)


def _prj_conf() -> dict:
    from esphome.components.zephyr import zephyr_data  # noqa: PLC0415
    from esphome.components.zephyr.const import KEY_PRJ_CONF  # noqa: PLC0415

    return zephyr_data()[KEY_PRJ_CONF][""]


def test_debug_enables_rtt_console(generate_main: Callable[[str | Path], str]) -> None:
    from esphome.components.zephyr import zephyr_data  # noqa: PLC0415

    generate_main(CONFIG_DIR / "debug_rtt.yaml")
    prj_conf = _prj_conf()
    for option in _RTT_CONSOLE_OPTIONS:
        assert prj_conf[f"CONFIG_{option}"] == (True, True), option
    assert zephyr_data()["rtt_channels"] == {0: "debug"}


def test_debug_rtt_console_off_by_default(
    generate_main: Callable[[str | Path], str],
) -> None:
    from esphome.components.zephyr import zephyr_data  # noqa: PLC0415

    generate_main(CONFIG_DIR / "debug_rtt_off.yaml")
    assert "CONFIG_RTT_CONSOLE" not in _prj_conf()
    assert zephyr_data()["rtt_channels"] == {}


def test_debug_without_rtt_family(generate_main: Callable[[str | Path], str]) -> None:
    generate_main(CONFIG_DIR / "debug_esp32.yaml")
    assert "CONFIG_RTT_CONSOLE" not in _prj_conf()


def test_rtt_console_rejected_without_rtt_family(
    set_core_config: SetCoreConfigCallable,
) -> None:
    from esphome.components.debug import CONF_ENABLE_RTT_CONSOLE, _validate_rtt_console  # noqa: PLC0415

    set_core_config(
        PlatformFramework.ZEPHYR_ZEPHYR,
        platform_data={"variant": "ESP32", "family": "esp32"},
    )
    with pytest.raises(Invalid, match="needs SEGGER RTT"):
        _validate_rtt_console({CONF_ENABLE_RTT_CONSOLE: True})


@pytest.mark.parametrize(
    ("config", "owner"),
    [
        ("debug_logger_rtt0.yaml", "logger"),
        ("debug_uart_label_rtt0.yaml", "uart 'rtt_bus'"),
    ],
)
def test_rtt0_rejected_with_debug(
    generate_main: Callable[[str | Path], str], config: str, owner: str
) -> None:
    with pytest.raises(EsphomeError, match=f"reserved by debug: .*move {owner} "):
        generate_main(CONFIG_DIR / config)


def test_other_channel_allowed_with_debug(
    generate_main: Callable[[str | Path], str],
) -> None:
    from esphome.components.zephyr import zephyr_data  # noqa: PLC0415

    generate_main(CONFIG_DIR / "debug_uart_rtt1.yaml")
    assert zephyr_data()["rtt_channels"] == {0: "debug", 1: "uart 'rtt_bus'"}


@pytest.mark.parametrize("debug_first", [True, False])
def test_debug_wins_rtt0_in_either_order(
    set_core_config: SetCoreConfigCallable, debug_first: bool
) -> None:
    from esphome.components.zephyr.rtt import DEBUG_OWNER, claim_rtt_channel  # noqa: PLC0415

    set_core_config(PlatformFramework.ZEPHYR_ZEPHYR, platform_data={"rtt_channels": {}})
    owners = [DEBUG_OWNER, "logger"] if debug_first else ["logger", DEBUG_OWNER]
    claim_rtt_channel(0, owners[0])
    with pytest.raises(EsphomeError, match="reserved by debug: .*move logger "):
        claim_rtt_channel(0, owners[1])
