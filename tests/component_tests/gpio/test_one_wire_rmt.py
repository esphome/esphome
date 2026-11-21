"""Tests for the use_rmt option of the gpio 1-Wire bus, through the whole config pipeline."""

from collections.abc import Callable
import logging
from pathlib import Path
import re

import pytest

from esphome import yaml_util
from esphome.components.esp32 import get_excluded_builtin_components
from esphome.components.gpio import one_wire as gpio_one_wire
from esphome.components.gpio.one_wire import (
    RMT_TIMING,
    GPIOOneWireBus,
    GPIORMTOneWireBus,
)
from esphome.config import Config, validate_config
from esphome.core import CORE

ESP32_S3_IDF = "esp32:\n  variant: esp32s3\n  framework:\n    type: esp-idf\n"
ESP32_S3_ARDUINO = "esp32:\n  variant: esp32s3\n  framework:\n    type: arduino\n"
ESP32_C2 = "esp32:\n  variant: esp32c2\n  framework:\n    type: esp-idf\n"
ESP32_C61 = "esp32:\n  variant: esp32c61\n  framework:\n    type: esp-idf\n"

DALLAS = """
sensor:
  - platform: dallas_temp
    one_wire_id: ow
    address: 0x120000065C17EE28
    name: Temperature
"""


def _yaml(platform: str, bus: str, pin: str = "5", tail: str = "") -> str:
    return (
        f"esphome:\n  name: test\n{platform}\none_wire:\n  - platform: gpio\n"
        f"    id: ow\n    pin: {pin}\n{bus}{tail}"
    )


USE_RMT = "    use_rmt: true\n"


@pytest.fixture
def write_yaml(tmp_path: Path) -> Callable[[str], Path]:
    def writer(content: str) -> Path:
        path = tmp_path / "test.yaml"
        path.write_text(content)
        return path

    return writer


@pytest.fixture
def validate(write_yaml: Callable[[str], Path]) -> Callable[[str], Config]:
    def validator(content: str) -> Config:
        path = write_yaml(content)
        CORE.config_path = path
        return validate_config(yaml_util.load_yaml(path), {})

    return validator


def _error_text(result: Config) -> str:
    return " | ".join(str(error) for error in result.errors)


@pytest.mark.parametrize("bus", ["", "    use_rmt: false\n"])
@pytest.mark.parametrize("platform", [ESP32_S3_IDF, ESP32_C2, ESP32_C61])
def test_without_use_rmt_stays_bit_bang(
    generate_main: Callable[[str | Path], str],
    write_yaml: Callable[[str], Path],
    platform: str,
    bus: str,
) -> None:
    """Also on variants without RMT: the validator looks for the key, so it must not see the default."""
    main_cpp = generate_main(write_yaml(_yaml(platform, bus)))

    assert "gpio::GPIOOneWireBus" in main_cpp
    assert "GPIORMTOneWireBus" not in main_cpp
    defines = {define.name for define in CORE.defines}
    assert "USE_ONE_WIRE_RMT" not in defines
    assert "USE_ONE_WIRE_TRANSACTION" not in defines


@pytest.mark.parametrize("platform", [ESP32_S3_IDF, ESP32_S3_ARDUINO])
def test_use_rmt_selects_the_rmt_bus(
    generate_main: Callable[[str | Path], str],
    write_yaml: Callable[[str], Path],
    platform: str,
) -> None:
    main_cpp = generate_main(write_yaml(_yaml(platform, USE_RMT)))

    assert "gpio::GPIORMTOneWireBus" in main_cpp
    defines = {define.name for define in CORE.defines}
    assert "USE_ONE_WIRE_RMT" in defines
    assert "USE_ONE_WIRE_TRANSACTION" in defines
    assert "esp_driver_rmt" not in get_excluded_builtin_components()


@pytest.mark.parametrize(
    ("platform", "pin"),
    [
        ("esp8266:\n  board: d1_mini\n", "5"),
        ("rp2040:\n  board: rpipicow\n", "5"),
        ("nrf52:\n  board: adafruit_itsybitsy_nrf52840\n", "5"),
        ("bk72xx:\n  board: generic-bk7231n-qfn32-tuya\n", "6"),
        ("host:\n", "5"),
    ],
    ids=["esp8266", "rp2", "nrf52", "libretiny", "host"],
)
def test_use_rmt_is_invalid_outside_esp32(
    validate: Callable[[str], Config], platform: str, pin: str
) -> None:
    assert "only available on ESP32" in _error_text(
        validate(_yaml(platform, USE_RMT, pin))
    )


