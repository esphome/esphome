"""Tests for the counter sensor."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome import config_validation as cv
from esphome.components.counter.sensor import CONFIG_SCHEMA, COUNTER_VALUE

INT64_MAX = 2**63 - 1


def test_counter_constructor_arguments(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Restore (on by default) and initial value (zero by default) are constructor arguments."""
    main_cpp = generate_main(component_config_path("counter_test.yaml"))

    assert "new(counter_a) counter::CounterSensor(true, 0);" in main_cpp
    assert "new(counter_b) counter::CounterSensor(false, -5000000000LL);" in main_cpp


def test_counter_sensor_option_registers_source(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Only the counter with a sensor option counts that sensor's updates."""
    main_cpp = generate_main(component_config_path("counter_test.yaml"))

    assert "counter_a->count_updates_from(source_sensor);" in main_cpp
    assert main_cpp.count("count_updates_from") == 1


def test_counter_binary_sensor_option_registers_source(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Only the counter with a binary_sensor option counts that sensor's changes to true."""
    main_cpp = generate_main(component_config_path("counter_test.yaml"))

    assert "counter_c->count_true_from(source_binary_sensor);" in main_cpp
    assert main_cpp.count("count_true_from") == 1


def test_counter_actions(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Values reach the counter as 64-bit integers; increment defaults to one."""
    main_cpp = generate_main(component_config_path("counter_test.yaml"))

    assert "::counter_a->set_value(100);" in main_cpp
    assert "::counter_b->set_value(-5000000000LL);" in main_cpp
    assert "::counter_a->increment(1);" in main_cpp
    assert "::counter_a->increment(-7);" in main_cpp
    assert "::counter_b->increment(static_cast<int64_t>(3));" in main_cpp


def test_counter_actions_without_id_use_only_counter(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """With one counter configured, the id may be left out of an action."""
    main_cpp = generate_main(component_config_path("counter_single.yaml"))

    assert "::only_counter->set_value(5);" in main_cpp
    assert "::only_counter->set_value(7);" in main_cpp
    assert "::only_counter->increment(-3);" in main_cpp


@pytest.mark.parametrize("value", [INT64_MAX + 1, -INT64_MAX - 1, 1.5])
def test_counter_initial_value_must_be_int64(value: float) -> None:
    with pytest.raises(cv.Invalid):
        CONFIG_SCHEMA({"id": "c1", "name": "C1", "initial_value": value})


def test_counter_cannot_count_itself() -> None:
    """A counter watching its own updates would recurse forever."""
    with pytest.raises(cv.Invalid, match="cannot count its own updates"):
        CONFIG_SCHEMA({"id": "c1", "name": "C1", "sensor": "c1"})


def test_counter_accepts_other_source() -> None:
    config = CONFIG_SCHEMA({"id": "c1", "name": "C1", "sensor": "other"})

    assert config["sensor"].id == "other"


@pytest.mark.parametrize("value", [0, 1, -1, INT64_MAX, -INT64_MAX])
def test_counter_value_accepts_int64_range(value: int) -> None:
    assert COUNTER_VALUE(value) == value


@pytest.mark.parametrize("value", [INT64_MAX + 1, -INT64_MAX - 1, 1.5])
def test_counter_value_rejects_out_of_range(value: float) -> None:
    with pytest.raises(cv.Invalid):
        COUNTER_VALUE(value)
