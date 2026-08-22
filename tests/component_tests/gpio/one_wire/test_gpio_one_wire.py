"""Tests for the GPIO 1-wire bus component.

Covers:
- ESP32 default (no use_rmt): GPIO bit-bang path is used (esp_driver_rmt stays excluded)
- ESP32 with use_rmt: true: esp_driver_rmt IDF component must be included
- ESP32-C2 (no RMT hardware): esp_driver_rmt must stay excluded
- Non-ESP32 (ESP8266): no esp32 config data; GPIO bit-bang path used
- Config validates correctly for each platform
- use_rmt: false on ESP32 forces GPIO bit-bang (esp_driver_rmt stays excluded)
- use_rmt: true on ESP8266 must raise a validation error
- Two buses on the same ESP32: independent instantiation, registration, pins, and RMT driver
- Two buses on the same ESP8266: independent instantiation and registration (GPIO bit-bang)
- FILTER_SOURCE_FILES: gpio_one_wire_rmt.cpp included only for ESP32, excluded for ESP8266

Two-bus isolation rationale
---------------------------
At runtime each GPIOOneWireBus instance owns its own FreeRTOS queue
(receive_queue_), its own RMT TX/RX channel handles (tx_channel_ /
rx_channel_), and its own rx_symbols_buf_.  The queue cleanup added
to every read/write operation (xQueueReset before rmt_receive) drains
stale events that belong to *this* instance only, so a failure or
timeout on one bus can never corrupt the other bus's queue.

These tests verify the Python code-generation side of that isolation:
- each bus gets its own GPIOOneWireBus object
- each bus is registered as an independent ESPHome component
  (App.register_component is called per-bus; ESPHome's component runner
  will therefore call setup() on both buses independently — a failure
  inside one bus's setup() only marks that bus as failed)
- each bus receives a different InternalGPIOPin object wired to a
  distinct GPIO number (so they can never share RMT channels)
- esp_driver_rmt is un-excluded once, regardless of how many buses
  call include_builtin_idf_component (set.discard is idempotent)
"""

from __future__ import annotations

from collections.abc import Callable
from pathlib import Path

import pytest

HERE = Path(__file__).parent


# ---------------------------------------------------------------------------
# Single-bus tests
# ---------------------------------------------------------------------------


def test_gpio_one_wire_esp32_idf_registers_bus(
    generate_main: Callable[[str | Path], str],
) -> None:
    """ESP32 IDF: GPIOOneWireBus must be instantiated and registered as a component."""
    main_cpp = generate_main(HERE / "test_gpio_one_wire_esp32_idf.yaml")

    assert "new(ow_bus) gpio::GPIOOneWireBus();" in main_cpp
    assert "App.register_component_(ow_bus" in main_cpp


def test_gpio_one_wire_esp32_idf_includes_rmt_driver(
    generate_main: Callable[[str | Path], str],
) -> None:
    """ESP32 IDF + use_rmt: true: esp_driver_rmt must be un-excluded and compiled.

    When explicitly enabled, to_code() calls
    include_builtin_idf_component("esp_driver_rmt"), which removes the driver
    from KEY_EXCLUDE_COMPONENTS.
    """
    generate_main(HERE / "test_gpio_one_wire_esp32_idf_use_rmt_true.yaml")

    # Check that the RMT driver was un-excluded (i.e. it will be compiled)
    from esphome.components.esp32 import KEY_ESP32, KEY_EXCLUDE_COMPONENTS
    from esphome.core import CORE

    excluded = CORE.data.get(KEY_ESP32, {}).get(KEY_EXCLUDE_COMPONENTS, set())
    assert "esp_driver_rmt" not in excluded, (
        "esp_driver_rmt should be included when use_rmt: true is set"
    )


