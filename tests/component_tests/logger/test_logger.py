"""Tests for the logger component."""

from collections.abc import Callable
from pathlib import Path
import re

import pytest

from esphome.components.logger import (
    FINAL_VALIDATE_SCHEMA,
    _validate_libretiny_uart_pins,
    validate_task_log_buffer_alignment,
)
from esphome.config_validation import Invalid
from esphome.const import PlatformFramework
from esphome.core import CORE
from tests.component_tests.types import SetCoreConfigCallable


def test_logger_pre_setup_before_other_components(generate_main):
    """Logger::pre_setup() must be called before any other component is created.

    Log functions call global_logger->log_vprintf_() without a null check,
    so global_logger must be set before anything can log.
    """
    main_cpp = generate_main("tests/component_tests/logger/test_logger.yaml")

    # Find the logger's pre_setup() call specifically
    logger_pre_setup = re.search(r"logger_logger->pre_setup\(\)", main_cpp)
    if logger_pre_setup is None:
        # Fall back to finding any logger-related pre_setup
        logger_pre_setup = re.search(r"logger\w*->pre_setup\(\)", main_cpp)
    assert logger_pre_setup is not None, (
        "Logger pre_setup() not found in generated code"
    )

    # Find all "new " allocations (component creation)
    new_allocations = list(re.finditer(r"\bnew [\w:]+", main_cpp))
    # Find all "new(" allocations (component creation) and combine them
    new_allocations.extend(re.finditer(r"\bnew\([^)]+\) [\w:]+", main_cpp))
    # Sort allocations by position in the file
    new_allocations.sort(key=lambda m: m.start())
    assert len(new_allocations) > 0, "No component allocations found"

    # Separate logger and non-logger allocations
    logger_allocs = [a for a in new_allocations if "logger" in a.group().lower()]
    non_logger_allocs = [
        a
        for a in new_allocations
        if "logger" not in a.group().lower()
        # Skip placement new for App
        and "(&App)" not in main_cpp[max(0, a.start() - 5) : a.start()]
    ]

    assert len(logger_allocs) > 0, (
        f"Logger allocation not found in: {[a.group() for a in new_allocations]}"
    )
    assert len(non_logger_allocs) > 0, (
        "No non-logger component allocations found — "
        "add a component to test_logger.yaml so the ordering check is meaningful"
    )

    # All non-logger allocations must appear after logger pre_setup()
    for alloc in non_logger_allocs:
        assert alloc.start() > logger_pre_setup.start(), (
            f"Component allocation '{alloc.group()}' at position {alloc.start()} "
            f"appears before logger pre_setup() at position {logger_pre_setup.start()}"
        )


def test_default_uart_selection_is_not_emitted(generate_main):
    """UART0 is the C++ initializer on ESP8266, so the setter is skipped."""
    main_cpp = generate_main("tests/component_tests/logger/test_logger.yaml")

    assert "set_uart_selection(" not in main_cpp


def test_custom_uart_selection_is_emitted(generate_main):
    """A non default UART still reaches the setter before pre_setup()."""
    main_cpp = generate_main("tests/component_tests/logger/test_logger_uart1.yaml")

    assert "set_uart_selection(logger::UART_SELECTION_UART1);" in main_cpp


def test_libretiny_default_uart_selection_is_not_emitted(generate_main):
    """DEFAULT is the C++ initializer on LibreTiny, so the setter is skipped."""
    main_cpp = generate_main(
        "tests/component_tests/logger/test_logger_libretiny_default.yaml"
    )

    assert "set_uart_selection(" not in main_cpp


def test_libretiny_uart0_is_emitted(generate_main):
    """UART0 is not the LibreTiny initializer, so it must still be set."""
    main_cpp = generate_main(
        "tests/component_tests/logger/test_logger_libretiny_uart0.yaml"
    )

    assert "set_uart_selection(logger::UART_SELECTION_UART0);" in main_cpp


def test_ram_log_strings_is_deprecated(
    generate_main: Callable[[str | Path], str],
    caplog: pytest.LogCaptureFixture,
) -> None:
    """`false` is accepted but ignored: no RAM build flag, and a warning with the removal version."""
    generate_main("tests/component_tests/logger/test_logger_ram_log_strings.yaml")

    assert not any("STORE_LOG_STR" in flag for flag in CORE.build_flags)
    assert "esp8266_store_log_strings_in_flash: false' is ignored" in caplog.text
    assert "2027.4.0" in caplog.text


def test_flash_log_strings_default_does_not_warn(
    generate_main: Callable[[str | Path], str],
    caplog: pytest.LogCaptureFixture,
) -> None:
    generate_main("tests/component_tests/logger/test_logger.yaml")

    assert "esp8266_store_log_strings_in_flash" not in caplog.text


@pytest.mark.parametrize("value", [0, 640, 768, 32768])
def test_task_log_buffer_size_aligned_on_esp32(
    set_core_config: SetCoreConfigCallable, value: int
) -> None:
    set_core_config(PlatformFramework.ESP32_IDF)
    assert validate_task_log_buffer_alignment(value) == value


def test_task_log_buffer_size_unaligned_rejected_on_esp32(
    set_core_config: SetCoreConfigCallable,
) -> None:
    set_core_config(PlatformFramework.ESP32_IDF)
    with pytest.raises(Invalid, match="multiple of 4"):
        validate_task_log_buffer_alignment(641)


