"""on_time: the values each schedule key accepts and the code generated for them."""

from collections.abc import Callable
from pathlib import Path
from typing import Any

import pytest

from esphome.components.time import (
    CRON_KEYS,
    DAYS_OF_MONTH_FIELD,
    DAYS_OF_WEEK_FIELD,
    HOURS_FIELD,
    MINUTES_FIELD,
    MONTHS_FIELD,
    SECONDS_FIELD,
    TIME_SCHEMA,
    CronField,
    cron_field_check,
    on_time_check,
)
import esphome.config_validation as cv
from esphome.const import (
    CONF_AT,
    CONF_CRON,
    CONF_DAYS_OF_MONTH,
    CONF_DAYS_OF_WEEK,
    CONF_HOUR,
    CONF_HOURS,
    CONF_MINUTE,
    CONF_MINUTES,
    CONF_MONTHS,
    CONF_ON_TIME,
    CONF_SECOND,
    CONF_SECONDS,
    CONF_THEN,
    CONF_TIMEZONE,
    PlatformFramework,
)
from tests.component_tests.types import SetCoreConfigCallable

ALL_DAYS_OF_MONTH = list(range(1, 32))
ALL_MONTHS = list(range(1, 13))
ALL_DAYS_OF_WEEK = list(range(1, 8))


@pytest.mark.parametrize(
    ("entry", "expected"),
    [
        ({CONF_SECONDS: 0}, {CONF_SECONDS: [0]}),
        ({CONF_SECONDS: "/15"}, {CONF_SECONDS: [0, 15, 30, 45, 60]}),
        ({CONF_SECONDS: "0,10,20"}, {CONF_SECONDS: [0, 10, 20]}),
        ({CONF_SECONDS: [30, 10]}, {CONF_SECONDS: [10, 30]}),
        ({CONF_MINUTES: "/5"}, {CONF_MINUTES: list(range(0, 60, 5))}),
        ({CONF_MINUTES: "3/20"}, {CONF_MINUTES: [3, 23, 43]}),
        ({CONF_HOURS: "8-17"}, {CONF_HOURS: list(range(8, 18))}),
        ({CONF_HOURS: "*"}, {CONF_HOURS: list(range(24))}),
        ({CONF_DAYS_OF_MONTH: "*/7"}, {CONF_DAYS_OF_MONTH: [1, 8, 15, 22, 29]}),
        ({CONF_DAYS_OF_MONTH: "1,15"}, {CONF_DAYS_OF_MONTH: [1, 15]}),
        ({CONF_MONTHS: "JAN-MAR"}, {CONF_MONTHS: [1, 2, 3]}),
        ({CONF_MONTHS: "JAN,JUL"}, {CONF_MONTHS: [1, 7]}),
        ({CONF_DAYS_OF_WEEK: "MON-FRI"}, {CONF_DAYS_OF_WEEK: [2, 3, 4, 5, 6]}),
        ({CONF_DAYS_OF_WEEK: "SAT,SUN"}, {CONF_DAYS_OF_WEEK: [1, 7]}),
        (
            {CONF_CRON: "0 30 7 * * MON-FRI"},
            {
                CONF_SECONDS: [0],
                CONF_MINUTES: [30],
                CONF_HOURS: [7],
                CONF_DAYS_OF_MONTH: ALL_DAYS_OF_MONTH,
                CONF_MONTHS: ALL_MONTHS,
                CONF_DAYS_OF_WEEK: [2, 3, 4, 5, 6],
            },
        ),
        (
            {CONF_CRON: "*/10 0 12 1,15 JAN,JUL ?"},
            {
                CONF_SECONDS: [0, 10, 20, 30, 40, 50, 60],
                CONF_MINUTES: [0],
                CONF_HOURS: [12],
                CONF_DAYS_OF_MONTH: [1, 15],
                CONF_MONTHS: [1, 7],
                CONF_DAYS_OF_WEEK: ALL_DAYS_OF_WEEK,
            },
        ),
        (
            {CONF_AT: "07:30"},
            {
                CONF_SECONDS: [0],
                CONF_MINUTES: [30],
                CONF_HOURS: [7],
                CONF_DAYS_OF_MONTH: ALL_DAYS_OF_MONTH,
                CONF_MONTHS: ALL_MONTHS,
                CONF_DAYS_OF_WEEK: ALL_DAYS_OF_WEEK,
            },
        ),
        (
            {CONF_AT: "23:59:15"},
            {
                CONF_SECONDS: [15],
                CONF_MINUTES: [59],
                CONF_HOURS: [23],
                CONF_DAYS_OF_MONTH: ALL_DAYS_OF_MONTH,
                CONF_MONTHS: ALL_MONTHS,
                CONF_DAYS_OF_WEEK: ALL_DAYS_OF_WEEK,
            },
        ),
    ],
)
def test_on_time_schedule_values(
    entry: dict[str, Any],
    expected: dict[str, list[int]],
    set_core_config: SetCoreConfigCallable,
) -> None:
    """Each key validates to the sorted list of values it matches; unset keys stay unset."""
    set_core_config(PlatformFramework.ESP32_IDF)
    config = TIME_SCHEMA(
        {CONF_TIMEZONE: "UTC0", CONF_ON_TIME: [{**entry, CONF_THEN: []}]}
    )
    conf = config[CONF_ON_TIME][0]
    assert {key: conf.get(key) for key in CRON_KEYS} == {
        key: expected.get(key) for key in CRON_KEYS
    }