def test_gpio_one_wire_esp32_idf_default_excludes_rmt_driver(
    generate_main: Callable[[str | Path], str],
) -> None:
    """ESP32 IDF default (no use_rmt): esp_driver_rmt must remain excluded."""
    generate_main(HERE / "test_gpio_one_wire_esp32_idf.yaml")

    from esphome.components.esp32 import KEY_ESP32, KEY_EXCLUDE_COMPONENTS
    from esphome.core import CORE

    excluded = CORE.data.get(KEY_ESP32, {}).get(KEY_EXCLUDE_COMPONENTS, set())
    assert "esp_driver_rmt" in excluded, (
        "esp_driver_rmt should remain excluded by default when use_rmt is omitted"
    )


def test_gpio_one_wire_esp32c2_excludes_rmt_driver(
    generate_main: Callable[[str | Path], str],
) -> None:
    """ESP32-C2 (no RMT hardware): esp_driver_rmt must remain excluded.

    The ESP32-C2 variant is in VARIANTS_NO_RMT.  to_code() must NOT call
    include_builtin_idf_component() for it, so the RMT driver stays excluded
    and the GPIO bit-bang code path is compiled instead.
    """
    generate_main(HERE / "test_gpio_one_wire_esp32c2_idf.yaml")

    from esphome.components.esp32 import KEY_ESP32, KEY_EXCLUDE_COMPONENTS
    from esphome.core import CORE

    excluded = CORE.data.get(KEY_ESP32, {}).get(KEY_EXCLUDE_COMPONENTS, set())
    assert "esp_driver_rmt" in excluded, (
        "esp_driver_rmt should remain excluded on ESP32-C2 (no RMT hardware)"
    )


def test_gpio_one_wire_esp32c2_config_valid(
    generate_main: Callable[[str | Path], str],
) -> None:
    """ESP32-C2: Configuration must be accepted without errors (no validation failure).

    The C2 variant uses GPIO bit-bang at runtime (with a warning), but the
    YAML configuration itself is perfectly valid.
    """
    main_cpp = generate_main(HERE / "test_gpio_one_wire_esp32c2_idf.yaml")

    assert "new(ow_bus) gpio::GPIOOneWireBus();" in main_cpp


def test_gpio_one_wire_esp8266_registers_bus(
    generate_main: Callable[[str | Path], str],
) -> None:
    """ESP8266: GPIOOneWireBus must be instantiated using the GPIO bit-bang path."""
    main_cpp = generate_main(HERE / "test_gpio_one_wire_esp8266.yaml")

    assert "new(ow_bus) gpio::GPIOOneWireBus();" in main_cpp
    assert "App.register_component_(ow_bus" in main_cpp


def test_gpio_one_wire_esp8266_no_rmt_data(
    generate_main: Callable[[str | Path], str],
) -> None:
    """ESP8266: No ESP32-specific RMT data should be present in CORE after generation."""
    generate_main(HERE / "test_gpio_one_wire_esp8266.yaml")

    from esphome.components.esp32 import KEY_ESP32
    from esphome.core import CORE

    # On ESP8266 there is no ESP32 platform data at all
    assert KEY_ESP32 not in CORE.data, (
        "ESP32 platform data should not be present when targeting ESP8266"
    )


# ---------------------------------------------------------------------------
# Two-bus isolation tests — ESP32 IDF (RMT path)
# ---------------------------------------------------------------------------


def test_two_buses_esp32_both_instantiated(
    generate_main: Callable[[str | Path], str],
) -> None:
    """Both buses must be instantiated as separate GPIOOneWireBus objects.

    If either bus is missing from the generated code the runtime behaviour
    would be undefined — a bug in code-generation could silently drop one.
    """
    main_cpp = generate_main(HERE / "test_gpio_one_wire_esp32_two_buses.yaml")

    assert "new(ow_bus1) gpio::GPIOOneWireBus();" in main_cpp
    assert "new(ow_bus2) gpio::GPIOOneWireBus();" in main_cpp


