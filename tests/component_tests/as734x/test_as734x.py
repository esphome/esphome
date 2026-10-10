"""Tests for the as734x band to channel index mapping.

The C++ drivers publish each band from a fixed slot of the channel array, so the order of
BANDS_41 and BANDS_43 in the Python schema is a contract with the readout code. Reordering
either side keeps every YAML test passing and only moves the readings to the wrong entity,
which no bench test would notice, so the indices are pinned here.
"""

import pytest

CONFIG = "tests/component_tests/as734x/test_as734x.yaml"

# Index of every AS7341 band, matching values[] in as7341.cpp read_channels().
AS7341_CHANNELS = (
    ("f1", 0),
    ("f2", 1),
    ("f3", 2),
    ("f4", 3),
    ("f5", 4),
    ("f6", 5),
    ("f7", 6),
    ("f8", 7),
    ("nir", 8),
    ("clear", 9),
)

# Index of every AS7343 band, matching SMUX_CHANNEL_MAP in as7343.cpp.
AS7343_CHANNELS = (
    ("f1", 0),
    ("f2", 1),
    ("fz", 2),
    ("f3", 3),
    ("f4", 4),
    ("fy", 5),
    ("f5", 6),
    ("fxl", 7),
    ("f6", 8),
    ("f7", 9),
    ("f8", 10),
    ("nir", 11),
    ("clear", 12),
)


@pytest.mark.parametrize(("band", "channel"), AS7341_CHANNELS)
def test_as7341_band_keeps_its_channel_index(
    generate_main, band: str, channel: int
) -> None:
    main_cpp = generate_main(CONFIG)

    assert f"chip_41->set_counts_sensor(chip_41_{band}, {channel});" in main_cpp


@pytest.mark.parametrize(("band", "channel"), AS7343_CHANNELS)
def test_as7343_band_keeps_its_channel_index(
    generate_main, band: str, channel: int
) -> None:
    main_cpp = generate_main(CONFIG)

    assert f"chip_43->set_counts_sensor(chip_43_{band}, {channel});" in main_cpp