@pytest.mark.parametrize("platform", [ESP32_C2, ESP32_C61])
def test_use_rmt_is_invalid_without_rmt_hardware(
    validate: Callable[[str], Config], platform: str
) -> None:
    assert "no RMT hardware" in _error_text(validate(_yaml(platform, USE_RMT)))


def test_use_rmt_rejects_an_inverted_pin(validate: Callable[[str], Config]) -> None:
    pin = "\n      number: 5\n      inverted: true"
    assert "inverted" in _error_text(validate(_yaml(ESP32_S3_IDF, USE_RMT, pin)))


@pytest.mark.parametrize(
    ("framework", "version"), [("esp-idf", "5.4.2"), ("arduino", "3.2.1")]
)
def test_use_rmt_needs_idf_5_5(
    validate: Callable[[str], Config], framework: str, version: str
) -> None:
    platform = (
        "esp32:\n  variant: esp32s3\n  framework:\n"
        f"    type: {framework}\n    version: {version}\n"
    )
    # Arduino 3.2.1 brings ESP-IDF 5.4.2 along
    assert "'use_rmt' requires ESP-IDF 5.5.0 or newer, got 5.4.2" in _error_text(
        validate(_yaml(platform, USE_RMT))
    )


def test_mixed_buses_in_one_device(validate: Callable[[str], Config]) -> None:
    tail = "  - platform: gpio\n    id: ow2\n    pin: 6\n"
    result = validate(_yaml(ESP32_S3_IDF, USE_RMT, tail=tail))

    assert not result.errors, _error_text(result)
    assert result["one_wire"][0]["id"].type is GPIORMTOneWireBus
    assert result["one_wire"][1]["id"].type is GPIOOneWireBus


def test_dallas_temp_resolves_the_rmt_bus(
    generate_main: Callable[[str | Path], str], write_yaml: Callable[[str], Path]
) -> None:
    main_cpp = generate_main(write_yaml(_yaml(ESP32_S3_IDF, USE_RMT, tail=DALLAS)))

    assert "gpio::GPIORMTOneWireBus" in main_cpp
    assert "set_one_wire_bus(ow)" in main_cpp


def _timing(**values: str) -> str:
    lines = "".join(f"      {key}: {value}\n" for key, value in values.items())
    return f"    rmt_timing:\n{lines}" if lines else "    rmt_timing: {}\n"


@pytest.mark.parametrize(
    "values",
    [
        {},
        {"write0_low": "60us"},
        {"write0_low": "105us", "slot": "120us"},
        {"write0_low": "60us", "slot": "71us"},
        {"slot": "120us"},
        {"read_threshold": "10us"},
        {"read_threshold": "25us"},
    ],
)
def test_rmt_timing_accepts_the_range(
    validate: Callable[[str], Config], values: dict[str, str]
) -> None:
    result = validate(_yaml(ESP32_S3_IDF, USE_RMT + _timing(**values)))
    assert not result.errors, _error_text(result)


@pytest.mark.parametrize(
    "values",
    [
        {"write0_low": "59us"},
        {"write0_low": "106us", "slot": "120us"},
        {"write0_low": "60us", "slot": "70us"},
        {"slot": "121us"},
        {"read_threshold": "9us"},
        {"read_threshold": "26us"},
    ],
)
def test_rmt_timing_rejects_values_out_of_range(
    validate: Callable[[str], Config], values: dict[str, str]
) -> None:
    assert validate(_yaml(ESP32_S3_IDF, USE_RMT + _timing(**values))).errors


@pytest.mark.parametrize(
    ("values", "valid"),
    [
        ({"write0_low": "69us", "slot": "80us"}, True),
        ({"write0_low": "70us", "slot": "80us"}, False),
        ({"write0_low": "99us"}, True),
        ({"write0_low": "100us"}, False),
    ],
)
def test_rmt_timing_keeps_recovery_after_a_written_0(
    validate: Callable[[str], Config], values: dict[str, str], valid: bool
) -> None:
    result = validate(_yaml(ESP32_S3_IDF, USE_RMT + _timing(**values)))
    assert not result.errors if valid else "+ 11us" in _error_text(result)