def test_two_buses_esp32_registered_independently(
    generate_main: Callable[[str | Path], str],
) -> None:
    """Each bus must be registered as an independent ESPHome component.

    ESPHome's component runner iterates the registered component list and
    calls setup() on each entry.  If a bus is not registered, its setup()
    is never called.  If both buses share a single registration entry, a
    failure in bus1->setup() (which calls mark_failed()) would prevent
    bus2->setup() from running because the component runner skips failed
    components before calling their own setup().

    Two separate App.register_component() calls guarantee that ESPHome
    treats the buses as fully independent: bus1 failing never prevents
    bus2 from being set up.
    """
    main_cpp = generate_main(HERE / "test_gpio_one_wire_esp32_two_buses.yaml")

    assert "App.register_component_(ow_bus1" in main_cpp
    assert "App.register_component_(ow_bus2" in main_cpp

    # Count occurrences: exactly two register_component calls for the two buses
    assert main_cpp.count("App.register_component_(ow_bus") == 2


def test_two_buses_esp32_have_independent_pins(
    generate_main: Callable[[str | Path], str],
) -> None:
    """Each bus must receive its own distinct InternalGPIOPin wired to a different GPIO.

    At runtime, each bus allocates its own RMT TX and RX channels on the GPIO
    it owns.  If both buses shared the same GPIO object they would compete for
    the same RMT channel slot, leading to a setup failure for the second bus.

    This test verifies that the code generator produces separate pin objects
    pointing to different hardware GPIO numbers (GPIO4 for bus1, GPIO17 for
    bus2), ensuring the runtime RMT channel allocation is isolated.
    """
    main_cpp = generate_main(HERE / "test_gpio_one_wire_esp32_two_buses.yaml")

    # Each bus gets its own set_pin() call
    assert "ow_bus1->set_pin(" in main_cpp
    assert "ow_bus2->set_pin(" in main_cpp

    # The two buses reference distinct GPIO pin objects (different _id suffixes)
    assert "ow_bus1->set_pin(esp32_esp32internalgpiopin_id)" in main_cpp
    assert "ow_bus2->set_pin(esp32_esp32internalgpiopin_id_2)" in main_cpp

    # Distinct hardware GPIO numbers
    assert "GPIO_NUM_4" in main_cpp
    assert "GPIO_NUM_17" in main_cpp


def test_two_buses_esp32_rmt_driver_included_once(
    generate_main: Callable[[str | Path], str],
) -> None:
    """esp_driver_rmt must be un-excluded even when two buses both request it.

    Both buses call include_builtin_idf_component("esp_driver_rmt") during
    to_code().  The underlying implementation uses set.discard(), which is
    idempotent — calling it twice leaves the driver correctly un-excluded.
    A bug here (e.g. double-inclusion raising an error, or exclusion being
    re-added) would prevent the RMT path from compiling for either bus.
    """
    generate_main(HERE / "test_gpio_one_wire_esp32_two_buses.yaml")

    from esphome.components.esp32 import KEY_ESP32, KEY_EXCLUDE_COMPONENTS
    from esphome.core import CORE

    excluded = CORE.data.get(KEY_ESP32, {}).get(KEY_EXCLUDE_COMPONENTS, set())
    assert "esp_driver_rmt" not in excluded, (
        "esp_driver_rmt must be un-excluded so both RMT buses compile correctly"
    )


def test_two_buses_esp32_generated_code_order(
    generate_main: Callable[[str | Path], str],
) -> None:
    """Bus1 setup code must appear before bus2 setup code, with no cross-references.

    The generated code must keep each bus's block fully self-contained.
    If bus2's set_pin() referenced bus1's pin object (or vice versa) the
    buses would be coupled and a teardown of one could corrupt the other.
    """
    main_cpp = generate_main(HERE / "test_gpio_one_wire_esp32_two_buses.yaml")

    pos_bus1_new = main_cpp.index("new(ow_bus1) gpio::GPIOOneWireBus();")
    pos_bus1_reg = main_cpp.index("App.register_component_(ow_bus1")
    pos_bus1_pin = main_cpp.index("ow_bus1->set_pin(")
    pos_bus2_new = main_cpp.index("new(ow_bus2) gpio::GPIOOneWireBus();")
    pos_bus2_reg = main_cpp.index("App.register_component_(ow_bus2")
    pos_bus2_pin = main_cpp.index("ow_bus2->set_pin(")

    # Bus1's entire setup block is before bus2's
    assert pos_bus1_new < pos_bus1_reg < pos_bus1_pin < pos_bus2_new, (
        "bus1 setup block must be complete before bus2 setup block begins"
    )
    assert pos_bus2_new < pos_bus2_reg < pos_bus2_pin, (
        "bus2 setup block must follow bus1 and be internally ordered"
    )

    # No cross-references: bus2 lines must not reference bus1's pin object
    bus2_pin_line = next(
        line for line in main_cpp.splitlines() if "ow_bus2->set_pin(" in line
    )
    assert "esp32internalgpiopin_id_2" in bus2_pin_line, (
        "bus2 must use its own pin object, not bus1's"
    )
    assert (
        "esp32internalgpiopin_id)"
        in main_cpp.splitlines()[
            next(
                i
                for i, line in enumerate(main_cpp.splitlines())
                if "ow_bus1->set_pin(" in line
            )
        ]
    ), "bus1 must use its own pin object, not bus2's"


