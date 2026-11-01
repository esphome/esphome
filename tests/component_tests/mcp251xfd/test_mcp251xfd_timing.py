"""Tests for MCP251xFD nominal and data phase configuration."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.components import canbus
from esphome.components.mcp251xfd import canbus as mcp251xfd
import esphome.config_validation as cv
from esphome.const import PlatformFramework

from ..types import SetCoreConfigCallable


def decode_timing(timing: int, data_phase: bool) -> tuple[int, int, int, int]:
    """Decode register fields using the datasheet's phase-specific masks."""
    segment1_mask, segment2_mask = (0x1F, 0x0F) if data_phase else (0xFF, 0x7F)
    assert (
        timing
        & ~(0xFF000000 | segment1_mask << 16 | segment2_mask << 8 | segment2_mask)
        == 0
    )
    return (
        (timing >> 24) + 1,
        ((timing >> 16) & segment1_mask) + 1,
        ((timing >> 8) & segment2_mask) + 1,
        (timing & segment2_mask) + 1,
    )


@pytest.fixture
def configure(set_core_config: SetCoreConfigCallable) -> Callable[..., dict]:
    """Validate MCP251xFD configuration with ESP32 GPIO support."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={"variant": "ESP32", "board": "esp32dev"},
    )

    def validate(**options: str) -> dict:
        return mcp251xfd.CONFIG_SCHEMA({"can_id": 1, "cs_pin": 21, **options})

    return validate


@pytest.mark.parametrize(
    ("mode", "expected"),
    [
        ("NORMAL", "CAN_MODE_NORMAL"),
        ("CLASSIC", "CAN_MODE_CLASSIC"),
        ("classic", "CAN_MODE_CLASSIC"),
        ("LOOPBACK", "CAN_MODE_LOOPBACK"),
        ("LISTENONLY", "CAN_MODE_LISTEN_ONLY"),
    ],
)
def test_mode_configuration(
    configure: Callable[..., dict], mode: str, expected: str
) -> None:
    """Configured modes select their corresponding controller enum values."""
    config = configure(mode=mode)
    assert str(config["mode"].enum_value) == str(getattr(mcp251xfd.CanMode, expected))


def test_default_mode(configure: Callable[..., dict]) -> None:
    """Normal mixed classic/FD operation remains the default."""
    config = configure()
    assert str(config["mode"].enum_value) == str(mcp251xfd.CanMode.CAN_MODE_NORMAL)


def test_invalid_mode(configure: Callable[..., dict]) -> None:
    """Unexposed hardware modes remain invalid configuration choices."""
    with pytest.raises(cv.Invalid, match="mode"):
        configure(mode="CONFIGURATION")


@pytest.mark.parametrize("clock", ["4MHZ", "20MHZ", "40MHZ"])
def test_default_spi_data_rate(configure: Callable[..., dict], clock: str) -> None:
    """The conservative SPI default remains valid with every oscillator."""
    assert configure(clock=clock)["data_rate"] == 8_000_000


@pytest.mark.parametrize("clock", ["4MHZ", "40MHZ"])
def test_faster_spi_data_rate(configure: Callable[..., dict], clock: str) -> None:
    """Both direct and PLL-derived 40 MHz clocks allow 16 MHz SPI."""
    assert configure(clock=clock, data_rate="16MHz")["data_rate"] == 16_000_000


@pytest.mark.parametrize(
    ("clock", "data_rate"),
    [("20MHZ", "10MHz"), ("4MHZ", "20MHz"), ("40MHZ", "20MHz")],
)
def test_excessive_spi_data_rate(
    configure: Callable[..., dict], clock: str, data_rate: str
) -> None:
    """SPI rates above the controller clock-dependent limit are rejected."""
    with pytest.raises(cv.Invalid, match="data_rate"):
        configure(clock=clock, data_rate=data_rate)


@pytest.mark.parametrize(
    ("clock", "maximum"),
    [("20MHZ", 8_500_000), ("4MHZ", 17_000_000), ("40MHZ", 17_000_000)],
)
def test_spi_data_rate_boundary(clock: str, maximum: int) -> None:
    """Check the datasheet limit independently of host SPI clock divisors."""
    config = {"clock": clock, "data_rate": maximum}
    assert mcp251xfd.validate_data_rate(config) == config
    with pytest.raises(cv.Invalid, match="data_rate"):
        mcp251xfd.validate_data_rate({**config, "data_rate": maximum + 1})


@pytest.mark.parametrize("clock", ["4MHZ", "20MHZ", "40MHZ"])
@pytest.mark.parametrize(
    "rate", [rate for rate in canbus.CAN_SPEEDS if rate != "1KBPS"]
)
def test_default_data_rate(
    configure: Callable[..., dict], clock: str, rate: str
) -> None:
    """Explicit equal phase rates support low rates and PLL clocks."""
    config = configure(clock=clock, bit_rate=rate, data_bit_rate=rate)
    assert config["data_bit_rate"] == rate
    nominal, data, tdc = mcp251xfd.bit_timings(clock, rate, rate)
    system_clock = 20_000_000 if clock == "20MHZ" else 40_000_000
    expected = {"33KBPS": 100_000 / 3, "83K3BPS": 250_000 / 3}.get(
        rate, canbus.get_rate(rate)
    )
    assert nominal >> 24 == data >> 24
    for timing, data_phase in ((nominal, False), (data, True)):
        prescaler, segment1, segment2, sjw = decode_timing(timing, data_phase)
        actual = system_clock / (prescaler * (1 + segment1 + segment2))
        assert abs(actual / expected - 1) <= 0.005
        assert sjw <= min(segment1, segment2)
    if expected >= 1_000_000:
        prescaler, segment1, _, _ = decode_timing(data, True)
        assert tdc == (2 << 16) | (prescaler * (1 + segment1) << 8)
    else:
        assert tdc == 0


@pytest.mark.parametrize("clock", ["4MHZ", "20MHZ", "40MHZ"])
def test_unachievable_low_data_rate(configure: Callable[..., dict], clock: str) -> None:
    """Even BRP=256 and 49 data quanta cannot achieve 1 kbps."""
    with pytest.raises(cv.Invalid, match="data_bit_rate"):
        configure(clock=clock, bit_rate="1KBPS", data_bit_rate="1KBPS")


def test_data_rate_validation(configure: Callable[..., dict]) -> None:
    """Data rates use the CAN FD table and cannot be slower than nominal."""
    with pytest.raises(cv.Invalid, match="data_bit_rate.*greater than or equal"):
        configure(bit_rate="500KBPS", data_bit_rate="250KBPS")
    with pytest.raises(cv.Invalid, match="data_bit_rate"):
        configure(data_bit_rate="9000KBPS")
    assert configure(data_bit_rate="2000kbps")["data_bit_rate"] == "2000KBPS"


@pytest.mark.parametrize("clock", ["4MHZ", "40MHZ"])
def test_eight_megabit_data_rate(configure: Callable[..., dict], clock: str) -> None:
    """8 Mbps uses five data quanta with a 40 MHz system clock."""
    configure(clock=clock, bit_rate="500KBPS", data_bit_rate="8000KBPS")
    nominal, data, tdc = mcp251xfd.bit_timings(clock, "500KBPS", "8000KBPS")
    assert decode_timing(data, True) == (1, 3, 1, 1)
    assert nominal >> 24 == 0
    assert tdc == 0x20400


def test_eight_megabit_rejected_at_twenty_mhz(configure: Callable[..., dict]) -> None:
    """20 MHz cannot achieve 8 Mbps within the allowed error."""
    with pytest.raises(cv.Invalid, match="data_bit_rate"):
        configure(clock="20MHZ", data_bit_rate="8000KBPS")


def test_no_common_prescaler(configure: Callable[..., dict]) -> None:
    """Individually valid rates may not share a valid prescaler."""
    with pytest.raises(cv.Invalid, match="prescaler"):
        configure(bit_rate="5KBPS", data_bit_rate="8000KBPS")


@pytest.mark.parametrize("clock", ["4MHZ", "20MHZ", "40MHZ"])
def test_low_nominal_with_faster_data(
    configure: Callable[..., dict], clock: str
) -> None:
    """A nominal 1 kbps remains usable when the data phase fits its register."""
    config = configure(clock=clock, bit_rate="1KBPS", data_bit_rate="5KBPS")
    assert config["data_bit_rate"] == "5KBPS"
    nominal, data, _ = mcp251xfd.bit_timings(clock, "1KBPS", "5KBPS")
    assert nominal >> 24 == data >> 24
    system_clock = 20_000_000 if clock == "20MHZ" else 40_000_000
    for timing, data_phase, expected in ((nominal, False, 1000), (data, True, 5000)):
        prescaler, segment1, segment2, _ = decode_timing(timing, data_phase)
        actual = system_clock / (prescaler * (1 + segment1 + segment2))
        assert abs(actual / expected - 1) <= 0.005


def test_codegen(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Both phase registers and encoded automatic TDC reach the constructor."""
    main_cpp = generate_main(component_config_path("mcp251xfd_timing.yaml"))
    assert (
        "mcp251xfd::MCP251XFD(mcp251xfd::CanClock::CAN_CLOCK_40MHZ, "
        "mcp251xfd::CanMode::CAN_MODE_NORMAL, 0x3E0F0F, 0x20000, 0x20400)"
    ) in main_cpp
    assert "->set_default_fd(true);" in main_cpp
    assert "->set_default_bit_rate_switch(true);" not in main_cpp
