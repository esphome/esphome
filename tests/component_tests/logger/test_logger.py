"""Tests for the logger component."""

from collections.abc import Callable
from pathlib import Path
import re

import pytest

from esphome.components.logger import uart_selection, validate_task_log_buffer_alignment
from esphome.components.zephyr.const import ZEPHYR_VARIANT_NRF52
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


def test_nrf52_emits_logger_node_label(
    generate_main: Callable[[str | Path], str],
) -> None:
    """logger_zephyr.cpp only binds a UART when this label is defined."""
    generate_main("tests/component_tests/logger/test_logger_nrf52_uart0.yaml")
    defines = {d.name: str(d.value) for d in CORE.defines}
    assert defines.get("LOGGER_UART_NODE_LABEL") == "uart0"


def test_nrf52_usb_cdc_leaves_label_to_cpp_fallback(
    generate_main: Callable[[str | Path], str],
) -> None:
    """platform: nrf52 claims CDC port 0; logger_zephyr.cpp falls back to cdc_acm_uart0."""
    generate_main("tests/component_tests/logger/test_logger_nrf52_usb_cdc.yaml")
    assert not any(d.name == "LOGGER_CDC_ACM_UART_LABEL" for d in CORE.defines)


def test_any_uart_number_accepted_on_zephyr(
    set_core_config: SetCoreConfigCallable,
) -> None:
    set_core_config(
        PlatformFramework.ZEPHYR_ZEPHYR,
        platform_data={"variant": ZEPHYR_VARIANT_NRF52, "family": "nordic"},
    )
    assert uart_selection("uart3") == "UART3"
    assert uart_selection("UART12") == "UART12"


def test_zephyr_uart3_codegen(
    monkeypatch: pytest.MonkeyPatch, generate_main: Callable[[str | Path], str]
) -> None:
    """UART2 and up have no UART_SELECTION_* on Zephyr and map to the UART0 default."""
    from esphome.components.zephyr import dts_lookup  # noqa: PLC0415

    # Resolve UART<n> to uart<n> without reading the board's devicetree
    monkeypatch.setattr(
        dts_lookup,
        "resolve_uart_node_label",
        lambda board, hw_uart, labels: hw_uart.lower(),
    )
    main_cpp = generate_main(
        "tests/component_tests/logger/test_logger_zephyr_uart3.yaml"
    )
    assert "set_uart_selection(" not in main_cpp
    defines = {d.name: str(d.value) for d in CORE.defines}
    assert defines["LOGGER_UART_NODE_LABEL"] == "uart3"


@pytest.mark.parametrize("value", ["rtt0", "Rtt2", "RAM", "semihost"])
def test_debug_probe_outputs_accepted_on_zephyr(
    set_core_config: SetCoreConfigCallable, value: str
) -> None:
    set_core_config(
        PlatformFramework.ZEPHYR_ZEPHYR,
        platform_data={"variant": ZEPHYR_VARIANT_NRF52, "family": "nordic"},
    )
    assert uart_selection(value) == value.upper()


def test_rtt_rejected_on_family_without_rtt(
    set_core_config: SetCoreConfigCallable,
) -> None:
    set_core_config(
        PlatformFramework.ZEPHYR_ZEPHYR,
        platform_data={"variant": "ESP32", "family": "esp32"},
    )
    with pytest.raises(Invalid, match="esp32 does not support"):
        uart_selection("RTT0")


def test_debug_probe_outputs_rejected_off_zephyr(
    set_core_config: SetCoreConfigCallable,
) -> None:
    set_core_config(PlatformFramework.ESP8266_ARDUINO)
    with pytest.raises(Invalid):
        uart_selection("RTT0")


def test_semihost_warns(
    set_core_config: SetCoreConfigCallable, caplog: pytest.LogCaptureFixture
) -> None:
    set_core_config(
        PlatformFramework.ZEPHYR_ZEPHYR,
        platform_data={"variant": ZEPHYR_VARIANT_NRF52, "family": "nordic"},
    )
    uart_selection("SEMIHOST")
    assert "no debugger is attached" in caplog.text


def test_zephyr_rtt_codegen(generate_main: Callable[[str | Path], str]) -> None:
    from esphome.components.zephyr import zephyr_data  # noqa: PLC0415
    from esphome.components.zephyr.const import KEY_OVERLAY, KEY_PRJ_CONF  # noqa: PLC0415

    main_cpp = generate_main("tests/component_tests/logger/test_logger_zephyr_rtt.yaml")
    assert "set_uart_selection(" not in main_cpp
    defines = {d.name: str(d.value) for d in CORE.defines}
    assert defines["LOGGER_UART_NODE_LABEL"] == "rtt0"
    assert "USE_LOGGER_CONSOLE" not in defines
    prj_conf = zephyr_data()[KEY_PRJ_CONF][""]
    assert prj_conf["CONFIG_USE_SEGGER_RTT"] == (True, True)
    assert prj_conf["CONFIG_LOG_BACKEND_RTT"] == (False, False)
    overlay = "".join(zephyr_data()[KEY_OVERLAY].values())
    assert 'rtt0: rtt0 { compatible = "segger,rtt-uart"; }' in overlay
    assert "zephyr,console = &rtt0;" in overlay


def test_zephyr_semihost_codegen(generate_main: Callable[[str | Path], str]) -> None:
    from esphome.components.zephyr import zephyr_data  # noqa: PLC0415
    from esphome.components.zephyr.const import KEY_PRJ_CONF  # noqa: PLC0415

    main_cpp = generate_main(
        "tests/component_tests/logger/test_logger_zephyr_semihost.yaml"
    )
    assert "set_uart_selection(" not in main_cpp
    defines = {d.name: str(d.value) for d in CORE.defines}
    assert "USE_LOGGER_CONSOLE" in defines
    assert defines["LOGGER_CONSOLE_NAME"] == '"SEMIHOST"'
    assert "LOGGER_UART_NODE_LABEL" not in defines
    prj_conf = zephyr_data()[KEY_PRJ_CONF][""]
    for option in ("CONSOLE", "PRINTK", "SEMIHOST", "SEMIHOST_CONSOLE"):
        assert prj_conf[f"CONFIG_{option}"] == (True, True)
    assert prj_conf["CONFIG_RTT_CONSOLE"] == (False, False)


def test_zephyr_rtt_reuses_board_node(
    monkeypatch: pytest.MonkeyPatch, generate_main: Callable[[str | Path], str]
) -> None:
    """A board that already declares rtt0 gets it enabled, not a duplicate label."""
    from esphome.components.zephyr import dts_lookup, zephyr_data  # noqa: PLC0415
    from esphome.components.zephyr.const import KEY_OVERLAY  # noqa: PLC0415

    monkeypatch.setattr(
        dts_lookup, "dts_node_label_exists", lambda board, label: label == "rtt0"
    )
    generate_main("tests/component_tests/logger/test_logger_zephyr_rtt.yaml")
    overlay = "".join(zephyr_data()[KEY_OVERLAY].values())
    assert '&rtt0 { status = "okay"; };' in overlay
    assert "rtt0: rtt0" not in overlay


def test_rtt3_not_a_logger_keyword(set_core_config: SetCoreConfigCallable) -> None:
    """Channel 3 needs SEGGER's buffer counts raised; it is only reachable as "&rtt3"."""
    set_core_config(
        PlatformFramework.ZEPHYR_ZEPHYR,
        platform_data={"variant": ZEPHYR_VARIANT_NRF52, "family": "nordic"},
    )
    with pytest.raises(Invalid):
        uart_selection("RTT3")
