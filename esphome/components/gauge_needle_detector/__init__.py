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
    """Compile in the given detectors; "auto" enables all of them."""
    # Also set here, so a consumer that calls this from its own to_code() gets
    # the component even when it does not list it in AUTO_LOAD.
    cg.add_define("USE_GAUGE_NEEDLE_DETECTOR")
    for algorithm in algorithms:
        if algorithm == "auto":
            for define in ALGORITHM_DEFINES.values():
                cg.add_define(define)
        elif (define := ALGORITHM_DEFINES.get(algorithm)) is not None:
            cg.add_define(define)
        else:
            raise ValueError(f"Unknown gauge needle algorithm: {algorithm}")
