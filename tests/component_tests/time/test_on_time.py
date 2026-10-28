"""on_time: the values each schedule key accepts and the code generated for them."""

from collections.abc import Callable
from pathlib import Path
import random
import re
from types import SimpleNamespace
from typing import Any

import pytest

from esphome.components.time import TIME_SCHEMA, cron_field_check
import esphome.config_validation as cv
from esphome.const import (
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

FIELDS = (
    CONF_SECONDS,
    CONF_MINUTES,
    CONF_HOURS,
    CONF_DAYS_OF_MONTH,
    CONF_MONTHS,
    CONF_DAYS_OF_WEEK,
)
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
            {"cron": "0 30 7 * * MON-FRI"},
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
            {"cron": "*/10 0 12 1,15 JAN,JUL ?"},
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
            {"at": "07:30"},
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
            {"at": "23:59:15"},
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
    assert {key: conf.get(key) for key in FIELDS} == {
        key: expected.get(key) for key in FIELDS
    }


@pytest.mark.parametrize(
    "entry",
    [
        {"cron": "0 30 7 * *"},
        {"cron": "0 30 7 * * MON", CONF_SECONDS: 0},
        {"at": "07:30", CONF_HOURS: 7},
        {"at": "07:30", "cron": "0 30 7 * * *"},
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
    _on_time_lines("automation_id", "x.second == 0 && x.minute % 5 == 0"),
    _on_time_lines(
        "automation_id_2",
        "x.second == 0 && x.minute == 30 && x.hour == 7"
        " && x.day_of_week >= 2 && x.day_of_week <= 6",
    ),
    _on_time_lines("automation_id_3", "x.second == 0 && x.minute == 30 && x.hour == 7"),
    _on_time_lines(
        "automation_id_4",
        "x.second == 0 && x.minute == 0 && x.hour >= 8 && x.hour <= 17",
    ),
    _on_time_lines(
        "automation_id_5",
        "x.second == 0 && x.minute == 0 && x.hour == 12"
        " && ((0x8002U >> x.day_of_month) & 1) && (x.month - 1) % 6 == 0",
    ),
    _on_time_lines(
        "automation_id_6",
        "x.second == 0 && x.minute == 0 && x.hour == 0 && (x.day_of_month - 1) % 7 == 0",
    ),
    _on_time_lines("automation_id_7", "x.second % 15 == 0"),
    _on_time_lines("automation_id_8", "x.second == 0 && (x.day_of_week - 1) % 6 == 0"),
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


FIELD_RANGES = [
    ("second", 0, 60),
    ("minute", 0, 59),
    ("hour", 0, 23),
    ("day_of_month", 1, 31),
    ("month", 1, 12),
    ("day_of_week", 1, 7),
]


def _matches(check: str, member: str, value: int) -> bool:
    # Python reads these checks like C++: the only bitwise "&" is inside parentheses.
    python = re.sub(r"(0x[0-9a-f]+)U(LL)?", r"\1", check).replace("&&", "and")
    return bool(eval(python, {"x": SimpleNamespace(**{member: value})}))  # noqa: S307


def _value_sets(low: int, high: int) -> list[list[int]]:
    span = list(range(low, high + 1))
    if len(span) <= 12:
        # every non-empty subset of months and days of the week
        return [
            [v for i, v in enumerate(span) if mask >> i & 1]
            for mask in range(1, 1 << len(span))
        ]
    rng = random.Random(f"{low}-{high}")
    sets = [span, [low], [high], span[1:], span[:-1], span[3:9]]
    sets += [list(range(low, high + 1, step)) for step in range(2, 20)]
    sets += [list(range(low + 1, high + 1, step)) for step in range(2, 20)]
    sets += [sorted(rng.sample(span, rng.randint(1, len(span)))) for _ in range(300)]
    return sets


@pytest.mark.parametrize(("member", "low", "high"), FIELD_RANGES)
def test_cron_field_check_matches_exactly_the_values(
    member: str, low: int, high: int
) -> None:
    """Over the field's whole range, the generated check is true exactly for the listed values."""
    for values in _value_sets(low, high):
        check = cron_field_check(member, values, low, high)
        if check is None:
            assert values == list(range(low, high + 1))
            continue
        matched = [v for v in range(low, high + 1) if _matches(check, member, v)]
        assert matched == values, check
