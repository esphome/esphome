"""Validate ATM90E32 thresholds before narrowing them to hardware registers."""

from typing import Any

import pytest

from esphome import config as esphome_config
import esphome.codegen as cg
from esphome.components.atm90e32 import sensor as atm90e32
import esphome.config_validation as cv
from esphome.const import KEY_CORE, KEY_TARGET_FRAMEWORK, KEY_TARGET_PLATFORM
from esphome.core import CORE
from esphome.types import ConfigType


def _validate(
    thresholds: ConfigType | None = None,
    phases: ConfigType | None = None,
    line_frequency: int = 60,
) -> ConfigType:
    config = {
        "line_frequency": cv.enum(atm90e32.LINE_FREQS, upper=True)(
            f"{line_frequency}Hz"
        ),
        "thresholds": atm90e32.THRESHOLDS_SCHEMA(thresholds or {}),
    }
    for phase, values in ({"phase_a": {}} if phases is None else phases).items():
        config[phase] = atm90e32.ATM90E32_PHASE_SCHEMA(values)
    return atm90e32._validate_thresholds(config)


@pytest.mark.parametrize("line_frequency, nominal", [(50, 220), (60, 120)])
def test_resolves_defaults_without_threshold_block(
    line_frequency: int, nominal: int
) -> None:
    thresholds = _validate(line_frequency=line_frequency)["thresholds"]
    assert thresholds["voltage_nominal"] == nominal
    assert thresholds["voltage_sag_v"] == pytest.approx(nominal * 0.78)
    assert thresholds["voltage_peak_v"] == pytest.approx(nominal * 1.22)
    assert "current_peak" not in thresholds
    assert thresholds["frequency_low_hz"] == line_frequency - 3
    assert thresholds["frequency_high_hz"] == line_frequency + 3


@pytest.mark.parametrize("key", ["frequency_low_hz", "frequency_high_hz"])
def test_rejects_frequency_register_overflow(key: str) -> None:
    with pytest.raises(cv.Invalid):
        _validate({key: "700Hz"})


def test_accepts_frequency_register_limit() -> None:
    thresholds = _validate({"frequency_high_hz": "655.35Hz"})["thresholds"]
    assert thresholds["frequency_high_hz"] == 655.35


@pytest.mark.parametrize(
    "low, high",
    [
        ("59.996Hz", "60.004Hz"),
        ("62Hz", "58Hz"),
    ],
)
def test_rejects_inverted_or_collapsed_frequency_band(low: str, high: str) -> None:
    with pytest.raises(cv.Invalid, match="frequency_low_hz must be lower"):
        _validate({"frequency_low_hz": low, "frequency_high_hz": high})


@pytest.mark.parametrize(
    "thresholds",
    [
        {"voltage_sag_v": "1V"},
        {"voltage_peak_v": "200V"},
        {"voltage_sag_pct": "1%"},
        {"voltage_peak_pct": "180%"},
    ],
)
def test_absolute_and_percent_voltages_require_safety_override(
    thresholds: ConfigType,
) -> None:
    with pytest.raises(cv.Invalid, match="allow_unsafe_thresholds"):
        _validate(thresholds)
    thresholds["allow_unsafe_thresholds"] = True
    _validate(thresholds, {"phase_a": {"gain_voltage": 20000}})


@pytest.mark.parametrize(
    "thresholds, phases",
    [
        ({"voltage_nominal": "240V"}, {"phase_a": {}}),
        ({"current_peak": "50A"}, {"phase_a": {"gain_ct": 10000}}),
        ({"voltage_peak_v": "150V"}, {"phase_b": {"gain_voltage": 0}}),
        ({"current_peak": "10A"}, {"phase_c": {"gain_ct": 0}}),
        (
            {"current_peak": "10A"},
            {"phase_a": {"gain_ct": 0}, "phase_b": {}},
        ),
        (
            {"voltage_nominal": "120V"},
            {"phase_a": {"gain_voltage": 0}, "phase_b": {}},
        ),
        ({"current_peak": "10A"}, {}),
        ({"voltage_nominal": "120V"}, {}),
    ],
)
def test_rejects_thresholds_unrepresentable_at_configured_gain(
    thresholds: ConfigType, phases: ConfigType
) -> None:
    with pytest.raises(cv.Invalid, match="gain"):
        _validate(thresholds, phases)


