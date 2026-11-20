"""Tests for the use_rmt option of the gpio 1-Wire bus, through the whole config pipeline."""

from collections.abc import Callable
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
    assert "USE_ONE_WIRE_RMT" not in {define.name for define in CORE.defines}


@pytest.mark.parametrize("platform", [ESP32_S3_IDF, ESP32_S3_ARDUINO])
def test_use_rmt_selects_the_rmt_bus(
    generate_main: Callable[[str | Path], str],
    write_yaml: Callable[[str], Path],
    platform: str,
) -> None:
    main_cpp = generate_main(write_yaml(_yaml(platform, USE_RMT)))

    assert "gpio::GPIORMTOneWireBus" in main_cpp
    assert "USE_ONE_WIRE_RMT" in {define.name for define in CORE.defines}
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
    assert "version" in _error_text(validate(_yaml(platform, USE_RMT)))


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