def test_task_log_buffer_size_unaligned_allowed_on_libretiny(
    set_core_config: SetCoreConfigCallable,
) -> None:
    set_core_config(PlatformFramework.BK72XX_ARDUINO)
    assert validate_task_log_buffer_alignment(641) == 641


@pytest.mark.parametrize(
    ("config", "log_v2"),
    [
        pytest.param("test_logger_idf6_esp_idf.yaml", True, id="native"),
        pytest.param("test_logger_idf6_platformio.yaml", False, id="platformio"),
    ],
)
def test_log_v2_only_on_native_toolchain(
    generate_main: Callable[[str | Path], str],
    config: str,
    log_v2: bool,
) -> None:
    """PlatformIO links the bootloader with the app's build flags, so the wrap has no target there."""
    from esphome.components.esp32 import KEY_ESP32, KEY_SDKCONFIG_OPTIONS

    generate_main(f"tests/component_tests/logger/{config}")

    assert ("-Wl,--wrap=esp_log_format" in CORE.build_flags) is log_v2
    assert ("USE_ESP32_LOG_V2" in {d.name for d in CORE.defines}) is log_v2
    sdkconfig = CORE.data[KEY_ESP32][KEY_SDKCONFIG_OPTIONS]
    assert ("CONFIG_LOG_VERSION_2" in sdkconfig) is log_v2


@pytest.mark.parametrize(
    ("config", "usb_cdc"),
    [
        pytest.param("test_logger_rp2_usb_cdc.yaml", True, id="usb_cdc"),
        pytest.param("test_logger_rp2_uart0.yaml", False, id="uart0"),
    ],
)
def test_rp2_usb_cdc_selection_define(
    generate_main: Callable[[str | Path], str],
    config: str,
    usb_cdc: bool,
) -> None:
    """The USB CDC crash replay only compiles in when the logger uses USB CDC."""
    generate_main(f"tests/component_tests/logger/{config}")

    assert (
        "USE_LOGGER_UART_SELECTION_USB_CDC" in {d.name for d in CORE.defines}
    ) is usb_cdc


@pytest.mark.parametrize(
    ("board", "hardware_uart", "valid"),
    [
        pytest.param("bw15", "UART1", False, id="bw15_uart1_two_pin_choices"),
        pytest.param("bw15", "UART0", True, id="bw15_uart0"),
        pytest.param("bw15", "UART2", True, id="bw15_uart2"),
        pytest.param("bw15", "DEFAULT", True, id="bw15_default"),
        pytest.param("not-a-board", "UART1", True, id="unknown_board"),
        # bw12 has no UART1, which falls back to the default port
        pytest.param("bw12", "UART1", True, id="bw12_missing_port"),
    ],
)
def test_libretiny_uart_needs_default_pins(
    set_core_config: SetCoreConfigCallable,
    board: str,
    hardware_uart: str,
    valid: bool,
) -> None:
    """Opening a LibreTiny port with no default pins hangs at boot, so it is rejected."""
    from esphome.components import rtl87xx
    from esphome.components.libretiny.const import (
        KEY_BOARD,
        KEY_COMPONENT_DATA,
        KEY_LIBRETINY,
    )

    set_core_config(PlatformFramework.RTL87XX_ARDUINO)
    CORE.data[KEY_LIBRETINY] = {
        KEY_COMPONENT_DATA: rtl87xx.COMPONENT_DATA,
        KEY_BOARD: board,
    }
    if valid:
        _validate_libretiny_uart_pins(hardware_uart)
    else:
        with pytest.raises(Invalid, match="no default pins for UART1"):
            _validate_libretiny_uart_pins(hardware_uart)


@pytest.mark.parametrize(
    ("platform", "baud_rate", "valid"),
    [
        pytest.param(PlatformFramework.RTL87XX_ARDUINO, 115200, False, id="serial"),
        pytest.param(PlatformFramework.RTL87XX_ARDUINO, 0, True, id="no_serial"),
        pytest.param(
            PlatformFramework.ESP8266_ARDUINO, 115200, True, id="not_libretiny"
        ),
    ],
)
def test_final_validate_checks_libretiny_uart_pins(
    set_core_config: SetCoreConfigCallable,
    platform: PlatformFramework,
    baud_rate: int,
    valid: bool,
) -> None:
    """The pin check runs from final validation, only when serial logging is on."""
    from esphome.components import rtl87xx
    from esphome.components.libretiny.const import (
        KEY_BOARD,
        KEY_COMPONENT_DATA,
        KEY_LIBRETINY,
    )
    from esphome.const import CONF_BAUD_RATE, CONF_HARDWARE_UART

    set_core_config(platform)
    CORE.data[KEY_LIBRETINY] = {
        KEY_COMPONENT_DATA: rtl87xx.COMPONENT_DATA,
        KEY_BOARD: "bw15",
    }
    config = {CONF_BAUD_RATE: baud_rate, CONF_HARDWARE_UART: "UART1"}
    if valid:
        FINAL_VALIDATE_SCHEMA(config)
    else:
        with pytest.raises(Invalid, match="no default pins for UART1"):
            FINAL_VALIDATE_SCHEMA(config)