def test_rmt_timing_error_names_the_effective_values(
    validate: Callable[[str], Config],
) -> None:
    """Only write0_low is set here, but the default of slot is what makes it invalid."""
    result = validate(_yaml(ESP32_S3_IDF, USE_RMT + _timing(write0_low="100us")))

    assert "slot 110us needs at least write0_low 100us + 11us" in _error_text(result)
    assert result.errors[0].path[-1] == "slot"


def test_rmt_timing_defaults_match_the_codec() -> None:
    """Otherwise rmt_timing: {} and no rmt_timing at all would drive the bus differently."""
    codec = Path(gpio_one_wire.__file__).with_name("rmt_one_wire_codec.h").read_text()

    for option, (default, _, _) in RMT_TIMING.items():
        match = re.search(rf"uint8_t {option}_us\{{(\d+)\}};", codec)
        assert match, f"{option}_us is not a field of Timing"
        assert int(match.group(1)) == default, option


@pytest.mark.parametrize("bus", ["", "    use_rmt: false\n"])
def test_rmt_timing_needs_use_rmt(validate: Callable[[str], Config], bus: str) -> None:
    result = validate(_yaml(ESP32_S3_IDF, bus + _timing(slot="110us")))
    assert "rmt_timing needs use_rmt: true" in _error_text(result)


def test_rmt_timing_reaches_the_constructor(
    generate_main: Callable[[str | Path], str], write_yaml: Callable[[str], Path]
) -> None:
    bus = USE_RMT + _timing(write0_low="80us", slot="100us", read_threshold="15us")
    main_cpp = generate_main(write_yaml(_yaml(ESP32_S3_IDF, bus)))

    assert ".write0_low_us = 80" in main_cpp
    assert ".slot_us = 100" in main_cpp
    assert ".read_threshold_us = 15" in main_cpp


@pytest.mark.parametrize(
    "values",
    [
        {"read_threshold": "15us"},
        {"slot": "110us"},
        {"read_threshold": "15us", "slot": "100us", "write0_low": "80us"},
    ],
    ids=["read_threshold_alone", "slot_alone", "swapped_keys"],
)
def test_rmt_timing_fields_keep_the_declaration_order(
    generate_main: Callable[[str | Path], str],
    write_yaml: Callable[[str], Path],
    values: dict[str, str],
) -> None:
    """GCC rejects designated initializers out of declaration order."""
    main_cpp = generate_main(
        write_yaml(_yaml(ESP32_S3_IDF, USE_RMT + _timing(**values)))
    )

    write0_low = main_cpp.index(".write0_low_us")
    assert write0_low < main_cpp.index(".slot_us", write0_low)
    assert main_cpp.index(".slot_us", write0_low) < main_cpp.index(
        ".read_threshold_us", write0_low
    )


def test_without_rmt_timing_the_constructor_keeps_its_default(
    generate_main: Callable[[str | Path], str], write_yaml: Callable[[str], Path]
) -> None:
    main_cpp = generate_main(write_yaml(_yaml(ESP32_S3_IDF, USE_RMT)))
    assert "rmt_one_wire::Timing" not in main_cpp


def test_rmt_timing_default_read_threshold(
    generate_main: Callable[[str | Path], str], write_yaml: Callable[[str], Path]
) -> None:
    """A read 1 is Low for up to 4 us plus 9 us of rise time, so the default is 14 us."""
    main_cpp = generate_main(write_yaml(_yaml(ESP32_S3_IDF, USE_RMT + _timing())))
    assert ".read_threshold_us = 14" in main_cpp


@pytest.mark.parametrize(
    ("pin", "key"),
    [
        (
            "\n      number: 5\n      mode:\n        output: true\n        pullup: true",
            "pullup",
        ),
        (
            "\n      number: 5\n      mode:\n        input: true\n        pulldown: true",
            "pulldown",
        ),
        ("\n      number: 5\n      mode: INPUT_PULLUP", "pullup"),
        ("\n      number: 5\n      drive_strength: 40mA", "drive_strength"),
        ("\n      number: 5\n      hold_state: true", "hold_state"),
    ],
    ids=["pullup", "pulldown", "input_pullup", "drive_strength", "hold_state"],
)
def test_use_rmt_rejects_pin_options_it_does_not_apply(
    validate: Callable[[str], Config], pin: str, key: str
) -> None:
    result = validate(_yaml(ESP32_S3_IDF, USE_RMT, pin))
    assert result.errors
    assert (
        f"use_rmt does not {'support' if key == 'hold_state' else 'apply'} '{key}'"
        in (_error_text(result))
    )
    assert result.errors[0].path[-1] == key


