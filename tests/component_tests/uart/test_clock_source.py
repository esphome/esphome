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
import esphome.config_validation as cv
from esphome.const import PlatformFramework
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
    """Emit IDF constants directly and preserve the omitted-option default."""
    config = tmp_path / "uart.yaml"
    config.write_text(
        "esphome:\n  name: uart-clock-test\n"
        "esp32:\n  variant: esp32c3\n  framework:\n    type: esp-idf\n"
        "uart:\n  id: test_uart\n  tx_pin: GPIO4\n  baud_rate: 9600\n"
        + (f"  clock_source: {source}\n" if source else ""),
        encoding="utf-8",
    )
    main_cpp = generate_main(config)
    if source is None:
        assert "set_clock_source(" not in main_cpp
    else:
        assert f"test_uart->set_clock_source(::UART_SCLK_{source.upper()});" in main_cpp