@pytest.mark.parametrize("phase", ["phase_b", "phase_c"])
def test_single_phase_threshold_does_not_require_phase_a(phase: str) -> None:
    thresholds = _validate({"current_peak": "10A"}, {phase: {}})["thresholds"]
    assert thresholds["current_peak"] == 10


@pytest.mark.parametrize(
    "thresholds, phases",
    [
        (
            {"current_peak": "10A"},
            {"phase_a": {"gain_ct": 15000}, "phase_b": {"gain_ct": 20000}},
        ),
        (
            {"voltage_nominal": "120V"},
            {"phase_a": {"gain_voltage": 15000}, "phase_b": {"gain_voltage": 20000}},
        ),
        (
            {"current_peak": "50A"},
            {"phase_a": {"gain_ct": 10000}, "phase_b": {"gain_ct": 27961}},
        ),
        (
            {"voltage_peak_v": "150V"},
            {"phase_a": {"gain_voltage": 4000}, "phase_b": {}},
        ),
    ],
)
def test_accepts_different_phase_gains_for_a_shared_threshold(
    thresholds: ConfigType, phases: ConfigType
) -> None:
    _validate(thresholds, phases)


@pytest.mark.parametrize(
    "thresholds, phases, key",
    [
        (
            {"voltage_sag_v": "96V"},
            {"phase_a": {"gain_voltage": 3000}, "phase_b": {}},
            "voltage_sag_v",
        ),
        (
            {"voltage_nominal": "200V", "voltage_sag_v": "100V"},
            {"phase_a": {"gain_voltage": 4000}, "phase_b": {}},
            "voltage_peak_v",
        ),
        (
            {"current_peak": "50A"},
            {"phase_a": {"gain_ct": 5000}, "phase_b": {"gain_ct": 10000}},
            "current_peak",
        ),
    ],
)
def test_rejects_overflow_at_the_gain_used_by_shared_register(
    thresholds: ConfigType, phases: ConfigType, key: str
) -> None:
    with pytest.raises(cv.Invalid, match=key):
        _validate(thresholds, phases)


@pytest.mark.parametrize(
    "key, value",
    [
        ("current_peak", float("nan")),
        ("voltage_nominal", float("inf")),
        ("frequency_high_hz", float("nan")),
    ],
)
def test_rejects_nonfinite_thresholds(key: str, value: Any) -> None:
    with pytest.raises(cv.Invalid):
        _validate({key: value})


def test_safety_override_does_not_allow_register_overflow() -> None:
    with pytest.raises(cv.Invalid, match="gain"):
        _validate({"voltage_peak_v": "1000V", "allow_unsafe_thresholds": True})


def test_default_thresholds_do_not_reject_legacy_phase_gains() -> None:
    _validate(phases={"phase_a": {"gain_ct": 10000}, "phase_b": {"gain_ct": 20000}})


@pytest.mark.asyncio
@pytest.mark.parametrize(
    "thresholds, current_peak, low, high",
    [
        (None, None, 5700, 6300),
        (
            {"frequency_low_hz": "59.95Hz", "frequency_high_hz": "60.01Hz"},
            None,
            5995,
            6001,
        ),
        ({"current_peak": "10A"}, 10.0, 5700, 6300),
        ({"current_peak": "65.53A"}, 65.53, 5700, 6300),
    ],
)
async def test_codegen_rounds_frequency_and_emits_configured_current(
    thresholds: ConfigType | None,
    current_peak: float | None,
    low: int,
    high: int,
) -> None:
    CORE.data[KEY_CORE] = {
        KEY_TARGET_PLATFORM: "esp32",
        KEY_TARGET_FRAMEWORK: "esp-idf",
    }
    CORE.data["esp32"] = {"variant": "ESP32", "board": "esp32dev"}
    esphome_config.path_context.set([])
    config = atm90e32.CONFIG_SCHEMA(
        {
            "id": "meter",
            "spi_id": "spi_bus",
            "cs_pin": 4,
            "line_frequency": "60Hz",
            "phase_b": {},
            **({"thresholds": thresholds} if thresholds is not None else {}),
        }
    )
    cg.new_Pvariable(config["spi_id"])
    CORE.component_ids.add("meter")

    await atm90e32.to_code(config)
    CORE.flush_tasks()

    source = CORE.cpp_main_section
    assert f"set_frequency_thresholds({low}, {high})" in source
    assert "set_line_freq(60)" in source
    if current_peak is None:
        assert "set_threshold_current_peak_a" not in source
    else:
        assert f"set_threshold_current_peak_a({current_peak}f)" in source
