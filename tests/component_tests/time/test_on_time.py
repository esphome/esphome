"""on_time: the values each schedule key accepts and the code generated for them."""

from collections.abc import Callable
from pathlib import Path
from typing import Any

import pytest

from esphome.components.time import CRON_KEYS, TIME_SCHEMA
import esphome.config_validation as cv
from esphome.const import (
    CONF_AT,
    CONF_CRON,
    CONF_DAYS_OF_MONTH,
    CONF_DAYS_OF_WEEK,
    CONF_HOURS,
    CONF_MINUTES,
    CONF_MONTHS,
    CONF_ON_TIME,
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
    "entry",
    [
        {CONF_CRON: "0 30 7 * *"},
        {CONF_CRON: "0 30 7 * * MON", CONF_SECONDS: 0},
        {CONF_AT: "07:30", CONF_HOURS: 7},
        {CONF_AT: "07:30", CONF_CRON: "0 30 7 * * *"},
        {CONF_SECONDS: 61},
        {CONF_DAYS_OF_MONTH: 0},
        {CONF_MONTHS: "FOO"},
        {},
    ],
)
def test_on_time_schedule_rejected(
    entry: dict[str, Any], set_core_config: SetCoreConfigCallable
) -> None:
    set_core_config(PlatformFramework.ESP32_IDF)
    with pytest.raises(cv.Invalid):
        TIME_SCHEMA({CONF_TIMEZONE: "UTC0", CONF_ON_TIME: [{**entry, CONF_THEN: []}]})


ALL_MINUTES = ", ".join(map(str, range(60)))
ALL_HOURS = ", ".join(map(str, range(24)))
ALL_DOM = ", ".join(map(str, range(1, 32)))
ALL_MONTHS_CPP = ", ".join(map(str, range(1, 13)))


def _cron_lines(
    var: str,
    seconds: str = "0",
    minutes: str = ALL_MINUTES,
    hours: str = ALL_HOURS,
    days_of_month: str = ALL_DOM,
    months: str = ALL_MONTHS_CPP,
    days_of_week: str = "1, 2, 3, 4, 5, 6, 7",
) -> str:
    return "\n".join(
        [
            f"new({var}) time::CronTrigger(sntp_time);",
            f"{var}->add_seconds({{{seconds}}});",
            f"{var}->add_minutes({{{minutes}}});",
            f"{var}->add_hours({{{hours}}});",
            f"{var}->add_days_of_month({{{days_of_month}}});",
            f"{var}->add_months({{{months}}});",
            f"{var}->add_days_of_week({{{days_of_week}}});",
        ]
    )


# One entry per on_time automation in config/on_time_fields.yaml, in order.
EXPECTED_ON_TIME_CODE = [
    _cron_lines(
        "time_crontrigger_id", minutes="0, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50, 55"
    ),
    _cron_lines(
        "time_crontrigger_id_2", minutes="30", hours="7", days_of_week="2, 3, 4, 5, 6"
    ),
    _cron_lines("time_crontrigger_id_3", minutes="30", hours="7"),
    _cron_lines(
        "time_crontrigger_id_4",
        minutes="0",
        hours="8, 9, 10, 11, 12, 13, 14, 15, 16, 17",
    ),
    _cron_lines(
        "time_crontrigger_id_5",
        minutes="0",
        hours="12",
        days_of_month="1, 15",
        months="1, 7",
    ),
    _cron_lines(
        "time_crontrigger_id_6",
        minutes="0",
        hours="0",
        days_of_month="1, 8, 15, 22, 29",
    ),
    _cron_lines("time_crontrigger_id_7", seconds="0, 15, 30, 45, 60"),
    _cron_lines("time_crontrigger_id_8", days_of_week="1, 7"),
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
