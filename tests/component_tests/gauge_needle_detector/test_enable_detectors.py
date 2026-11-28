"""enable_detectors() compiles in only the requested detectors."""

from collections.abc import Iterable

import pytest

from esphome.components import gauge_needle_detector
from esphome.core import CORE

DEFINES = gauge_needle_detector.ALGORITHM_DEFINES
PREPROCESSOR = "USE_GAUGE_NEEDLE_PREPROCESSOR"


def _enabled() -> set[str]:
    return {
        define.name
        for define in CORE.defines
        if define.name.startswith("USE_GAUGE_NEEDLE_")
    }


@pytest.mark.parametrize(
    ("algorithms", "expected"),
    [
        (["legacy"], {DEFINES["legacy"]}),
        (["radial_profile"], {DEFINES["radial_profile"], PREPROCESSOR}),
        (["auto"], {*DEFINES.values(), PREPROCESSOR}),
        (
            (name for name in ["radial_profile"]),
            {DEFINES["radial_profile"], PREPROCESSOR},
        ),
    ],
    ids=["legacy_only", "single_filtered", "auto", "one_shot_iterable"],
)
def test_enabled_defines(algorithms: Iterable[str], expected: set[str]) -> None:
    gauge_needle_detector.enable_detectors(algorithms)
    assert _enabled() == expected | {"USE_GAUGE_NEEDLE_DETECTOR"}


@pytest.mark.parametrize("algorithms", [["radial_profle"], ["auto", "radial_profle"]])
def test_unknown_algorithm_is_rejected(algorithms: list[str]) -> None:
    with pytest.raises(ValueError, match="radial_profle"):
        gauge_needle_detector.enable_detectors(algorithms)
    assert not _enabled()