# ---------------------------------------------------------------------------
# Two-bus isolation tests — ESP8266 (GPIO bit-bang path)
# ---------------------------------------------------------------------------


def test_two_buses_esp8266_both_instantiated(
    generate_main: Callable[[str | Path], str],
) -> None:
    """ESP8266: Both GPIO bit-bang buses must be instantiated independently."""
    main_cpp = generate_main(HERE / "test_gpio_one_wire_esp8266_two_buses.yaml")

    assert "new(ow_bus1) gpio::GPIOOneWireBus();" in main_cpp
    assert "new(ow_bus2) gpio::GPIOOneWireBus();" in main_cpp


def test_two_buses_esp8266_registered_independently(
    generate_main: Callable[[str | Path], str],
) -> None:
    """ESP8266: Both buses must be registered as separate ESPHome components."""
    main_cpp = generate_main(HERE / "test_gpio_one_wire_esp8266_two_buses.yaml")

    assert "App.register_component_(ow_bus1" in main_cpp
    assert "App.register_component_(ow_bus2" in main_cpp
    assert main_cpp.count("App.register_component_(ow_bus") == 2


def test_two_buses_esp8266_have_independent_pins(
    generate_main: Callable[[str | Path], str],
) -> None:
    """ESP8266: Each bus must receive its own distinct pin object."""
    main_cpp = generate_main(HERE / "test_gpio_one_wire_esp8266_two_buses.yaml")

    assert "ow_bus1->set_pin(" in main_cpp
    assert "ow_bus2->set_pin(" in main_cpp

    # The two set_pin() calls must reference different pin objects
    pin_lines = [
        line
        for line in main_cpp.splitlines()
        if "->set_pin(" in line and "ow_bus" in line
    ]
    assert len(pin_lines) == 2
    assert pin_lines[0] != pin_lines[1], (
        "bus1 and bus2 must receive distinct pin objects"
    )


# ---------------------------------------------------------------------------
# FILTER_SOURCE_FILES tests
# ---------------------------------------------------------------------------


def test_filter_source_files_rmt_included_for_esp32(
    generate_main: Callable[[str | Path], str],
) -> None:
    """ESP32 IDF: gpio_one_wire_rmt.cpp must NOT be excluded (i.e. it is compiled).

    FILTER_SOURCE_FILES lists gpio_one_wire_rmt.cpp for ESP32_IDF and
    ESP32_ARDUINO only.  For ESP32 IDF the file must remain in the build.
    """
    generate_main(HERE / "test_gpio_one_wire_esp32_idf.yaml")

    from esphome.components.gpio.one_wire import FILTER_SOURCE_FILES

    excluded = FILTER_SOURCE_FILES()
    assert "gpio_one_wire_rmt.cpp" not in excluded, (
        "gpio_one_wire_rmt.cpp must be compiled (not excluded) on ESP32 IDF"
    )


