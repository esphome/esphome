"""Needle-angle detection for analog gauge dials."""

from collections.abc import Iterable

import esphome.codegen as cg
from esphome.types import ConfigType

CODEOWNERS = ["@nliaudat"]
DOMAIN = "gauge_needle_detector"

gauge_needle_detector_ns = cg.esphome_ns.namespace("gauge_needle_detector")
NeedleDetector = gauge_needle_detector_ns.class_("NeedleDetector")
Detector = gauge_needle_detector_ns.class_("Detector")
Preprocessor = gauge_needle_detector_ns.class_("Preprocessor")

# Algorithm name to the define that compiles that detector in.
ALGORITHM_DEFINES: dict[str, str] = {
    "legacy": "USE_GAUGE_NEEDLE_INTENSITY_SCAN",
    "radial_profile": "USE_GAUGE_NEEDLE_RADIAL_PROFILE",
    "hough_transform": "USE_GAUGE_NEEDLE_HOUGH_TRANSFORM",
    "template_match": "USE_GAUGE_NEEDLE_TEMPLATE_MATCH",
}


async def to_code(config: ConfigType) -> None:
    cg.add_define("USE_GAUGE_NEEDLE_DETECTOR")


def enable_detectors(algorithms: Iterable[str]) -> None:
    """Compile in the given detectors; "auto" enables all of them.

    The caller must also list this component in AUTO_LOAD, which copies its sources.
    """
    requested = set(algorithms)
    if unknown := requested - set(ALGORITHM_DEFINES) - {"auto"}:
        raise ValueError(
            f"Unknown gauge needle algorithm: {', '.join(sorted(unknown))}"
        )
    enabled = set(ALGORITHM_DEFINES) if "auto" in requested else requested
    cg.add_define("USE_GAUGE_NEEDLE_DETECTOR")
    for algorithm in enabled:
        cg.add_define(ALGORITHM_DEFINES[algorithm])
    # The filtered detectors share the preprocessor; the raw scan does not need it.
    if enabled - {"legacy"}:
        cg.add_define("USE_GAUGE_NEEDLE_PREPROCESSOR")
