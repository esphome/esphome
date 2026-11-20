"""Tests for the thermostat config validation."""

from collections.abc import Callable
from pathlib import Path
import re
import textwrap

import pytest

from esphome.config import read_config
from esphome.core import CORE


def test_dry_only_config_is_valid(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """A dehumidifier has dry mode but no cool_action; dry mode uses the cooling timers."""
    main_cpp = generate_main(component_config_path("dry_only.yaml"))

    assert "dehumidifier->set_cooling_minimum_off_time_in_sec(0);" in main_cpp
    assert "dehumidifier->set_cooling_minimum_run_time_in_sec(0);" in main_cpp
    assert "dehumidifier->set_supports_dry(true);" in main_cpp
    assert "dehumidifier->set_supports_cool(" not in main_cpp


def test_cooling_timer_needs_cool_or_dry_action(
    component_config_path: Callable[[str], Path],
    capsys: pytest.CaptureFixture[str],
) -> None:
    """A cooling timer with neither cool_action nor dry_action is still rejected."""
    CORE.config_path = component_config_path("cooling_timer_without_action.yaml")
    assert read_config({}) is None
    captured = capsys.readouterr()
    assert (
        "'min_cooling_off_time' requires 'cool_action' or 'dry_action'" in captured.out
    )


_BASE = """\
esphome:
  name: test

logger:

esp32:
  board: esp32dev

sensor:
  - platform: template
    id: temp_sensor
    lambda: return 21.0;

climate:
  - platform: thermostat
    id: thermo
    name: Thermo
    sensor: temp_sensor
    min_idle_time: 30s
    min_cooling_off_time: 300s
    min_cooling_run_time: 300s
    idle_action:
      - logger.log: idle
    cool_action:
      - logger.log: cool
"""


def _read_error(tmp_path: Path, extra: str, capsys: pytest.CaptureFixture[str]) -> str:
    """Validate the base cooling thermostat plus *extra* and return the error output."""
    path = tmp_path / "test.yaml"
    path.write_text(
        _BASE + textwrap.indent(textwrap.dedent(extra), "    "), encoding="utf-8"
    )
    CORE.config_path = path
    assert read_config({}) is None
    return re.sub(r"\x1b\[[0-9;]*m", "", capsys.readouterr().out)


@pytest.mark.parametrize(
    ("extra", "message"),
    [
        pytest.param(
            """\
            preset:
              - name: Away
                default_target_temperature_high: 24°C
                mode: HEAT
            """,
            "mode is set to HEAT for Away but heat_action is not present in the configuration",
            id="preset_mode",
        ),
        pytest.param(
            """\
            preset:
              - name: Away
                default_target_temperature_high: 24°C
                mode: FAN_ONLY
            """,
            "mode is set to FAN_ONLY for Away but fan_only_action is not present in the configuration",
            id="preset_mode_fan_only",
        ),
        pytest.param(
            """\
            preset:
              - name: Away
                default_target_temperature_high: 24°C
                fan_mode: LOW
            """,
            "fan_mode is set to LOW for Away but fan_mode_low_action is not present in the configuration",
            id="preset_fan_mode",
        ),
        pytest.param(
            """\
            fan_mode_low_action:
              - logger.log: low
            min_fan_mode_switching_time: 15s
            preset:
              - name: Away
                default_target_temperature_high: 24°C
                fan_mode: QUIET
            """,
            "fan_mode is set to QUIET for Away but fan_mode_quiet_action is not present in the configuration",
            id="preset_fan_mode_other_action_set",
        ),
        pytest.param(
            """\
            preset:
              - name: Away
                default_target_temperature_high: 24°C
                swing_mode: VERTICAL
            """,
            "swing_mode is set to VERTICAL for Away but swing_vertical_action is not present in the configuration",
            id="preset_swing_mode",
        ),
        pytest.param(
            """\
            min_fan_mode_switching_time: 15s
            """,
            "At least one of fan_mode_on_action, fan_mode_off_action, fan_mode_auto_action, "
            "fan_mode_low_action, fan_mode_medium_action, fan_mode_high_action, "
            "fan_mode_middle_action, fan_mode_focus_action, fan_mode_diffuse_action, "
            "fan_mode_quiet_action must be defined to use min_fan_mode_switching_time",
            id="fan_mode_switching_time_without_fan_action",
        ),
        pytest.param(
            """\
            fan_mode_diffuse_action:
              - logger.log: diffuse
            """,
            "'fan_mode_diffuse_action' requires 'min_fan_mode_switching_time'",
            id="fan_action_without_switching_time",
        ),
        pytest.param(
            """\
            fan_with_cooling: true
            """,
            "fan_only_action must be defined to use fan_with_cooling",
            id="fan_with_cooling",
        ),
        pytest.param(
            """\
            fan_with_heating: true
            """,
            "fan_only_action must be defined to use fan_with_heating",
            id="fan_with_heating",
        ),
        pytest.param(
            """\
            fan_only_action:
              - logger.log: fan
            min_fanning_off_time: 30s
            """,
            "min_fanning_run_time must be defined to use fan_only_action",
            id="fan_only_action_timers",
        ),
        pytest.param(
            """\
            default_target_temperature_low: 18°C
            """,
            "default_target_temperature_low is no longer valid.",
            id="legacy_low",
        ),
        pytest.param(
            """\
            default_target_temperature_high: 24°C
            """,
            "default_target_temperature_high is no longer valid.",
            id="legacy_high",
        ),
        pytest.param(
            """\
            heat_cool_mode: false
            """,
            "heat_cool_mode cannot be 'false'",
            id="heat_cool_mode_false",
        ),
        pytest.param(
            """\
            humidity_sensor: temp_sensor
            humidity_control_off_action:
              - logger.log: humidity off
            humidity_control_humidify_action:
              - logger.log: humidify
            humidity_control_dehumidify_action:
              - logger.log: dehumidify
            """,
            "two or more values in the same group of exclusion 'humidity_control'",
            id="humidity_control_exclusive",
        ),
        pytest.param(
            """\
            preset:
              - name: Away
                default_target_temperature_high: 40°C
            """,
            "default_target_temperature_high for Away is set to 40.0 which is more than "
            "the visual maximum temperature of 30.0",
            id="preset_above_visual_max",
        ),
        pytest.param(
            """\
            heat_action:
              - logger.log: heat
            min_heating_off_time: 300s
            min_heating_run_time: 300s
            preset:
              - name: Away
                default_target_temperature_high: 24°C
                default_target_temperature_low: 5°C
            """,
            "default_target_temperature_low for Away is set to 5.0 which is less than "
            "the visual minimum temperature of 10.0",
            id="preset_below_visual_min",
        ),
        pytest.param(
            """\
            fan_only_action:
              - logger.log: fan
            fan_only_action_uses_fan_mode_timer: true
            """,
            "min_fan_mode_switching_time must be defined to use fan_only_action",
            id="fan_only_action_uses_fan_mode_timer",
        ),
        pytest.param(
            """\
            on_boot_restore_from: default_preset
            """,
            "default_preset must be defined to use on_boot_restore_from in DEFAULT_PRESET mode",
            id="restore_default_preset_without_default",
        ),
        pytest.param(
            """\
            away_config:
              default_target_temperature_high: 26°C
            """,
            "away_config is no longer valid. Please switch to using a preset named Away",
            id="legacy_away_config",
        ),
        pytest.param(
            """\
            default_mode: cool
            """,
            "default_mode is no longer valid. Please switch to using presets and specify a default_preset",
            id="legacy_default_mode",
        ),
        pytest.param(
            """\
            default_preset: Home
            preset:
              - name: Away
                default_target_temperature_high: 24°C
            """,
            "default_preset set to 'Home' but no such preset has been defined. "
            "Available presets: ['Away']",
            id="default_preset_missing",
        ),
    ],
)
def test_invalid_configs(
    tmp_path: Path, capsys: pytest.CaptureFixture[str], extra: str, message: str
) -> None:
    """Each rule rejects the config with its own message."""
    assert message in _read_error(tmp_path, extra, capsys)


def test_legacy_low_suggests_preset(
    tmp_path: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    """The legacy target temperatures error prints an equivalent preset."""
    out = _read_error(
        tmp_path,
        """\
        default_target_temperature_low: 18°C
        default_target_temperature_high: 24°C
        """,
        capsys,
    )
    assert (
        "  preset:\n"
        "    -  name: Your new preset\n"
        "       default_target_temperature_low: 18.0\n"
        "       default_target_temperature_high: 24.0."
    ) in out


def _assert_thermo_calls(main_cpp: str, expected: list[str]) -> None:
    """The thermostat's calls are exactly *expected*, in order, with nothing extra."""
    calls = [line.strip() for line in main_cpp.splitlines() if "thermo->" in line]
    assert len(calls) == len(expected), calls
    for call, line in zip(calls, expected, strict=True):
        assert call.endswith(line), f"{call!r} is not {line!r}"


def test_all_triggers_codegen(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Every trigger, supports flag and setting is generated in a fixed order."""
    main_cpp = generate_main(component_config_path("all_triggers.yaml"))

    expected = [
        "thermo->set_supports_two_points(true);",
        "thermo->set_set_point_minimum_differential(0.5f);",
        "thermo->set_sensor(temp_sensor);",
        "thermo->set_humidity_sensor(temp_sensor);",
        "thermo->set_cool_deadband(0.6f);",
        "thermo->set_cool_overrun(0.7f);",
        "thermo->set_heat_deadband(0.8f);",
        "thermo->set_heat_overrun(0.9f);",
        "thermo->set_cooling_maximum_run_time_in_sec(601);",
        "thermo->set_heating_maximum_run_time_in_sec(602);",
        "thermo->set_cooling_minimum_off_time_in_sec(301);",
        "thermo->set_cooling_minimum_run_time_in_sec(302);",
        "thermo->set_fan_mode_minimum_switching_time_in_sec(15);",
        "thermo->set_fanning_minimum_off_time_in_sec(31);",
        "thermo->set_fanning_minimum_run_time_in_sec(32);",
        "thermo->set_heating_minimum_off_time_in_sec(303);",
        "thermo->set_heating_minimum_run_time_in_sec(304);",
        "thermo->set_supplemental_cool_delta(2.0f);",
        "thermo->set_supplemental_heat_delta(3.0f);",
        "thermo->set_idle_minimum_time_in_sec(30);",
        "thermo->set_supports_fan_only_action_uses_fan_mode_timer(true);",
        "thermo->set_supports_fan_only_cooling(false);",
        "thermo->set_supports_fan_with_cooling(true);",
        "thermo->set_supports_fan_with_heating(false);",
        "thermo->set_use_startup_delay(true);",
        "(thermo->get_idle_action_trigger());",
        "(thermo->get_cool_action_trigger());",
        "thermo->set_supports_cool(true);",
        "(thermo->get_supplemental_cool_action_trigger());",
        "(thermo->get_dry_action_trigger());",
        "thermo->set_supports_dry(true);",
        "(thermo->get_fan_only_action_trigger());",
        "thermo->set_supports_fan_only(true);",
        "(thermo->get_heat_action_trigger());",
        "thermo->set_supports_heat(true);",
        "(thermo->get_supplemental_heat_action_trigger());",
        "(thermo->get_auto_mode_trigger());",
        "thermo->set_supports_auto(true);",
        "(thermo->get_cool_mode_trigger());",
        "thermo->set_supports_cool(true);",
        "(thermo->get_dry_mode_trigger());",
        "thermo->set_supports_dry(true);",
        "(thermo->get_fan_only_mode_trigger());",
        "thermo->set_supports_fan_only(true);",
        "(thermo->get_heat_mode_trigger());",
        "thermo->set_supports_heat(true);",
        "(thermo->get_heat_cool_mode_trigger());",
        "thermo->set_supports_heat_cool(true);",
        "(thermo->get_off_mode_trigger());",
    ]
    for mode in (
        "on",
        "off",
        "auto",
        "low",
        "medium",
        "high",
        "middle",
        "focus",
        "diffuse",
        "quiet",
    ):
        expected += [
            f"(thermo->get_fan_mode_{mode}_trigger());",
            f"thermo->set_supports_fan_mode_{mode}(true);",
        ]
    for mode in ("both", "horizontal", "off", "vertical"):
        expected += [
            f"(thermo->get_swing_mode_{mode}_trigger());",
            f"thermo->set_supports_swing_mode_{mode}(true);",
        ]
    expected += [
        "(thermo->get_humidity_change_trigger());",
        "(thermo->get_temperature_change_trigger());",
        "thermo->set_supports_humidification(true);",
        "(thermo->get_humidity_control_humidify_action_trigger());",
        "(thermo->get_humidity_control_off_action_trigger());",
        "thermo->set_humidity_hysteresis(0.5f);",
        "(thermo->get_preset_change_trigger());",
    ]
    _assert_thermo_calls(main_cpp, expected)


def test_heat_cool_mode_true_sets_support_only(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """heat_cool_mode: true enables the mode without building an automation."""
    main_cpp = generate_main(component_config_path("heat_cool_true.yaml"))

    assert "thermo->set_supports_heat_cool(true);" in main_cpp
    assert "get_heat_cool_mode_trigger" not in main_cpp


def test_dehumidify_codegen(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Dehumidification sets its supports flag before building its automation."""
    main_cpp = generate_main(component_config_path("dehumidify.yaml"))

    _assert_thermo_calls(
        main_cpp,
        [
            "thermo->set_set_point_minimum_differential(0.5f);",
            "thermo->set_sensor(temp_sensor);",
            "thermo->set_humidity_sensor(temp_sensor);",
            "thermo->set_cool_deadband(0.5f);",
            "thermo->set_cool_overrun(0.5f);",
            "thermo->set_heat_deadband(0.5f);",
            "thermo->set_heat_overrun(0.5f);",
            "thermo->set_cooling_minimum_off_time_in_sec(300);",
            "thermo->set_cooling_minimum_run_time_in_sec(300);",
            "thermo->set_idle_minimum_time_in_sec(30);",
            "thermo->set_supports_fan_only_action_uses_fan_mode_timer(false);",
            "thermo->set_supports_fan_only_cooling(false);",
            "thermo->set_supports_fan_with_cooling(false);",
            "thermo->set_supports_fan_with_heating(false);",
            "thermo->set_use_startup_delay(false);",
            "(thermo->get_idle_action_trigger());",
            "(thermo->get_cool_action_trigger());",
            "thermo->set_supports_cool(true);",
            "thermo->set_supports_dehumidification(true);",
            "(thermo->get_humidity_control_dehumidify_action_trigger());",
            "(thermo->get_humidity_control_off_action_trigger());",
            "thermo->set_humidity_hysteresis(1.0f);",
        ],
    )