def test_filter_source_files_rmt_excluded_for_esp8266(
    generate_main: Callable[[str | Path], str],
) -> None:
    """ESP8266: gpio_one_wire_rmt.cpp must be excluded (not compiled).

    gpio_one_wire_rmt.cpp contains RMT-specific code that only compiles on
    ESP32.  FILTER_SOURCE_FILES must exclude it for non-ESP32 platforms so
    the build system never attempts to compile it.
    """
    generate_main(HERE / "test_gpio_one_wire_esp8266.yaml")

    from esphome.components.gpio.one_wire import FILTER_SOURCE_FILES

    excluded = FILTER_SOURCE_FILES()
    assert "gpio_one_wire_rmt.cpp" in excluded, (
        "gpio_one_wire_rmt.cpp must be excluded on ESP8266 (no RMT hardware)"
    )


def test_filter_source_files_gpio_never_excluded(
    generate_main: Callable[[str | Path], str],
) -> None:
    """ESP8266: gpio_one_wire.cpp (GPIO bit-bang) must never be excluded.

    gpio_one_wire.cpp contains the GPIO bit-bang fallback which is needed on
    all platforms.  FILTER_SOURCE_FILES must keep it in the build for every
    supported platform.
    """
    generate_main(HERE / "test_gpio_one_wire_esp8266.yaml")

    from esphome.components.gpio.one_wire import FILTER_SOURCE_FILES

    excluded = FILTER_SOURCE_FILES()
    assert "gpio_one_wire.cpp" not in excluded, (
        "gpio_one_wire.cpp (GPIO bit-bang) must always be compiled"
    )


# ---------------------------------------------------------------------------
# use_rmt config option tests
# ---------------------------------------------------------------------------


def test_use_rmt_false_on_esp32_keeps_rmt_driver_excluded(
    generate_main: Callable[[str | Path], str],
) -> None:
    """ESP32 IDF with use_rmt: false must NOT include esp_driver_rmt.

    When the user explicitly sets use_rmt: false, the GPIO bit-bang path is
    used even on hardware that supports RMT.  The RMT driver must therefore
    remain excluded so it doesn't consume compile time or flash.
    """
    generate_main(HERE / "test_gpio_one_wire_esp32_idf_use_rmt_false.yaml")

    from esphome.components.esp32 import KEY_ESP32, KEY_EXCLUDE_COMPONENTS
    from esphome.core import CORE

    excluded = CORE.data.get(KEY_ESP32, {}).get(KEY_EXCLUDE_COMPONENTS, set())
    assert "esp_driver_rmt" in excluded, (
        "esp_driver_rmt should remain excluded when use_rmt: false is set"
    )


def test_use_rmt_false_on_esp32_defines_no_macro(
    generate_main: Callable[[str | Path], str],
) -> None:
    """ESP32 IDF with use_rmt: false must not define USE_ONE_WIRE_RMT.

    The USE_ONE_WIRE_RMT macro is what switches gpio_one_wire_rmt.cpp from
    an empty file into the full RMT driver.  When use_rmt: false it must be
    absent so the GPIO bit-bang code path compiles instead.
    """
    main_cpp = generate_main(HERE / "test_gpio_one_wire_esp32_idf_use_rmt_false.yaml")

    assert "USE_ONE_WIRE_RMT" not in main_cpp, (
        "USE_ONE_WIRE_RMT must not be defined when use_rmt: false"
    )


def test_use_rmt_false_on_esp32_still_registers_bus(
    generate_main: Callable[[str | Path], str],
) -> None:
    """ESP32 IDF with use_rmt: false still generates a valid bus component."""
    main_cpp = generate_main(HERE / "test_gpio_one_wire_esp32_idf_use_rmt_false.yaml")

    assert "new(ow_bus) gpio::GPIOOneWireBus();" in main_cpp
    assert "App.register_component_(ow_bus" in main_cpp


def test_use_rmt_true_on_esp8266_raises_error(
    generate_main: Callable[[str | Path], str],
) -> None:
    """ESP8266 with use_rmt: true must raise a validation error.

    RMT hardware is only available on ESP32.  Requesting use_rmt: true on any
    other platform is a configuration mistake and must be caught at validation
    time rather than producing broken firmware.
    """
    import esphome.config_validation as cv

    with pytest.raises((cv.Invalid, Exception)):
        generate_main(HERE / "test_gpio_one_wire_esp8266_use_rmt_true.yaml")
