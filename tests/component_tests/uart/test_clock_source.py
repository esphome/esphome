"""Tests for UART clock source validation and code generation."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.components.esp32 import KEY_VARIANT, VARIANTS
from esphome.components.uart import (
    CONF_CLOCK_SOURCE,
    CONFIG_SCHEMA,
    UART_CLOCK_SOURCES,
    UART_CLOCK_SOURCES_BY_VARIANT,
)
from esphome.config import read_config
import esphome.config_validation as cv
from esphome.const import PlatformFramework
from esphome.core import CORE
from tests.component_tests.types import SetCoreConfigCallable


@pytest.fixture
def clock_source_validator() -> cv.All:
    """Find the clock validator without depending on pre-validator ordering."""
    schema = next(
        validator
        for validator in CONFIG_SCHEMA.validators
        if isinstance(validator, cv.Schema)
    )
    return schema.schema[cv.Optional(CONF_CLOCK_SOURCE)]


def test_clock_source_table_covers_all_variants() -> None:
    """New ESP32 variants must declare their supported UART clocks."""
    assert set(UART_CLOCK_SOURCES_BY_VARIANT) == set(VARIANTS)
    for sources in UART_CLOCK_SOURCES_BY_VARIANT.values():
        assert "DEFAULT" in sources
        assert set(sources) <= UART_CLOCK_SOURCES.keys()


@pytest.mark.parametrize(
    ("variant", "source", "supported"),
    [
        ("ESP32", "REF_TICK", True),
        ("ESP32", "XTAL", False),
        ("ESP32S2", "REF_TICK", True),
        ("ESP32S2", "RTC", False),
        ("ESP32C3", "APB", True),
        ("ESP32C3", "xtal", True),
        ("ESP32C3", "REF_TICK", False),
        ("ESP32S3", "RTC", True),
        ("ESP32S3", "REF_TICK", False),
        ("ESP32C2", "APB", False),
        ("ESP32C6", "APB", False),
        ("ESP32C6", "DEFAULT", True),
        ("ESP32C5", "RTC", True),
        ("ESP32C61", "XTAL", True),
        ("ESP32H4", "RTC", True),
        ("ESP32H21", "XTAL", True),
        ("ESP32S31", "RTC", True),
        ("ESP32H2", "XTAL", True),
        ("ESP32P4", "RTC", True),
        ("ESP32C3", "INVALID", False),
    ],
)
def test_clock_source_validation(
    variant: str,
    source: str,
    supported: bool,
    set_core_config: SetCoreConfigCallable,
    clock_source_validator: cv.All,
) -> None:
    """Reject unsupported clocks and normalize accepted names."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_VARIANT: variant},
    )
    if supported:
        assert clock_source_validator(source) == source.upper()
    else:
        with pytest.raises(cv.Invalid):
            clock_source_validator(source)


def test_clock_source_requires_esp32(
    set_core_config: SetCoreConfigCallable,
    clock_source_validator: cv.All,
) -> None:
    """Reject the ESP32-only option before accessing the chip variant."""
    set_core_config(PlatformFramework.ESP8266_ARDUINO)
    with pytest.raises(cv.Invalid, match="ESP32"):
        clock_source_validator("DEFAULT")


@pytest.mark.parametrize("source", [None, "default", "xtal"])
def test_clock_source_codegen(
    source: str | None,
    tmp_path: Path,
    generate_main: Callable[[str | Path], str],
) -> None:
    """Emit IDF constants directly and skip the setter when nothing changes."""
    config = tmp_path / "uart.yaml"
    config.write_text(
        "esphome:\n  name: uart-clock-test\n"
        "esp32:\n  variant: esp32c3\n  framework:\n    type: esp-idf\n"
        "uart:\n  id: test_uart\n  tx_pin: GPIO4\n  baud_rate: 9600\n"
        + (f"  clock_source: {source}\n" if source else ""),
        encoding="utf-8",
    )
    main_cpp = generate_main(config)
    if source in (None, "default"):
        assert "set_clock_source(" not in main_cpp
    else:
        assert f"test_uart->set_clock_source(::UART_SCLK_{source.upper()});" in main_cpp