@pytest.mark.parametrize(
    "pin",
    [
        "\n      number: 5\n      drive_strength: 20mA",
        "\n      number: 5\n      mode: INPUT_OUTPUT_OPEN_DRAIN",
    ],
    ids=["default_drive_strength", "open_drain"],
)
def test_use_rmt_accepts_pin_options_without_effect(
    validate: Callable[[str], Config], pin: str
) -> None:
    result = validate(_yaml(ESP32_S3_IDF, USE_RMT, pin))
    assert not result.errors, _error_text(result)


def test_bit_bang_keeps_all_pin_options(validate: Callable[[str], Config]) -> None:
    pin = "\n      number: 5\n      drive_strength: 40mA\n      mode: INPUT_PULLUP"
    result = validate(_yaml(ESP32_S3_IDF, "", pin))
    assert not result.errors, _error_text(result)


ESP32_IDF = "esp32:\n  variant: esp32\n  framework:\n    type: esp-idf\n"


@pytest.mark.parametrize(
    ("pin", "message"),
    [
        ("\n      number: 34\n      mode: INPUT", "use_rmt drives the pin"),
        ("34", "does not support output pin mode"),
        ("\n      number: 5\n      mode: INPUT", "use_rmt drives the pin"),
    ],
    ids=["input_only_pin_in_input_mode", "input_only_pin", "input_mode"],
)
def test_use_rmt_needs_an_output_pin(
    validate: Callable[[str], Config], pin: str, message: str
) -> None:
    """GPIO34 to GPIO39 of the ESP32 are input-only, the TX channel would fail in setup()."""
    result = validate(_yaml(ESP32_IDF, USE_RMT, pin))
    assert message in _error_text(result)


ESP32_C3 = "esp32:\n  variant: esp32c3\n  framework:\n    type: esp-idf\n"


def _buses(*use_rmt: bool) -> str:
    """More gpio buses after the first one, with or without use_rmt."""
    return "".join(
        f"  - platform: gpio\n    pin: {6 + i}\n" + (USE_RMT if rmt else "")
        for i, rmt in enumerate(use_rmt)
    )


def _rmt_warnings(caplog: pytest.LogCaptureFixture) -> list[str]:
    return [message for message in caplog.messages if "RMT memory" in message]


def test_use_rmt_buses_count_against_the_rmt_channels(
    validate: Callable[[str], Config],
) -> None:
    """Each bus takes one TX and one RX channel, and the C3 has two of each."""
    result = validate(_yaml(ESP32_C3, USE_RMT, tail=_buses(True, True)))
    text = _error_text(result)
    assert "ESP32C3 has 2 RMT channels for TX, but these 3 components" in text
    assert "ESP32C3 has 2 RMT channels for RX, but these 3 components" in text
    assert "'ow', 'one_wire[1]', 'one_wire[2]'" in text


def test_bit_bang_buses_take_no_rmt_channel(
    validate: Callable[[str], Config],
) -> None:
    result = validate(_yaml(ESP32_C3, USE_RMT, tail=_buses(True, False, False)))
    assert not result.errors, _error_text(result)


RECEIVER = "remote_receiver:\n  pin: 3\n"


@pytest.mark.parametrize(
    ("receiver", "warns"),
    [(RECEIVER, True), (RECEIVER + "  rmt_symbols: 48\n", False)],
    ids=["default_rmt_symbols", "one_block"],
)
def test_use_rmt_bus_shares_rx_memory_with_remote_receiver(
    generate_main: Callable[[str | Path], str],
    write_yaml: Callable[[str], Path],
    caplog: pytest.LogCaptureFixture,
    receiver: str,
    warns: bool,
) -> None:
    """The bus is set up first and takes one of the two C3 RX blocks."""
    with caplog.at_level(logging.WARNING):
        generate_main(write_yaml(_yaml(ESP32_C3, USE_RMT, tail=receiver)))
    messages = _rmt_warnings(caplog)
    if not warns:
        assert not messages
        return
    assert len(messages) == 1
    assert (
        "'remote_receiver[0]' will not fit: 'ow' (RX 48), 'ow' (TX 48), "
        "'remote_receiver[0]' (96)"
    ) in messages[0]
    assert "Set 'rmt_symbols: 48' on 'remote_receiver[0]'" in messages[0]