@pytest.mark.parametrize(
    ("entry", "message"),
    [
        ({CONF_CRON: "0 30 7 * *"}, "exactly 6 space-separated parts, not 5"),
        (
            {CONF_CRON: "0 30 7 * * MON", CONF_SECONDS: 0},
            "Cannot use option seconds when cron: is specified",
        ),
        (
            {CONF_AT: "07:30", CONF_HOURS: 7},
            "Cannot use option hours when at: is specified",
        ),
        (
            {CONF_AT: "07:30", CONF_CRON: "0 30 7 * * *"},
            "Cannot use option at with cron",
        ),
        ({CONF_SECONDS: 61}, r"seconds 61 is out of range \(min=0 max=60\)"),
        ({CONF_DAYS_OF_MONTH: 0}, r"days of month 0 is out of range \(min=1 max=31\)"),
        ({CONF_MONTHS: "FOO"}, "must be an integer, got FOO"),
        ({}, "Must contain at least one of seconds"),
    ],
)
def test_on_time_schedule_rejected(
    entry: dict[str, Any], message: str, set_core_config: SetCoreConfigCallable
) -> None:
    set_core_config(PlatformFramework.ESP32_IDF)
    with pytest.raises(cv.Invalid, match=message):
        TIME_SCHEMA({CONF_TIMEZONE: "UTC0", CONF_ON_TIME: [{**entry, CONF_THEN: []}]})


def _on_time_lines(automation: str, check: str) -> str:
    return "\n".join(
        [
            "sntp_time->add_on_time_callback([](const std::remove_cvref_t<ESPTime> & x) -> void {",
            f"    if (!({check}))",
            "      return;",
            f"    ::{automation}->trigger();",
        ]
    )


# One entry per on_time automation in config/on_time_fields.yaml, in order.
EXPECTED_ON_TIME_CODE = [
    _on_time_lines(
        "automation_id", "x.second == 0 && ((0x84210842108421ULL >> x.minute) & 1)"
    ),
    _on_time_lines(
        "automation_id_2",
        "x.second == 0 && x.minute == 30 && x.hour == 7"
        " && ((0x7cU >> x.day_of_week) & 1)",
    ),
    _on_time_lines("automation_id_3", "x.second == 0 && x.minute == 30 && x.hour == 7"),
    _on_time_lines(
        "automation_id_4",
        "x.second == 0 && x.minute == 0 && ((0x3ff00U >> x.hour) & 1)",
    ),
    _on_time_lines(
        "automation_id_5",
        "x.second == 0 && x.minute == 0 && x.hour == 12"
        " && ((0x8002U >> x.day_of_month) & 1) && ((0x82U >> x.month) & 1)",
    ),
    _on_time_lines(
        "automation_id_6",
        "x.second == 0 && x.minute == 0 && x.hour == 0"
        " && ((0x20408102U >> x.day_of_month) & 1)",
    ),
    _on_time_lines("automation_id_7", "((0x1000200040008001ULL >> x.second) & 1)"),
    _on_time_lines(
        "automation_id_8", "x.second == 0 && ((0x82U >> x.day_of_week) & 1)"
    ),
]


