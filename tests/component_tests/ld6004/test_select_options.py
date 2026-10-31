"""The select options keep the order the C++ command tables index.

This is the only Python-side guard against reordering an option list. The C++ test in
tests/components/ld6004/select keeps its own copy of the names, so it pins the command
each index sends but cannot see a reorder of the Python lists.
"""

import pytest

from esphome.components.ld6004.select import P20_MODE_OPTIONS, WORK_MODE_OPTIONS


@pytest.mark.parametrize(
    ("options", "expected"),
    [
        (
            WORK_MODE_OPTIONS,
            (
                "normal",
                "low_power",
                "radar_off_p20_high",
                "radar_off_p20_low",
                "high_reflectivity",
            ),
        ),
        (
            P20_MODE_OPTIONS,
            (
                "presence_high",
                "presence_low",
                "constant_low",
                "constant_high",
                "pulse_low",
                "pulse_high",
            ),
        ),
    ],
    ids=["work_mode", "p20_mode"],
)
def test_select_options_order(
    options: tuple[str, ...], expected: tuple[str, ...]
) -> None:
    assert options == expected
