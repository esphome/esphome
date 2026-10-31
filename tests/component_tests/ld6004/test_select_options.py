"""The select options keep the order the C++ command tables index.

tests/components/ld6004/select/ld6004_select_test.cpp pins the same order on the C++
side, with the control code each option sends, so a reorder on either side fails a test.
"""

from esphome.components.ld6004.select import P20_MODE_OPTIONS, WORK_MODE_OPTIONS


def test_work_mode_options() -> None:
    assert WORK_MODE_OPTIONS == (
        "normal",
        "low_power",
        "radar_off_p20_high",
        "radar_off_p20_low",
        "high_reflectivity",
    )


def test_p20_mode_options() -> None:
    assert P20_MODE_OPTIONS == (
        "presence_high",
        "presence_low",
        "constant_low",
        "constant_high",
        "pulse_low",
        "pulse_high",
    )