def test_on_time_generated_code(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Each on_time automation generates its schedule check, in config order."""
    main_cpp = generate_main(component_config_path("on_time_fields.yaml"))
    position = 0
    for expected in EXPECTED_ON_TIME_CODE:
        found = main_cpp.find(expected, position)
        assert found != -1, f"missing or out of order:\n{expected}"
        position = found + len(expected)
    assert main_cpp.count("sntp_time->start_on_time_checks();") == 1
    assert main_cpp.find("sntp_time->start_on_time_checks();", position) != -1


@pytest.mark.parametrize(
    ("field", "values", "check"),
    [
        (SECONDS_FIELD, list(range(61)), None),
        (DAYS_OF_WEEK_FIELD, list(range(1, 8)), None),
        (SECONDS_FIELD, [], "false"),
        (MINUTES_FIELD, [30], "x.minute == 30"),
        (DAYS_OF_MONTH_FIELD, [31], "x.day_of_month == 31"),
        # a 0-60 field takes a 64-bit mask even when every listed value is below 32
        (SECONDS_FIELD, [0, 1], "((0x3ULL >> x.second) & 1)"),
        (MINUTES_FIELD, [0, 59], "((0x800000000000001ULL >> x.minute) & 1)"),
        (HOURS_FIELD, [8, 17], "((0x20100U >> x.hour) & 1)"),
        (DAYS_OF_MONTH_FIELD, [1, 31], "((0x80000002U >> x.day_of_month) & 1)"),
        (MONTHS_FIELD, [1, 7], "((0x82U >> x.month) & 1)"),
        (DAYS_OF_WEEK_FIELD, [2, 3, 4, 5, 6], "((0x7cU >> x.day_of_week) & 1)"),
    ],
)
def test_cron_field_check(
    field: CronField, values: list[int], check: str | None
) -> None:
    assert cron_field_check(field, values) == check


def test_empty_field_validates_and_generates(
    set_core_config: SetCoreConfigCallable,
) -> None:
    set_core_config(PlatformFramework.ESP32_IDF)
    config = TIME_SCHEMA(
        {CONF_TIMEZONE: "UTC0", CONF_ON_TIME: [{CONF_SECONDS: [], CONF_THEN: []}]}
    )
    assert on_time_check(config[CONF_ON_TIME][0]) == "false"


def test_list_values_are_deduplicated_ints(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """``true``/``false`` in a list have always meant 1/0; the check must be valid C++."""
    set_core_config(PlatformFramework.ESP32_IDF)
    config = TIME_SCHEMA(
        {
            CONF_TIMEZONE: "UTC0",
            CONF_ON_TIME: [{CONF_SECONDS: [True, False, True], CONF_THEN: []}],
        }
    )
    seconds = config[CONF_ON_TIME][0][CONF_SECONDS]
    assert seconds == [0, 1]
    assert all(type(value) is int for value in seconds)


def test_at_dict_values_are_ints(set_core_config: SetCoreConfigCallable) -> None:
    """The dict form of ``at`` takes the same path, so ``hour: true`` is checked as 1."""
    set_core_config(PlatformFramework.ESP32_IDF)
    config = TIME_SCHEMA(
        {
            CONF_TIMEZONE: "UTC0",
            CONF_ON_TIME: [
                {
                    CONF_AT: {CONF_HOUR: True, CONF_MINUTE: 0, CONF_SECOND: 0},
                    CONF_THEN: [],
                }
            ],
        }
    )
    assert on_time_check(config[CONF_ON_TIME][0]) == (
        "x.second == 0 && x.minute == 0 && x.hour == 1"
    )


def test_every_second_schedule_has_no_check(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """A schedule covering every field's full range fires every second, with no check."""
    set_core_config(PlatformFramework.ESP32_IDF)
    config = TIME_SCHEMA(
        {
            CONF_TIMEZONE: "UTC0",
            CONF_ON_TIME: [{CONF_CRON: "* * * * * *", CONF_THEN: []}],
        }
    )
    assert on_time_check(config[CONF_ON_TIME][0]) is None