@pytest.mark.parametrize(
    ("variant", "baud_rate", "source", "power_management", "expected"),
    [
        pytest.param("esp32c3", 9600, None, True, "XTAL", id="c3_xtal"),
        pytest.param("esp32s3", 115200, None, True, "XTAL", id="s3_xtal"),
        pytest.param("esp32", 9600, None, True, "REF_TICK", id="esp32_ref_tick"),
        pytest.param("esp32s2", 250000, None, True, "REF_TICK", id="s2_ref_tick"),
        pytest.param("esp32", 460800, None, True, "REF_TICK", id="esp32_fast_auto"),
        pytest.param("esp32c6", 9600, None, True, None, id="c6_unchanged"),
        pytest.param("esp32c3", 9600, None, False, None, id="no_power_management"),
        pytest.param("esp32c3", 9600, "apb", True, "APB", id="user_choice_wins"),
        pytest.param("esp32c3", 9600, "default", True, None, id="user_default_wins"),
    ],
)
def test_clock_source_with_power_management(
    variant: str,
    baud_rate: int,
    source: str | None,
    power_management: bool,
    expected: str | None,
    tmp_path: Path,
    generate_main: Callable[[str | Path], str],
) -> None:
    """Power management moves an APB-clocked UART to a clock that keeps its rate."""
    config = tmp_path / "uart.yaml"
    config.write_text(
        "esphome:\n  name: uart-clock-test\n"
        f"esp32:\n  variant: {variant}\n  framework:\n    type: esp-idf\n"
        f"uart:\n  id: test_uart\n  rx_pin: GPIO4\n  baud_rate: {baud_rate}\n"
        + (f"  clock_source: {source}\n" if source else "")
        + (
            "power_management:\n  - platform: esp32_power_management\n"
            if power_management
            else ""
        ),
        encoding="utf-8",
    )
    main_cpp = generate_main(config)
    if expected is None:
        assert "clock_source(" not in main_cpp
    elif source is None:
        assert (
            f"test_uart->set_automatic_clock_source(::UART_SCLK_{expected});"
            in main_cpp
        )
    else:
        assert f"test_uart->set_clock_source(::UART_SCLK_{expected});" in main_cpp


@pytest.mark.parametrize(
    ("baud_rate", "source", "min_frequency", "warned"),
    [
        pytest.param(250001, None, None, True, id="above_limit"),
        pytest.param(250000, None, None, False, id="at_limit"),
        pytest.param(460800, "apb", None, False, id="user_choice"),
        pytest.param(460800, None, "40MHz", True, id="min_below_80"),
        pytest.param(460800, None, "80MHz", False, id="min_keeps_apb"),
    ],
)
def test_apb_fallback_warning_at_validation(
    baud_rate: int,
    source: str | None,
    min_frequency: str | None,
    warned: bool,
    tmp_path: Path,
    caplog: pytest.LogCaptureFixture,
) -> None:
    """`esphome config` shows the warning, not just a compile."""
    config = tmp_path / "uart.yaml"
    config.write_text(
        "esphome:\n  name: uart-clock-test\n"
        "esp32:\n  variant: esp32\n  framework:\n    type: esp-idf\n"
        f"uart:\n  id: test_uart\n  rx_pin: GPIO4\n  baud_rate: {baud_rate}\n"
        + (f"  clock_source: {source}\n" if source else "")
        + "power_management:\n  - platform: esp32_power_management\n"
        + (f"    min_frequency: {min_frequency}\n" if min_frequency else ""),
        encoding="utf-8",
    )
    CORE.config_path = config
    result = read_config({})
    assert result is not None
    assert ("stays on the APB clock" in caplog.text) is warned
